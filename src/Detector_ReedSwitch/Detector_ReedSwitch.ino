#include <avr/wdt.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define REED_LOW_PIN    2
#define REED_HIGH_PIN   3
#define REED_SAFE_PIN   4

#define RELAY_LOW_PIN   A0
#define RELAY_HIGH_PIN  A1
#define RELAY_SAFE_PIN  A2

#define ADC_MAX 1023.0

float Vcc = 0.0;

// Reed switch in pull-up configuration:
// - closed contact by magnet -> LOW ~ 0V
// - open contact -> HIGH ~ VCC
// Relay should mirror the reed contact state.
const float REED_ACTIVE_V   = 1.0;
const float REED_RELEASED_V = 2.5;

unsigned long lastLogTime = 0;
const unsigned long logInterval = 500;

bool reedLowState = false;
bool reedHighState = false;
bool reedSafeState = false;

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

int readADCStable(uint8_t pin)
{
    uint32_t sum = 0;
    const uint8_t samples = 8;

    for (uint8_t i = 0; i < samples; i++)
    {
        analogRead(pin);
        delayMicroseconds(20);
        sum += analogRead(pin);
    }

    return (int)(sum / samples);
}

float adcToVoltage(int adcValue)
{
    return (adcValue / ADC_MAX) * Vcc;
}

bool applyHysteresis(float voltage, bool currentState)
{
    // Reed closed -> voltage low (active state)
    if (!currentState && voltage < REED_ACTIVE_V)
        return true;

    // Reed open -> voltage high (inactive state)
    if (currentState && voltage > REED_RELEASED_V)
        return false;

    return currentState;
}

void setup()
{
    wdt_disable();

    Serial.begin(115200);

    pinMode(REED_LOW_PIN, INPUT_PULLUP);
    pinMode(REED_HIGH_PIN, INPUT_PULLUP);
    pinMode(REED_SAFE_PIN, INPUT_PULLUP);

    pinMode(RELAY_LOW_PIN, OUTPUT);
    pinMode(RELAY_HIGH_PIN, OUTPUT);
    pinMode(RELAY_SAFE_PIN, OUTPUT);

    digitalWrite(RELAY_LOW_PIN, LOW);
    digitalWrite(RELAY_HIGH_PIN, LOW);
    digitalWrite(RELAY_SAFE_PIN, LOW);

    delay(100);

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;);
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("ReedSwitch Init..."));
    display.display();
    delay(1000);

    Vcc = readVcc() / 1000.0;

    delay(100);

    float vLow  = adcToVoltage(readADCStable(REED_LOW_PIN));
    float vHigh = adcToVoltage(readADCStable(REED_HIGH_PIN));
    float vSafe = adcToVoltage(readADCStable(REED_SAFE_PIN));

    reedLowState  = applyHysteresis(vLow, false);
    reedHighState = applyHysteresis(vHigh, false);
    reedSafeState = applyHysteresis(vSafe, false);

    digitalWrite(RELAY_LOW_PIN,  reedLowState);
    digitalWrite(RELAY_HIGH_PIN, reedHighState);
    digitalWrite(RELAY_SAFE_PIN, reedSafeState);

    Serial.println("=== REED SWITCH START ===");
    Serial.print("Measured Vcc: ");
    Serial.print(Vcc, 3);
    Serial.println(" V");

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("=== REED START ==="));
    display.print(F("Vcc: "));
    display.print(Vcc, 2);
    display.println(F(" V"));
    display.display();
    delay(2000);

    wdt_enable(WDTO_1S);
}

void loop()
{
    wdt_reset();

    int reedLowRaw  = readADCStable(REED_LOW_PIN);
    int reedHighRaw = readADCStable(REED_HIGH_PIN);
    int reedSafeRaw = readADCStable(REED_SAFE_PIN);

    float reedLowV  = adcToVoltage(reedLowRaw);
    float reedHighV = adcToVoltage(reedHighRaw);
    float reedSafeV = adcToVoltage(reedSafeRaw);

    reedLowState  = applyHysteresis(reedLowV, reedLowState);
    reedHighState = applyHysteresis(reedHighV, reedHighState);
    reedSafeState = applyHysteresis(reedSafeV, reedSafeState);

    digitalWrite(RELAY_LOW_PIN,  reedLowState);
    digitalWrite(RELAY_HIGH_PIN, reedHighState);
    digitalWrite(RELAY_SAFE_PIN, reedSafeState);

    if (millis() - lastLogTime >= logInterval)
    {
        lastLogTime = millis();

        Serial.print("VCC: ");
        Serial.print(Vcc, 3);
        Serial.print(" V | LOW: ");
        Serial.print(reedLowV, 3);
        Serial.print(" V | HIGH: ");
        Serial.print(reedHighV, 3);
        Serial.print(" V | SAFE: ");
        Serial.print(reedSafeV, 3);
        Serial.print(" V | States: ");
        Serial.print(reedLowState ? "1" : "0");
        Serial.print(" ");
        Serial.print(reedHighState ? "1" : "0");
        Serial.print(" ");
        Serial.println(reedSafeState ? "1" : "0");

        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);

        display.println(F("ReedSwitch Monitor"));
        display.println(F("------------------"));

        display.print(F("VCC: "));
        display.print(Vcc, 2);
        display.println(F("V"));

        display.print(F("LOW:  "));
        display.print(reedLowV, 2);
        display.println(F("V"));

        display.print(F("HIGH: "));
        display.print(reedHighV, 2);
        display.println(F("V"));

        display.print(F("SAFE: "));
        display.print(reedSafeV, 2);
        display.println(F("V"));

        display.println(F("------------------"));

        display.print(F("States: "));
        display.print(reedLowState ? "1" : "0");
        display.print(F(" "));
        display.print(reedHighState ? "1" : "0");
        display.print(F(" "));
        display.println(reedSafeState ? "1" : "0");

        display.display();
    }
}
