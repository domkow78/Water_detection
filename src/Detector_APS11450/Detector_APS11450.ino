/*
 * ============================================================================
 *  Detektor poziomu wody - APS11450
 * ============================================================================
 *
 *  Sterownik poziomu wody oparty o trzy czujniki Halla APS11450 oraz plywak
 *  z magnesem. Dla sterownika zewnetrznego uklad zachowuje sie identycznie
 *  jak trzy elektrody zanurzeniowe (E1-E3).
 *
 *  Architektura (Soft_Req_Spec_APS11450.md, sekcja 2):
 *      APS11450 x3 -> Sensor Manager -> Level Resolver -> Relay Manager
 *
 *      - Sensor Manager : tylko sprzet (odczyt LOW/HIGH/SAFETY, ADC,
 *                         Internal/External Fault). Nie interpretuje poziomu.
 *      - Level Resolver : jedyny modul z pamiecia stanu. Filtruje stany
 *                         przejsciowe, wykrywa przeskoki, diagnostyka logiczna.
 *      - Relay Manager  : mapuje poziom (E1-E3) na przekazniki.
 *
 *  Zrodla:
 *      - Soft_Req_Spec_APS11450.md (architektura, poziomy, automat, tabela
 *        przejsc sekcja 9, niedozwolone kombinacje sekcja 10)
 *      - APS11450.md (pasma napiec sekcja 6, diagnostyka sekcja 7)
 *      - Detektor_APS1145.ino (wzorzec sprzetowy: readVcc/readADCStable/OLED)
 *
 *  UWAGA: plik budowany przyrostowo wg How_to_work.md (kroki K1..K12).
 *         Aktualny krok wskazuje marker ">>> RESUME HERE <<<" w How_to_work.md.
 * ============================================================================
 */

#include <avr/wdt.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------------------------------------------------------------------------
// Wyswietlacz OLED SSD1306 128x64, I2C, adres 0x3C
// ---------------------------------------------------------------------------
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define OLED_RESET          -1
#define SCREEN_ADDRESS      0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------------------------------------------------------------------------
// Piny czujnikow Halla APS11450 (wejscia analogowe, diagnostyka przez ADC)
// ---------------------------------------------------------------------------
#define HALL_LOW_PIN        A3
#define HALL_HIGH_PIN       A6
#define HALL_SAFE_PIN       A7

// ---------------------------------------------------------------------------
// Piny przekaznikow (wyjscia) - odpowiadaja wirtualnym elektrodom E1-E3
// ---------------------------------------------------------------------------
#define RELAY_LOW_PIN       A0      // E1 / R1
#define RELAY_HIGH_PIN      A1      // E2 / R2
#define RELAY_SAFE_PIN      A2      // E3 / R3

// ---------------------------------------------------------------------------
// Parametry ADC / zasilania
// ---------------------------------------------------------------------------
#define ADC_MAX             1023.0
#define SERIAL_BAUD         115200

float Vcc = 0.0;                    // zmierzone napiecie zasilania [V] (VPU)

// ---------------------------------------------------------------------------
// Klasyfikacja napiecia APS11450 - pasma jako ulamek VPU (=Vcc)
// (APS11450.md sekcja 6)
//
//   0 - 10%  -> External Fault (usterka zewnetrzna)
//  10 - 30%  -> poprawny LOW
//  30 - 70%  -> External Fault (usterka zewnetrzna)
//  70 - 90%  -> poprawny HIGH
//  90 -100%  -> Internal Fault (VOUT = VPU)
// ---------------------------------------------------------------------------
const float VFRAC_EXTLOW_MAX = 0.10f;   // granica 0-10%
const float VFRAC_LOW_MAX    = 0.30f;   // granica LOW  10-30%
const float VFRAC_EXTMID_MAX = 0.70f;   // granica usterki 30-70%
const float VFRAC_HIGH_MAX   = 0.90f;   // granica HIGH 70-90%
                                        // powyzej 0.90 -> Internal Fault

// ---------------------------------------------------------------------------
// Stale czasowe (How_to_work.md sekcja 4.5)
// ---------------------------------------------------------------------------
const unsigned long STABLE_MS            = 1000;   // czas stabilizacji poziomu
const unsigned long INIT_SAMPLE_MS       = 1500;   // okno probkowania po restarcie
const unsigned long TRANSIENT_TIMEOUT_MS = 5000;   // maks. czas stanu przejsciowego
const unsigned long logInterval          = 500;    // okres logowania Serial/OLED

