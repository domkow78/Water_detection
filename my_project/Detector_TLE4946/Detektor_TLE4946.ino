const float VCC = 4.5;

const int hallPin1 = A0;
const int hallPin2 = A1;
const int hallPin3 = A2;

const int ledPin1 = 2;
const int ledPin2 = 3;
const int ledPin3 = 4;

// Progi procentowe
const float LOW_MAX = 0.10;
const float HIGH_MIN = 0.90;

bool error1 = false;
bool error2 = false;
bool error3 = false;

unsigned long lastBlink = 0;
bool blinkState = false;

void setup() {
  Serial.begin(9600);

  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  pinMode(ledPin3, OUTPUT);

  digitalWrite(ledPin1, HIGH);
  digitalWrite(ledPin2, HIGH);
  digitalWrite(ledPin3, HIGH);
}

void loop() {
  error1 = processSensor(hallPin1, ledPin1, "A0");
  error2 = processSensor(hallPin2, ledPin2, "A1");
  error3 = processSensor(hallPin3, ledPin3, "A2");

  Serial.println();

  // Miganie LED dla błędnych czujników
  unsigned long now = millis();
  if (now - lastBlink > 300) {
    lastBlink = now;
    blinkState = !blinkState;

    if (error1) digitalWrite(ledPin1, blinkState ? LOW : HIGH);
    if (error2) digitalWrite(ledPin2, blinkState ? LOW : HIGH);
    if (error3) digitalWrite(ledPin3, blinkState ? LOW : HIGH);
  }

  delay(100); // mniejszy delay, aby nie blokować pętli
}

bool processSensor(int analogPin, int ledPin, const char* label) {
  int raw = analogRead(analogPin);
  float voltage = (raw / 1023.0) * VCC;
  float ratio = voltage / VCC;

  Serial.print(label);
  Serial.print(": ");
  Serial.print(voltage, 2);
  Serial.print(" V (");

  if (ratio <= LOW_MAX) {
    Serial.print("LOW)");
    digitalWrite(ledPin, LOW);
    return false;
  } else if (ratio >= HIGH_MIN) {
    Serial.print("HIGH)");
    digitalWrite(ledPin, HIGH);
    return false;
  } else {
    Serial.print("ERROR)");
    return true;
  }

  Serial.print("   ");
}
