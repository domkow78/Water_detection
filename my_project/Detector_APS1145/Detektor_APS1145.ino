#include <avr/wdt.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Piny czujników Halla (wejścia analogowe)
#define HALL_LOW_PIN        A3
#define HALL_HIGH_PIN       A6
#define HALL_SAFE_PIN       A7

// Piny przekaźników (wyjścia)
#define RELAY_LOW_PIN       A0
#define RELAY_HIGH_PIN      A1
#define RELAY_SAFE_PIN      A2

#define ADC_MAX 1023.0

float Vcc = 0.0;

// Progi histeresy dla czujnika APS1145 (analogowego)
// Czujnik APS1145 daje ~50% VCC bez magnesu, ~20% przy N-pole, ~80% przy S-pole
const float LOW_THRESHOLD_V  = 1.5;   // Próg dolny - poniżej tej wartości przekaźnik wyłącza się
const float HIGH_THRESHOLD_V = 3.5;   // Próg górny - powyżej tej wartości przekaźnik włącza się

unsigned long lastLogTime = 0;
const unsigned long logInterval = 500;

bool hallLowState  = false;
bool hallHighState = false;
bool hallSafeState = false;

// Funkcja pomiaru napięcia zasilania VCC
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

// Funkcja stabilnego odczytu ADC
int readADCStable(uint8_t pin)
{
    analogRead(pin);
    delayMicroseconds(20);
    return analogRead(pin);
}

// Konwersja wartości ADC na napięcie
float adcToVoltage(int adcValue)
{
    return (adcValue / ADC_MAX) * Vcc;
}

// Funkcja histerezy dla czujnika APS1145
// Dla czujnika analogowego: HIGH gdy napięcie > HIGH_THRESHOLD, LOW gdy < LOW_THRESHOLD
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
    display.println(F("APS1145 Init..."));
    display.display();
    delay(1000);

    Vcc = readVcc() / 1000.0;

    delay(100);

    // Inicjalizacja stanów czujników
    float vLow  = adcToVoltage(readADCStable(HALL_LOW_PIN));
    float vHigh = adcToVoltage(readADCStable(HALL_HIGH_PIN));
    float vSafe = adcToVoltage(readADCStable(HALL_SAFE_PIN));

    hallLowState  = (vLow  > HIGH_THRESHOLD_V);
    hallHighState = (vHigh > HIGH_THRESHOLD_V);
    hallSafeState = (vSafe > HIGH_THRESHOLD_V);

    digitalWrite(RELAY_LOW_PIN,  hallLowState);
    digitalWrite(RELAY_HIGH_PIN, hallHighState);
    digitalWrite(RELAY_SAFE_PIN, hallSafeState);

    Serial.println("=== APS1145 SYSTEM START ===");
    Serial.print("Measured Vcc: ");
    Serial.print(Vcc, 3);
    Serial.println(" V");
    Serial.print("Initial voltages - LOW: ");
    Serial.print(vLow, 2);
    Serial.print(" V, HIGH: ");
    Serial.print(vHigh, 2);
    Serial.print(" V, SAFE: ");
    Serial.print(vSafe, 2);
    Serial.println(" V");

    // Wyświetlenie informacji startowej na OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("=== APS1145 START ==="));
    display.print(F("Vcc: "));
    display.print(Vcc, 2);
    display.println(F(" V"));
    display.print(F("L:"));
    display.print(vLow, 1);
    display.print(F(" H:"));
    display.print(vHigh, 1);
    display.print(F(" S:"));
    display.println(vSafe, 1);
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
        display.println(F("APS1145 Monitor"));
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
        display.print(F("Relay: "));
        display.print(hallLowState ? "1" : "0");
        display.print(F(" "));
        display.print(hallHighState ? "1" : "0");
        display.print(F(" "));
        display.println(hallSafeState ? "1" : "0");
        
        display.display();
    }
}