// ---------------------------------------------------------------------------
// Polityka bezpieczenstwa w stanie FAULT (Soft_Req_Spec sekcja 3)
// FAULT_RELAYS = wszystkie przekazniki ON (stan alarmowy - urzadzenie zewnetrzne
// musi zareagowac na ten stan i wyrzucic blad)
// ---------------------------------------------------------------------------
#define FAULT_RELAY_LOW     HIGH
#define FAULT_RELAY_HIGH    HIGH
#define FAULT_RELAY_SAFE    HIGH

// ===========================================================================
//  DEFINICJE TYPÓW (muszą być PRZED pierwszą funkcją)
//  Arduino IDE automatycznie generuje prototypy funkcji. Jeśli typy
//  (enum/struct) są zadeklarowane niżej niż pierwszy nagłówek funkcji,
//  kompilator zgłasza błędy "does not name a type".
// ===========================================================================

// K2. Klasyfikacja sygnalu APS11450 (SignalClass)
enum SignalClass {
    SIG_LOW,        // poprawny LOW  (10-30% VPU)
    SIG_HIGH,       // poprawny HIGH (70-90% VPU)
    SIG_EXT_FAULT,  // usterka zewnetrzna (0-10% lub 30-70% VPU)
    SIG_INT_FAULT   // usterka wewnetrzna (90-100% VPU, VOUT = VPU)
};

// K3. Wynik warstwy sprzetowej (Sensor Manager)
struct SensorState {
    // Kontrakt wg Soft_Req_Spec_APS11450.md sekcja 2:
    bool low;           // aktywny HIGH na czujniku LOW
    bool high;          // aktywny HIGH na czujniku HIGH
    bool safety;        // aktywny HIGH na czujniku SAFETY
    bool lowFault;      // usterka (internal || external) czujnika LOW
    bool highFault;     // usterka czujnika HIGH
    bool safetyFault;   // usterka czujnika SAFETY
    // Rozszerzenie diagnostyczne - rozroznienie internal/external
    // potrzebne do kodow E10-E12 (internal) oraz E20-E22 (external):
    SignalClass lowClass;
    SignalClass highClass;
    SignalClass safetyClass;
};

// K6. Model poziomu
enum Level {
    LVL_UNKNOWN,
    LVL_EMPTY,
    LVL_LEVEL1,
    LVL_LEVEL2,
    LVL_LEVEL3,
    LVL_FAULT
};

// K6. Kody diagnostyczne
enum DiagCode {
    E00 = 0,    // brak bledu
    E10 = 10,   // Internal Fault: low
    E11 = 11,   // Internal Fault: high
    E12 = 12,   // Internal Fault: safe
    E20 = 20,   // External Fault: low
    E21 = 21,   // External Fault: high
    E22 = 22,   // External Fault: safe
    E30 = 30,   // kombinacja niedozwolona (niespojne stany)
    E31 = 31,   // przeskok poziomu
    E32 = 32,   // zbyt dlugi stan przejsciowy (timeout)
    E33 = 33,   // blad sekwencji
    E34 = 34    // utrata synchronizacji
};

// K7. Wynik resolvera poziomu
struct LevelResult {
    Level    level;   // aktualny (zatwierdzony) poziom systemu
    DiagCode code;    // kod diagnostyczny biezacego cyklu
};

// Klasyfikuje napiecie wyjscia (jako ulamek VPU = Vcc) do klasy sygnalu.
// Granice wg APS11450.md sekcja 6.
SignalClass classifySignal(float vFrac)
{
    if (vFrac < VFRAC_EXTLOW_MAX) return SIG_EXT_FAULT; // 0-10%
    if (vFrac < VFRAC_LOW_MAX)    return SIG_LOW;        // 10-30%
    if (vFrac < VFRAC_EXTMID_MAX) return SIG_EXT_FAULT;  // 30-70%
    if (vFrac < VFRAC_HIGH_MAX)   return SIG_HIGH;       // 70-90%
    return SIG_INT_FAULT;                                // 90-100%
}

// ===========================================================================
//  K4. Helpery sprzetowe (wzorzec z Detektor_APS1145.ino)
// ===========================================================================

