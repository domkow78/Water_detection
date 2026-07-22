/*
 * ============================================================================
 *  Detektor poziomu wody - Reed Switch
 * ============================================================================
 *
 *  Implementacja zgodna z Soft_Req_Spec_ReedSwitch.md:
 *    Reed Switch x3 -> Sensor Manager -> Level Resolver -> Relay Manager
 *
 *  - Sensor Manager: odczyt wejsc + debounce (bez interpretacji poziomu)
 *  - Level Resolver: jedyny modul z pamiecia stanu i filtracja przejsc
 *  - Relay Manager : mapowanie poziomu na wyjscia R1/R2/R3 (E1/E2/E3)
 *
 *  Uwaga: kontaktrony pracuja jako INPUT_PULLUP, czyli stan aktywny = LOW
 *  (zamkniety kontaktron zwiera wejscie do GND).
 * ============================================================================
 */

#include <avr/wdt.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------------------------------------------------------------------------
// OLED SSD1306 128x64, I2C
// ---------------------------------------------------------------------------
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define OLED_RESET          -1
#define SCREEN_ADDRESS      0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------------------------------------------------------------------------
// Wejscia kontaktronow
// ---------------------------------------------------------------------------
#define REED_LOW_PIN        2
#define REED_HIGH_PIN       3
#define REED_SAFE_PIN       4

// Kontaktrony pod INPUT_PULLUP: aktywny gdy LOW
#define REED_ACTIVE_STATE   LOW

// ---------------------------------------------------------------------------
// Wyjscia przekaźnikow (emulacja elektrod E1-E3)
// ---------------------------------------------------------------------------
#define RELAY_LOW_PIN       A0      // E1 / R1
#define RELAY_HIGH_PIN      A1      // E2 / R2
#define RELAY_SAFE_PIN      A2      // E3 / R3

#define SERIAL_BAUD         115200

const unsigned long DEBOUNCE_MS          = 35;
const unsigned long STABLE_MS            = 1000;
const unsigned long INIT_SAMPLE_MS       = 1500;
const unsigned long TRANSIENT_TIMEOUT_MS = 5000;
const unsigned long LOG_INTERVAL_MS      = 500;

// ---------------------------------------------------------------------------
// TABELA MAPOWANIA: raw [0-7] -> elektrody [E1 E2 E3]
// ---------------------------------------------------------------------------
// raw = (low, high, safety) jako bity
// Index:  0    1    2    3    4    5    6    7
// raw:  000  001  010  011  100  101  110  111
//
// Elektrody: bit2=E1, bit1=E2, bit0=E3
// Wartość w tabeli = (E1 << 2) | (E2 << 1) | E3
//
// Domyslnie: raw bity mapowane bezposrednio na elektrody (raw == E)
// EDYTUJ PONIZSZA TABELE ZGODNIE Z WYMAGANIAMI URZADZENIA
// ---------------------------------------------------------------------------
const uint8_t rawToElectrodes[8] = {
	0b000,  // raw=000 -> E=000 (EMPTY)
	0b001,  // raw=001 -> E=001
	0b010,  // raw=010 -> E=010
	0b011,  // raw=011 -> E=011
	0b100,  // raw=100 -> E=100 (LEVEL1)
	0b101,  // raw=101 -> E=101
	0b110,  // raw=110 -> E=110 (LEVEL2)
	0b111   // raw=111 -> E=111 (LEVEL3)
};

enum Level {
	LVL_UNKNOWN,
	LVL_EMPTY,
	LVL_LEVEL1,
	LVL_LEVEL2,
	LVL_LEVEL3
};

enum DiagCode {
	E00 = 0,
	E30 = 30,   // kombinacja niedozwolona
	E31 = 31,   // przeskok poziomu
	E32 = 32    // zbyt dlugi stan przejsciowy
};

struct SensorState {
	bool low;
	bool high;
	bool safety;
};

struct LevelResult {
	Level level;
	DiagCode code;
};

struct DebouncedInput {
	uint8_t pin;
	bool stableRaw;
	bool lastRaw;
	unsigned long lastChangeMs;
};

DebouncedInput reedLow  = {REED_LOW_PIN, HIGH, HIGH, 0};
DebouncedInput reedHigh = {REED_HIGH_PIN, HIGH, HIGH, 0};
DebouncedInput reedSafe = {REED_SAFE_PIN, HIGH, HIGH, 0};

Level currentLevel = LVL_UNKNOWN;
Level candidateLevel = LVL_UNKNOWN;
unsigned long candidateSince = 0;
unsigned long transientSince = 0;

unsigned long lastLogTime = 0;

