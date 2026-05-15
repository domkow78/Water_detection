#include <avr/wdt.h>

#define HALL_LOW_PIN        A3
#define HALL_HIGH_PIN       A6
#define HALL_SAFE_PIN       A7

#define RELAY_LOW_PIN       A0
#define RELAY_HIGH_PIN      A1
#define RELAY_SAFE_PIN      A2

#define ADC_MAX 1023.0

float Vcc = 0.0;

const float LOW_THRESHOLD_V  = 0.3;
const float HIGH_THRESHOLD_V = 1.5;

unsigned long lastLogTime = 0;
const unsigned long logInterval = 500;

bool hallLowState  = false;
bool hallHighState = false;
bool hallSafeState = false;

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
    analogRead(pin);
    delayMicroseconds(20);
    return analogRead(pin);
}

float adcToVoltage(int adcValue)
{
    return (adcValue / ADC_MAX) * Vcc;
}

bool applyHysteresis(float voltage, bool currentState)
{
    if (!currentState && voltage > HIGH_THRESHOLD_V)
        return true;

    if (currentState && voltage < LOW_THRESHOLD_V)
        return false;

    return currentState;
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

    Vcc = readVcc() / 1000.0;

    delay(100);

    float vLow  = adcToVoltage(readADCStable(HALL_LOW_PIN));
    float vHigh = adcToVoltage(readADCStable(HALL_HIGH_PIN));
    float vSafe = adcToVoltage(readADCStable(HALL_SAFE_PIN));

    hallLowState  = (vLow  > HIGH_THRESHOLD_V);
    hallHighState = (vHigh > HIGH_THRESHOLD_V);
    hallSafeState = (vSafe > HIGH_THRESHOLD_V);

    digitalWrite(RELAY_LOW_PIN,  hallLowState);
    digitalWrite(RELAY_HIGH_PIN, hallHighState);
    digitalWrite(RELAY_SAFE_PIN, hallSafeState);

    Serial.println("=== SYSTEM START ===");
    Serial.print("Measured Vcc: ");
    Serial.print(Vcc, 3);
    Serial.println(" V");

    wdt_enable(WDTO_1S);
}

void loop()
{
    wdt_reset();

    int hallLowRaw  = readADCStable(HALL_LOW_PIN);
    int hallHighRaw = readADCStable(HALL_HIGH_PIN);
    int hallSafeRaw = readADCStable(HALL_SAFE_PIN);

    float hallLowV  = adcToVoltage(hallLowRaw);
    float hallHighV = adcToVoltage(hallHighRaw);
    float hallSafeV = adcToVoltage(hallSafeRaw);

    hallLowState  = applyHysteresis(hallLowV,  hallLowState);
    hallHighState = applyHysteresis(hallHighV, hallHighState);
    hallSafeState = applyHysteresis(hallSafeV, hallSafeState);

    digitalWrite(RELAY_LOW_PIN,  hallLowState);
    digitalWrite(RELAY_HIGH_PIN, hallHighState);
    digitalWrite(RELAY_SAFE_PIN, hallSafeState);

    if (millis() - lastLogTime >= logInterval)
    {
        lastLogTime = millis();

        Serial.print("LOW: ");
        Serial.print(hallLowV, 3);
        Serial.print(" V | HIGH: ");
        Serial.print(hallHighV, 3);
        Serial.print(" V | SAFE: ");
        Serial.print(hallSafeV, 3);
        Serial.print(" V || States: ");
        Serial.print(hallLowState);
        Serial.print(" ");
        Serial.print(hallHighState);
        Serial.print(" ");
        Serial.println(hallSafeState);
    }
}