// Pomiar napiecia zasilania VCC (bandgap 1.1V), wynik w mV.
long readVcc()
{
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
    delay(2);
    ADCSRA |= _BV(ADSC);
    while (bit_is_set(ADCSRA, ADSC));
    long result = ADC;
    result = 1125300L / result;
    return result;
}

// Stabilny odczyt ADC: odrzucony pierwszy pomiar + krotka pauza.
int readADCStable(uint8_t pin)
{
    analogRead(pin);
    delayMicroseconds(20);
    return analogRead(pin);
}

// Konwersja wartosci ADC na napiecie [V] wzgledem zmierzonego Vcc.
float adcToVoltage(int adcValue)
{
    return (adcValue / ADC_MAX) * Vcc;
}

// ===========================================================================
//  K5. Sensor Manager - readSensors()
//  Odczytuje trzy czujniki i wypelnia SensorState. Nie interpretuje poziomu.
//  active  = SIG_HIGH
//  fault   = SIG_INT_FAULT || SIG_EXT_FAULT
//  Klasa jest wyznaczana z ulamka VPU = ADC / 1023 (referencja ADC = Vcc).
// ===========================================================================

// Odczyt pojedynczego czujnika -> klasa sygnalu.
SignalClass readSensorClass(uint8_t pin)
{
    float vFrac = readADCStable(pin) / ADC_MAX;
    return classifySignal(vFrac);
}

SensorState readSensors()
{
    SensorState s;

    s.lowClass    = readSensorClass(HALL_LOW_PIN);
    s.highClass   = readSensorClass(HALL_HIGH_PIN);
    s.safetyClass = readSensorClass(HALL_SAFE_PIN);

    s.low    = (s.lowClass    == SIG_HIGH);
    s.high   = (s.highClass   == SIG_HIGH);
    s.safety = (s.safetyClass == SIG_HIGH);

    s.lowFault    = (s.lowClass    == SIG_INT_FAULT || s.lowClass    == SIG_EXT_FAULT);
    s.highFault   = (s.highClass   == SIG_INT_FAULT || s.highClass   == SIG_EXT_FAULT);
    s.safetyFault = (s.safetyClass == SIG_INT_FAULT || s.safetyClass == SIG_EXT_FAULT);

    return s;
}

// ===========================================================================
//  K6. Model poziomu i helpery raw
// ===========================================================================

// Surowe bity raw = (low, high, safety), MSB = low.
// raw = (low<<2) | (high<<1) | safety.
uint8_t rawFromSensors(const SensorState &s)
{
    return (uint8_t)((s.low ? 4 : 0) | (s.high ? 2 : 0) | (s.safety ? 1 : 0));
}

// Czy kombinacja raw jest dozwolona (odpowiada realnemu poziomowi).
// Dozwolone: 000,100,110,111. Niedozwolone: 010,001,101,011 (sekcja 10 spec).
bool isValidRaw(uint8_t raw)
{
    return (raw == 0b000 || raw == 0b100 || raw == 0b110 || raw == 0b111);
}

// Mapuje dozwolony raw na poziom; dla niedozwolonego zwraca LVL_UNKNOWN.
Level levelFromRaw(uint8_t raw)
{
    switch (raw) {
        case 0b000: return LVL_EMPTY;
        case 0b100: return LVL_LEVEL1;
        case 0b110: return LVL_LEVEL2;
        case 0b111: return LVL_LEVEL3;
        default:    return LVL_UNKNOWN;
    }
}

// ===========================================================================
//  K7. Level Resolver - jedyny modul z pamiecia stanu
//  (Soft_Req_Spec sekcje 5,7,9,10). Zwraca poziom + kod diagnostyczny.
// ===========================================================================

// --- Stan wewnetrzny resolvera ---
Level         currentLevel   = LVL_UNKNOWN;  // zatwierdzony poziom
Level         candidateLevel = LVL_UNKNOWN;  // poziom oczekujacy na stabilizacje
unsigned long candidateSince = 0;            // millis() pojawienia sie kandydata
unsigned long transientSince = 0;            // millis() poczatku stanu niedozwolonego (0=brak)

// Indeks poziomu do oceny sasiedztwa (EMPTY=0..LEVEL3=3, inne = -1).
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

