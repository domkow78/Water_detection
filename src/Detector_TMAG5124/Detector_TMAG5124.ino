#include <avr/wdt.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define SENSOR_LOW_PIN        A3
#define SENSOR_HIGH_PIN       A6
#define SENSOR_SAFE_PIN       A7

#define RELAY_LOW_PIN        A0
#define RELAY_HIGH_PIN       A1
#define RELAY_SAFE_PIN       A2

#define ADC_MAX 1023.0
#define RSENSE_OHMS 220.0
#define SENSOR_CURRENT_NO_MAGNET_MA 14.5
#define SENSOR_CURRENT_WITH_MAGNET_MA 3.5

float Vcc = 0.0;

// TMAG5124B is a current source, not a voltage source.
// With 220 Ω sense resistor:
// - no magnet: 14.5 mA -> 3.19 V
// - magnet present: 3.5 mA -> 0.77 V
// Magnet detected means lower voltage on the sense resistor.
const float MAGNET_DETECTED_V = 1.5;
const float MAGNET_RELEASED_V = 2.6;

const float SENSOR_OPEN_V = 0.25;       // ~1.1 mA at 220 Ω => probable open circuit / no current
const float SENSOR_OVER_CURRENT_V = 4.2; // ~19 mA at 220 Ω => clearly above normal range

unsigned long lastLogTime = 0;
const unsigned long logInterval = 500;

bool sensorLowState = false;
bool sensorHighState = false;
bool sensorSafeState = false;

enum SensorDiag {
    DIAG_OK,
    DIAG_OPEN,
    DIAG_OVER_CURRENT
};

const char* diagToString(SensorDiag diag)
{
    switch (diag)
    {
        case DIAG_OPEN:
            return "OPEN";
        case DIAG_OVER_CURRENT:
            return "OVR";
        case DIAG_OK:
        default:
            return "OK";
    }
}

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
    // Magnet present => lower voltage on the sense resistor.
    if (!currentState && voltage < MAGNET_DETECTED_V)
        return true;

    if (currentState && voltage > MAGNET_RELEASED_V)
        return false;

    return currentState;
}

SensorDiag getSensorDiag(float voltage)
{
    if (voltage < SENSOR_OPEN_V)
        return DIAG_OPEN;

    if (voltage > SENSOR_OVER_CURRENT_V)
        return DIAG_OVER_CURRENT;

    return DIAG_OK;
}

void setup()
{
    wdt_disable();

    Serial.begin(115200);

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
    display.println(F("TMAG5124 Init..."));
    display.display();
    delay(1000);

    Vcc = readVcc() / 1000.0;

    delay(100);

    float vLow  = adcToVoltage(readADCStable(SENSOR_LOW_PIN));
    float vHigh = adcToVoltage(readADCStable(SENSOR_HIGH_PIN));
    float vSafe = adcToVoltage(readADCStable(SENSOR_SAFE_PIN));

    sensorLowState  = (vLow  < MAGNET_DETECTED_V);
    sensorHighState = (vHigh < MAGNET_DETECTED_V);
    sensorSafeState = (vSafe < MAGNET_DETECTED_V);

    digitalWrite(RELAY_LOW_PIN,  sensorLowState);
    digitalWrite(RELAY_HIGH_PIN, sensorHighState);
    digitalWrite(RELAY_SAFE_PIN, sensorSafeState);

    Serial.println("=== TMAG5124 SYSTEM START ===");
    Serial.print("Measured Vcc: ");
    Serial.print(Vcc, 3);
    Serial.println(" V");

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("=== TMAG5124 START ==="));
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

    int sensorLowRaw  = readADCStable(SENSOR_LOW_PIN);
    int sensorHighRaw = readADCStable(SENSOR_HIGH_PIN);
    int sensorSafeRaw = readADCStable(SENSOR_SAFE_PIN);

    float sensorLowV  = adcToVoltage(sensorLowRaw);
    float sensorHighV = adcToVoltage(sensorHighRaw);
    float sensorSafeV = adcToVoltage(sensorSafeRaw);

    SensorDiag sensorLowDiag  = getSensorDiag(sensorLowV);
    SensorDiag sensorHighDiag = getSensorDiag(sensorHighV);
    SensorDiag sensorSafeDiag = getSensorDiag(sensorSafeV);

    if (sensorLowDiag == DIAG_OPEN || sensorLowDiag == DIAG_OVER_CURRENT)
        sensorLowState = false;
    else
        sensorLowState = applyHysteresis(sensorLowV, sensorLowState);

    if (sensorHighDiag == DIAG_OPEN || sensorHighDiag == DIAG_OVER_CURRENT)
        sensorHighState = false;
    else
        sensorHighState = applyHysteresis(sensorHighV, sensorHighState);

    if (sensorSafeDiag == DIAG_OPEN || sensorSafeDiag == DIAG_OVER_CURRENT)
        sensorSafeState = false;
    else
        sensorSafeState = applyHysteresis(sensorSafeV, sensorSafeState);

    digitalWrite(RELAY_LOW_PIN,  sensorLowState);
    digitalWrite(RELAY_HIGH_PIN, sensorHighState);
    digitalWrite(RELAY_SAFE_PIN, sensorSafeState);

    if (millis() - lastLogTime >= logInterval)
    {
        lastLogTime = millis();

        Serial.print("VCC: ");
        Serial.print(Vcc, 3);
        Serial.print(" V | LOW: ");
        Serial.print(sensorLowV, 3);
        Serial.print(" V [");
        Serial.print(diagToString(sensorLowDiag));
        Serial.print("] | HIGH: ");
        Serial.print(sensorHighV, 3);
        Serial.print(" V [");
        Serial.print(diagToString(sensorHighDiag));
        Serial.print("] | SAFE: ");
        Serial.print(sensorSafeV, 3);
        Serial.print(" V [");
        Serial.print(diagToString(sensorSafeDiag));
        Serial.print("] || States: ");
        Serial.print(sensorLowState);
        Serial.print(" ");
        Serial.print(sensorHighState);
        Serial.print(" ");
        Serial.println(sensorSafeState);

        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);

        display.println(F("TMAG5124 Monitor"));
        display.println(F("------------------"));

        display.print(F("VCC: "));
        display.print(Vcc, 2);
        display.println(F("V"));

        display.print(F("LOW:  "));
        display.print(sensorLowV, 2);
        display.print(F("V "));
        display.println(diagToString(sensorLowDiag));

        display.print(F("HIGH: "));
        display.print(sensorHighV, 2);
        display.print(F("V "));
        display.println(diagToString(sensorHighDiag));

        display.print(F("SAFE: "));
        display.print(sensorSafeV, 2);
        display.print(F("V "));
        display.println(diagToString(sensorSafeDiag));

        display.println(F("------------------"));

        display.print(F("States: "));
        display.print(sensorLowState ? "1" : "0");
        display.print(F(" "));
        display.print(sensorHighState ? "1" : "0");
        display.print(F(" "));
        display.println(sensorSafeState ? "1" : "0");

        display.display();
    }
}