void initDebouncedInput(DebouncedInput &in)
{
	pinMode(in.pin, INPUT_PULLUP);
	bool raw = digitalRead(in.pin);
	in.stableRaw = raw;
	in.lastRaw = raw;
	in.lastChangeMs = millis();
}

void updateDebouncedInput(DebouncedInput &in)
{
	bool raw = digitalRead(in.pin);
	unsigned long now = millis();

	if (raw != in.lastRaw) {
		in.lastRaw = raw;
		in.lastChangeMs = now;
	}

	if ((now - in.lastChangeMs) >= DEBOUNCE_MS) {
		in.stableRaw = in.lastRaw;
	}
}

bool inputActive(const DebouncedInput &in)
{
	return (in.stableRaw == REED_ACTIVE_STATE);
}

SensorState readSensors()
{
	updateDebouncedInput(reedLow);
	updateDebouncedInput(reedHigh);
	updateDebouncedInput(reedSafe);

	SensorState s;
	s.low = inputActive(reedLow);
	s.high = inputActive(reedHigh);
	s.safety = inputActive(reedSafe);
	return s;
}

uint8_t rawFromSensors(const SensorState &s)
{
	return (uint8_t)((s.low ? 4 : 0) | (s.high ? 2 : 0) | (s.safety ? 1 : 0));
}

bool isValidRaw(uint8_t raw)
{
	return (raw < 8);  // wszystkie stany raw 0-7 sa dopuszczalne
}

Level levelFromRaw(uint8_t raw)
{
	// Przykladowe mapowanie poziomow na podstawie elektrоd
	// (mozna dostosowac do rzeczywistej sekwencji magnesu)
	uint8_t e = rawToElectrodes[raw & 0x07];
	
	switch (e) {
		case 0b000: return LVL_EMPTY;
		case 0b100: return LVL_LEVEL1;
		case 0b110: return LVL_LEVEL2;
		case 0b111: return LVL_LEVEL3;
		default:    return LVL_UNKNOWN;
	}
}

int levelIndex(Level lvl)
{
	switch (lvl) {
		case LVL_EMPTY:  return 0;
		case LVL_LEVEL1: return 1;
		case LVL_LEVEL2: return 2;
		case LVL_LEVEL3: return 3;
		default:         return -1;
	}
}

void resolverInit(Level lvl)
{
	currentLevel = lvl;
	candidateLevel = lvl;
	candidateSince = 0;
	transientSince = 0;
}

LevelResult resolveLevel(const SensorState &s)
{
	unsigned long now = millis();
	uint8_t raw = rawFromSensors(s);

	if (!isValidRaw(raw)) {
		if (transientSince == 0) {
			transientSince = now;
			return {currentLevel, E30};
		}

		if ((now - transientSince) >= TRANSIENT_TIMEOUT_MS) {
			return {currentLevel, E32};
		}

		return {currentLevel, E30};
	}

	transientSince = 0;
	Level observed = levelFromRaw(raw);

	if (currentLevel == LVL_UNKNOWN) {
		currentLevel = observed;
		candidateLevel = observed;
		candidateSince = 0;
		return {currentLevel, E00};
	}

	if (observed == currentLevel) {
		candidateLevel = currentLevel;
		candidateSince = 0;
		return {currentLevel, E00};
	}

	if (candidateLevel != observed) {
		candidateLevel = observed;
		candidateSince = now;
		return {currentLevel, E00};
	}

	if ((now - candidateSince) < STABLE_MS) {
		return {currentLevel, E00};
	}

	int cur = levelIndex(currentLevel);
	int nxt = levelIndex(observed);
	if (cur >= 0 && nxt >= 0 && abs(nxt - cur) == 1) {
		currentLevel = observed;
		return {currentLevel, E00};
	}

	return {currentLevel, E31};
}

void electrodesFromLevel(Level lvl, bool &e1, bool &e2, bool &e3)
{
	// Odczytaj elektrody dla danego poziomu z tabeli mapowania
	uint8_t e = rawToElectrodes[rawFromSensors(readSensors())];
	
	e1 = (e & 0x04) != 0;  // bit 2
	e2 = (e & 0x02) != 0;  // bit 1
	e3 = (e & 0x01) != 0;  // bit 0
}

void applyElectrodes(bool e1, bool e2, bool e3)
{
	digitalWrite(RELAY_LOW_PIN,  e1 ? HIGH : LOW);
	digitalWrite(RELAY_HIGH_PIN, e2 ? HIGH : LOW);
	digitalWrite(RELAY_SAFE_PIN, e3 ? HIGH : LOW);
}

void applyLevel(Level lvl)
{
	bool e1, e2, e3;
	electrodesFromLevel(lvl, e1, e2, e3);
	applyElectrodes(e1, e2, e3);
}