// Kod usterki sprzetowej: priorytet Internal (E10-E12) przed External (E20-E22).
DiagCode faultCode(const SensorState &s)
{
    if (s.lowClass    == SIG_INT_FAULT) return E10;
    if (s.highClass   == SIG_INT_FAULT) return E11;
    if (s.safetyClass == SIG_INT_FAULT) return E12;
    if (s.lowClass    == SIG_EXT_FAULT) return E20;
    if (s.highClass   == SIG_EXT_FAULT) return E21;
    if (s.safetyClass == SIG_EXT_FAULT) return E22;
    return E00;
}

// Ustawia stan poczatkowy resolvera (wywolywane po glosowaniu w setup).
void resolverInit(Level initial)
{
    currentLevel   = initial;
    candidateLevel = LVL_UNKNOWN;
    candidateSince = millis();
    transientSince = 0;
}

LevelResult resolveLevel(const SensorState &s)
{
    LevelResult r;
    unsigned long now = millis();

    // 1) Diagnostyka sprzetowa -> FAULT + Alarm (tabela przejsc: dowolny->FAULT).
    if (s.lowFault || s.highFault || s.safetyFault) {
        currentLevel   = LVL_FAULT;
        candidateLevel = LVL_UNKNOWN;
        candidateSince = now;
        transientSince = 0;
        r.level = LVL_FAULT;
        r.code  = faultCode(s);
        return r;
    }

    // Usterka ustapila: FAULT -> UNKNOWN (odzysk), dalej jak po restarcie.
    if (currentLevel == LVL_FAULT) {
        currentLevel   = LVL_UNKNOWN;
        candidateLevel = LVL_UNKNOWN;
        candidateSince = now;
        transientSince = 0;
    }

    uint8_t raw = rawFromSensors(s);

    // 2) Kombinacja niedozwolona -> trzymaj poprzedni poziom (sekcja 10).
    if (!isValidRaw(raw)) {
        if (transientSince == 0) transientSince = now;
        candidateLevel = LVL_UNKNOWN;         // niepewny stan przejsciowy
        r.level = currentLevel;
        r.code  = (now - transientSince >= TRANSIENT_TIMEOUT_MS) ? E32 : E30;
        return r;
    }

    // 3) Raw poprawny.
    transientSince = 0;
    Level observed = levelFromRaw(raw);

    // 3a) Zgodny z aktualnym poziomem -> stabilnie, bez zmiany.
    if (observed == currentLevel) {
        candidateLevel = LVL_UNKNOWN;
        r.level = currentLevel;
        r.code  = E00;
        return r;
    }

    // 3b) Nowy kandydat -> rozpocznij odliczanie stabilizacji.
    if (observed != candidateLevel) {
        candidateLevel = observed;
        candidateSince = now;
        r.level = currentLevel;
        r.code  = E00;
        return r;
    }

    // 3c) Kandydat utrzymuje sie, ale za krotko -> czekaj.
    if (now - candidateSince < STABLE_MS) {
        r.level = currentLevel;
        r.code  = E00;
        return r;
    }

    // 3d) Kandydat stabilny przez STABLE_MS -> zatwierdz.
    DiagCode code = E00;
    if (currentLevel == LVL_UNKNOWN) {
        // Inicjalizacja / odzysk (INIT_COMPLETE): przyjmij dowolny poprawny poziom.
        currentLevel = observed;
    } else {
        int diff = levelIndex(observed) - levelIndex(currentLevel);
        currentLevel = observed;                 // zatwierdzamy nowy poziom
        if (diff != 1 && diff != -1) code = E31;  // przeskok poziomu (nie-sasiedni)
    }
    candidateLevel = LVL_UNKNOWN;
    r.level = currentLevel;
    r.code  = code;
    return r;
}

// ===========================================================================
//  K8. Wirtualne elektrody (Soft_Req_Spec sekcja 4)
//  EMPTY=000, LEVEL1=100, LEVEL2=110, LEVEL3=111.
// ===========================================================================
void electrodesFromLevel(Level lvl, bool &e1, bool &e2, bool &e3)
{
    switch (lvl) {
        case LVL_LEVEL1: e1 = true;  e2 = false; e3 = false; break;
        case LVL_LEVEL2: e1 = true;  e2 = true;  e3 = false; break;
        case LVL_LEVEL3: e1 = true;  e2 = true;  e3 = true;  break;
        case LVL_EMPTY:                                       // 000
        default:         e1 = false; e2 = false; e3 = false; break;
    }
}

