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

float Vcc = 0.0;

// Relay board is configured as active-low: ON = LOW, OFF/OPEN = HIGH.
const bool RELAY_ACTIVE = LOW;
const bool RELAY_OPEN = HIGH;

// Measured real-world values on this board:
// - no magnet: ~2.56..2.59 V
// - magnet present: ~0.68..0.72 V
// Hysteresis uses a midpoint window to avoid chattering around the transition.
const float MAGNET_DETECTED_V = 1.2;
const float MAGNET_RELEASED_V = 1.8;

// Fault detection thresholds: below 0.25 V = open circuit, above 4.2 V = overcurrent.
const float SENSOR_OPEN_V = 0.25;
const float SENSOR_OVER_CURRENT_V = 4.2;

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

bool isValidSensorVoltage(float voltage)
{
    return voltage >= SENSOR_OPEN_V && voltage <= SENSOR_OVER_CURRENT_V;
}

SensorDiag getSensorDiag(float voltage)
{
    if (voltage < SENSOR_OPEN_V)
        return DIAG_OPEN;

    if (voltage > SENSOR_OVER_CURRENT_V)
        return DIAG_OVER_CURRENT;

    return DIAG_OK;
}

// Clear fault state and keep the relay open if the input is invalid.
void updateRelayState(bool& sensorState, float voltage, SensorDiag diag)
{
    if (diag == DIAG_OPEN || diag == DIAG_OVER_CURRENT)
    {
        sensorState = false;
        return;
    }

    sensorState = applyHysteresis(voltage, sensorState);
}

// The board expects an active-low relay drive.
void setRelayState(uint8_t pin, bool active)
{
    digitalWrite(pin, active ? RELAY_ACTIVE : RELAY_OPEN);
}

const char* bitState(bool state)
{
    return state ? "1" : "0";
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

    sensorLowState  = false;
    sensorHighState = false;
    sensorSafeState = false;

    if (isValidSensorVoltage(vLow))
        sensorLowState = (vLow < MAGNET_DETECTED_V);
    if (isValidSensorVoltage(vHigh))
        sensorHighState = (vHigh < MAGNET_DETECTED_V);
    if (isValidSensorVoltage(vSafe))
        sensorSafeState = (vSafe < MAGNET_DETECTED_V);

    if (!isValidSensorVoltage(vLow))
        sensorLowState = false;
    if (!isValidSensorVoltage(vHigh))
        sensorHighState = false;
    if (!isValidSensorVoltage(vSafe))
        sensorSafeState = false;

    setRelayState(RELAY_LOW_PIN,  sensorLowState);
    setRelayState(RELAY_HIGH_PIN, sensorHighState);
    setRelayState(RELAY_SAFE_PIN, sensorSafeState);

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

    updateRelayState(sensorLowState, sensorLowV, sensorLowDiag);
    updateRelayState(sensorHighState, sensorHighV, sensorHighDiag);
    updateRelayState(sensorSafeState, sensorSafeV, sensorSafeDiag);

    setRelayState(RELAY_LOW_PIN,  sensorLowState);
    setRelayState(RELAY_HIGH_PIN, sensorHighState);
    setRelayState(RELAY_SAFE_PIN, sensorSafeState);

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
        Serial.print("] || LOW/HIGH/SAFE: ");
        Serial.print(bitState(sensorLowState));
        Serial.print("/");
        Serial.print(bitState(sensorHighState));
        Serial.print("/");
        Serial.println(bitState(sensorSafeState));

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

        display.print(F("LOW/HIGH/SAFE: "));
        display.print(bitState(sensorLowState));
        display.print(F("/"));
        display.print(bitState(sensorHighState));
        display.print(F("/"));
        display.println(bitState(sensorSafeState));

        display.display();
    }
}