const char *levelName(Level lvl)
{
	switch (lvl) {
		case LVL_UNKNOWN: return "UNKNOWN";
		case LVL_EMPTY:   return "EMPTY";
		case LVL_LEVEL1:  return "LEVEL1";
		case LVL_LEVEL2:  return "LEVEL2";
		case LVL_LEVEL3:  return "LEVEL3";
		default:          return "?";
	}
}

const char *diagCodeStr(DiagCode c)
{
	switch (c) {
		case E00: return "E00";
		case E30: return "E30";
		case E31: return "E31";
		case E32: return "E32";
		default:  return "E??";
	}
}

Level indexToLevel(int idx)
{
	switch (idx) {
		case 0: return LVL_EMPTY;
		case 1: return LVL_LEVEL1;
		case 2: return LVL_LEVEL2;
		case 3: return LVL_LEVEL3;
		default:return LVL_UNKNOWN;
	}
}

void setup()
{
	wdt_disable();

	Serial.begin(SERIAL_BAUD);

	pinMode(RELAY_LOW_PIN, OUTPUT);
	pinMode(RELAY_HIGH_PIN, OUTPUT);
	pinMode(RELAY_SAFE_PIN, OUTPUT);

	digitalWrite(RELAY_LOW_PIN, LOW);
	digitalWrite(RELAY_HIGH_PIN, LOW);
	digitalWrite(RELAY_SAFE_PIN, LOW);

	initDebouncedInput(reedLow);
	initDebouncedInput(reedHigh);
	initDebouncedInput(reedSafe);

	if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
		for (;;);
	}

	display.clearDisplay();
	display.setTextSize(1);
	display.setTextColor(SSD1306_WHITE);
	display.setCursor(0, 0);
	display.println(F("ReedSwitch Init..."));
	display.display();

	int votes[4] = {0, 0, 0, 0};
	unsigned long t0 = millis();
	while ((millis() - t0) < INIT_SAMPLE_MS) {
		SensorState s = readSensors();
		uint8_t raw = rawFromSensors(s);
		if (isValidRaw(raw)) {
			Level lvl = levelFromRaw(raw);
			int idx = levelIndex(lvl);
			if (idx >= 0) votes[idx]++;
		}
		delay(20);
	}

	int bestIdx = 0;
	for (int i = 1; i < 4; i++) {
		if (votes[i] > votes[bestIdx]) bestIdx = i;
	}

	Level initLevel = indexToLevel(bestIdx);
	resolverInit(initLevel);
	applyLevel(initLevel);

	Serial.println(F("=== REED SWITCH SYSTEM START ==="));
	Serial.print(F("INIT_COMPLETE: "));
	Serial.println(levelName(initLevel));

	display.clearDisplay();
	display.setCursor(0, 0);
	display.println(F("REED SWITCH START"));
	display.print(F("INIT: "));
	display.println(levelName(initLevel));
	display.display();

	wdt_enable(WDTO_1S);
}

void loop()
{
	wdt_reset();

	SensorState s = readSensors();
	LevelResult r = resolveLevel(s);
	applyLevel(r.level);

	unsigned long now = millis();
	if ((now - lastLogTime) >= LOG_INTERVAL_MS) {
		lastLogTime = now;

		uint8_t raw = rawFromSensors(s);
		bool e1, e2, e3;
		electrodesFromLevel(r.level, e1, e2, e3);

		Serial.print(F("RAW="));
		Serial.print((raw & 0b100) ? '1' : '0');
		Serial.print((raw & 0b010) ? '1' : '0');
		Serial.print((raw & 0b001) ? '1' : '0');
		Serial.print(F(" | LVL="));
		Serial.print(levelName(r.level));
		Serial.print(F(" | E="));
		Serial.print(e1 ? '1' : '0');
		Serial.print(e2 ? '1' : '0');
		Serial.print(e3 ? '1' : '0');
		Serial.print(F(" | "));
		Serial.println(diagCodeStr(r.code));

		display.clearDisplay();
		display.setCursor(0, 0);
		display.println(F("ReedSwitch Monitor"));
		display.println(F("------------------"));
		display.print(F("RAW: "));
		display.print((raw & 0b100) ? '1' : '0');
		display.print((raw & 0b010) ? '1' : '0');
		display.println((raw & 0b001) ? '1' : '0');
		display.print(F("LVL: "));
		display.println(levelName(r.level));
		display.print(F("E: "));
		display.print(e1 ? '1' : '0');
		display.print(e2 ? '1' : '0');
		display.println(e3 ? '1' : '0');
		display.print(F("CODE: "));
		display.println(diagCodeStr(r.code));
		display.display();
	}
}