// ===========================================================================
//  K9. Relay Manager - mapuje elektrody E1-E3 na przekazniki R1-R3.
//  W stanie FAULT stosuje polityke bezpieczenstwa FAULT_RELAYS.
// ===========================================================================

// Steruje przekaznikami bezposrednio wg elektrod.
void applyElectrodes(bool e1, bool e2, bool e3)
{
    digitalWrite(RELAY_LOW_PIN,  e1 ? HIGH : LOW);  // R1 = E1
    digitalWrite(RELAY_HIGH_PIN, e2 ? HIGH : LOW);  // R2 = E2
    digitalWrite(RELAY_SAFE_PIN, e3 ? HIGH : LOW);  // R3 = E3
}

// Ustawia wyjscia na podstawie poziomu.
void applyLevel(Level lvl)
{
    if (lvl == LVL_FAULT) {
        // Polityka bezpieczenstwa - wszystkie przekazniki ON (stan alarmowy).
        digitalWrite(RELAY_LOW_PIN,  FAULT_RELAY_LOW);
        digitalWrite(RELAY_HIGH_PIN, FAULT_RELAY_HIGH);
        digitalWrite(RELAY_SAFE_PIN, FAULT_RELAY_SAFE);
        return;
    }
    if (lvl == LVL_UNKNOWN) {
        // Poziom nieustalony (init/odzysk) - utrzymaj poprzedni stan wyjsc.
        return;
    }
    bool e1, e2, e3;
    electrodesFromLevel(lvl, e1, e2, e3);
    applyElectrodes(e1, e2, e3);
}

// ===========================================================================
//  Helpery prezentacji (Serial / OLED)
// ===========================================================================

// Krotki symbol klasy sygnalu: L=LOW, H=HIGH, X=External, I=Internal.
char sigChar(SignalClass c)
{
    switch (c) {
        case SIG_LOW:       return 'L';
        case SIG_HIGH:      return 'H';
        case SIG_EXT_FAULT: return 'X';
        case SIG_INT_FAULT: return 'I';
        default:            return '?';
    }
}

// Nazwa poziomu do logu.
const char *levelName(Level lvl)
{
    switch (lvl) {
        case LVL_EMPTY:  return "EMPTY";
        case LVL_LEVEL1: return "LEVEL1";
        case LVL_LEVEL2: return "LEVEL2";
        case LVL_LEVEL3: return "LEVEL3";
        case LVL_FAULT:  return "FAULT";
        default:         return "UNKNOWN";
    }
}

// Formatuje kod diagnostyczny jako "Exx" (buf: min. 4 bajty).
void diagCodeStr(DiagCode code, char *buf)
{
    int v = (int)code;
    buf[0] = 'E';
    buf[1] = '0' + (v / 10) % 10;
    buf[2] = '0' + (v % 10);
    buf[3] = '\0';
}

// Indeks poziomu -> Level (odwrotnosc levelIndex dla EMPTY..LEVEL3).
Level indexToLevel(int idx)
{
    switch (idx) {
        case 1:  return LVL_LEVEL1;
        case 2:  return LVL_LEVEL2;
        case 3:  return LVL_LEVEL3;
        default: return LVL_EMPTY;
    }
}

unsigned long lastLogTime = 0;

