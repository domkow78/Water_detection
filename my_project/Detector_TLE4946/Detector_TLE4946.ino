#include <avr/wdt.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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

    // Inicjalizacja wyświetlacza OLED
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;);
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Initializing..."));
    display.display();
    delay(1000);

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

    // Wyświetlenie informacji startowej na OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("=== SYSTEM START ==="));
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

        // Wyświetlenie danych na OLED
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);
        
        // Nagłówek
        display.println(F("Hall Sensor Monitor"));
        display.println(F("------------------"));
        
        // Napięcia czujników
        display.print(F("LOW:  "));
        display.print(hallLowV, 2);
        display.println(F(" V"));
        
        display.print(F("HIGH: "));
        display.print(hallHighV, 2);
        display.println(F(" V"));
        
        display.print(F("SAFE: "));
        display.print(hallSafeV, 2);
        display.println(F(" V"));
        
        display.println(F("------------------"));
        
        // Stany przekaźników
        display.print(F("States: "));
        display.print(hallLowState ? "1" : "0");
        display.print(F(" "));
        display.print(hallHighState ? "1" : "0");
        display.print(F(" "));
        display.println(hallSafeState ? "1" : "0");
        
        display.display();
    }
}