// ===========================================================================
//  K10. setup()
// ===========================================================================
void setup()
{
    wdt_disable();
    Serial.begin(SERIAL_BAUD);

    // Piny przekaznikow jako wyjscia; do konca inicjalizacji stan bezpieczny (OFF).
    pinMode(RELAY_LOW_PIN,  OUTPUT);
    pinMode(RELAY_HIGH_PIN, OUTPUT);
    pinMode(RELAY_SAFE_PIN, OUTPUT);
    digitalWrite(RELAY_LOW_PIN,  LOW);
    digitalWrite(RELAY_HIGH_PIN, LOW);
    digitalWrite(RELAY_SAFE_PIN, LOW);

    delay(100);

    // OLED.
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("APS11450 Init..."));
    display.display();

    // Pomiar napiecia zasilania (VPU).
    Vcc = readVcc() / 1000.0;

    // Restart wg sekcji 6: probkowanie przez INIT_SAMPLE_MS + glosowanie
    // wiekszosciowe -> najbardziej prawdopodobny poziom. Probki z usterka
    // lub niedozwolonym raw sa pomijane.
    int votes[4] = { 0, 0, 0, 0 };   // EMPTY, LEVEL1, LEVEL2, LEVEL3
    unsigned long start = millis();
    while (millis() - start < INIT_SAMPLE_MS) {
        SensorState s = readSensors();
        if (!(s.lowFault || s.highFault || s.safetyFault)) {
            uint8_t raw = rawFromSensors(s);
            if (isValidRaw(raw)) {
                int idx = levelIndex(levelFromRaw(raw));
                if (idx >= 0) votes[idx]++;
            }
        }
        delay(20);
    }
    int best = 0;
    for (int i = 1; i < 4; i++) {
        if (votes[i] > votes[best]) best = i;
    }
    Level initLevel = (votes[best] > 0) ? indexToLevel(best) : LVL_EMPTY;

    // Inicjalizacja resolvera + ustawienie elektrod/przekaznikow.
    resolverInit(initLevel);
    applyLevel(initLevel);

    // Log INIT_COMPLETE.
    Serial.println(F("=== APS11450 START ==="));
    Serial.print(F("Vcc: "));
    Serial.print(Vcc, 3);
    Serial.println(F(" V"));
    Serial.print(F("INIT_COMPLETE level: "));
    Serial.println(levelName(initLevel));
    Serial.print(F("Votes E/L1/L2/L3: "));
    Serial.print(votes[0]); Serial.print(' ');
    Serial.print(votes[1]); Serial.print(' ');
    Serial.print(votes[2]); Serial.print(' ');
    Serial.println(votes[3]);

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("=== APS11450 ==="));
    display.print(F("Vcc: "));
    display.print(Vcc, 2);
    display.println(F(" V"));
    display.print(F("Init: "));
    display.println(levelName(initLevel));
    display.println(F("INIT_COMPLETE"));
    display.display();
    delay(1500);

    lastLogTime = millis();
    wdt_enable(WDTO_1S);
}

// ===========================================================================
//  K11. loop()
// ===========================================================================
void loop()
{
    wdt_reset();

    SensorState s   = readSensors();
    LevelResult res = resolveLevel(s);
    applyLevel(res.level);

    if (millis() - lastLogTime >= logInterval) {
        lastLogTime = millis();

        // Napiecia do wyswietlenia (odczyt diagnostyczny).
        float vLow  = adcToVoltage(readADCStable(HALL_LOW_PIN));
        float vHigh = adcToVoltage(readADCStable(HALL_HIGH_PIN));
        float vSafe = adcToVoltage(readADCStable(HALL_SAFE_PIN));

        bool e1, e2, e3;
        electrodesFromLevel(res.level, e1, e2, e3);

        char code[4];
        diagCodeStr(res.code, code);

        uint8_t raw = rawFromSensors(s);

        // --- Serial ---
        Serial.print(F("V(L/H/S): "));
        Serial.print(vLow, 2);  Serial.print('/');
        Serial.print(vHigh, 2); Serial.print('/');
        Serial.print(vSafe, 2);
        Serial.print(F(" | sig: "));
        Serial.print(sigChar(s.lowClass));
        Serial.print(sigChar(s.highClass));
        Serial.print(sigChar(s.safetyClass));
        Serial.print(F(" | raw: "));
        Serial.print((raw >> 2) & 1);
        Serial.print((raw >> 1) & 1);
        Serial.print(raw & 1);
        Serial.print(F(" | level: "));
        Serial.print(levelName(res.level));
        Serial.print(F(" | E: "));
        Serial.print(e1); Serial.print(e2); Serial.print(e3);
        Serial.print(F(" | code: "));
        Serial.println(code);

        // --- OLED ---
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println(F("APS11450 Monitor"));
        display.println(F("----------------"));
        display.print(F("Level: "));
        display.println(levelName(res.level));
        display.print(F("E: "));
        display.print(e1); display.print(' ');
        display.print(e2); display.print(' ');
        display.println(e3);
        display.print(F("Sig: "));
        display.print(sigChar(s.lowClass));
        display.print(sigChar(s.highClass));
        display.println(sigChar(s.safetyClass));
        display.print(F("Code: "));
        display.println(code);
        display.display();
    }
}
