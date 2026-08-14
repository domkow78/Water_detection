# Opis działania programu dla TLE4946

## 1. Cel programu

Program monitoruje trzy kanały czujników TLE4946 i na ich podstawie steruje trzema przekaźnikami. Każdy kanał jest odczytywany jako napięcie analogowe pochodzące z czujnika Halla, a następnie analizowany przez program z wykorzystaniem progów histerezy.

Głównym celem jest wykrywanie zmiany stanu czujnika w zależności od poziomu pola magnetycznego i utrzymanie stabilnych stanów logicznych w obecności zakłóceń, niewielkich zmian napięcia oraz szumu pomiarowego.

---

## 2. Założenia projektowe

### 2.1. Obwód pomiarowy

Każdy czujnik TLE4946 jest podłączony do wejścia ADC mikrokontrolera:
- HALL_LOW_PIN = A3
- HALL_HIGH_PIN = A6
- HALL_SAFE_PIN = A7

Napięcie z czujnika jest mierzone względem aktualnego napięcia zasilania układu.

### 2.2. Przekaźniki

Sterowanie przekaźnikami odbywa się poprzez wyjścia cyfrowe:
- RELAY_LOW_PIN = A0
- RELAY_HIGH_PIN = A1
- RELAY_SAFE_PIN = A2

Przekaźniki są aktywowane lub wyłączane na podstawie stanu odczytu z odpowiedniego czujnika.

---

## 3. Odczyt z ADC i uśrednianie

Każdy kanał odczytywany jest z wejścia ADC mikrokontrolera. W celu ograniczenia wpływu szumu i chwilowych niestabilności sygnału program wykonuje odczyty stabilizowane.

W praktyce funkcja `readADCStable()` działa w następujący sposób:
1. wykonuje pierwszy pomiar dla danego wejścia
2. odczekuje krótko (20 µs)
3. wykonuje kolejny pomiar
4. zwraca wartość z drugiego odczytu

To pozwala zmniejszyć błędy wynikające z nieustalonego stanu wejścia ADC lub krótkotrwałych zakłóceń.

Po odczycie wartość ADC jest zamieniana na napięcie:

V = (ADC / 1023) × VCC

Dzięki temu napięcia z czujników są skalowane do rzeczywistego napięcia zasilania systemu, a nie przyjmowane na sztywno jako 5 V.

Dodatkowo w tej wersji program wykonuje średniowanie z 8 próbek dla każdego kanału, co zwiększa odporność na drobne fluktuacje sygnału.

---

## 4. Weryfikacja VCC

Program mierzy napięcie zasilania mikrokontrolera w funkcji `readVcc()`. Jest to ważny element, ponieważ sygnał z czujnika musi być przeliczony względem aktualnego VCC.

Wewnętrzny mechanizm AVR ustawia wejście ADC do pracy z referencją wewnętrzną, po czym odczytuje wynik. Na podstawie tego wyniku obliczana jest wartość zasilania w milivoltach.

Równanie zastosowane w programie:

VCC = 1125300 / ADC

Po przeliczeniu otrzymane napięcie jest przekazywane dalej do funkcji konwertującej ADC na napięcie:

V = (ADC / 1023) × VCC

Dzięki temu pomiar ma większą dokładność i jest odporny na zmiany napięcia zasilania.

---

## 5. Logika detekcji i histereza

Po odczycie napięcia z każdego kanału program stosuje funkcję `applyHysteresis()`. Jest to podstawowy mechanizm stabilizacji działania układu.

Progi w tej wersji:
- LOW_THRESHOLD_V = 0.3 V
- HIGH_THRESHOLD_V = 1.5 V

Logika:
- jeżeli stan jest wyłączony i napięcie przekroczy 1,5 V, stan zmienia się na włączony
- jeżeli stan jest włączony i napięcie spadnie poniżej 0,3 V, stan zmienia się na wyłączony
- w pozostałych przypadkach stan pozostaje bez zmian

To zapobiega miganiu stanu przy napięciach zbliżonych do progu i poprawia stabilność pracy.

W praktyce program traktuje czujnik jako aktywny, gdy jego napięcie rośnie powyżej progu aktywacji. Gdy napięcie spadnie poniżej progu wyłączenia, czujnik wraca do stanu nieaktywnego.

To jest typowa logika dla czujnika Halla pracującego jako źródło napięcia analogowego, który reaguje na obecność lub brak pola magnetycznego.

---

## 6. Inicjalizacja i start systemu

W funkcji `setup()` program wykonuje następujące czynności:
1. wyłącza watchdog
2. inicjalizuje port szeregowy
3. konfiguruje piny przekaźników jako wyjścia
4. ustawia przekaźniki w stanie wyłączonym
5. inicjalizuje wyświetlacz OLED
6. odczytuje VCC
7. odczytuje napięcia z trzech czujników
8. przydziela początkowe stany logiczne na podstawie progu aktywacji
9. ustawia przekaźniki zgodnie z odczytem
10. uruchamia watchdog

Warto zaznaczyć, że przy starcie nie jest wykonywana żadna specjalna procedura kalibracji; sygnał jest po prostu odczytywany na bieżąco i oceniany względem progów.

Po starcie wyświetlany jest komunikat startowy oraz wartość VCC.

---

## 7. Pętla główna

W pętli `loop()` program wykonuje następujące czynności cyklicznie:
1. resetuje watchdog
2. odczytuje napięcia z każdego czujnika
3. konwertuje je na napięcia względem VCC
4. stosuje histerezę dla każdego kanału
5. ustawia stany przekaźników
6. raz na 500 ms wysyła dane do Serial Monitor
7. raz na 500 ms odświeża ekran OLED

---

## 8. Diagnostyka i prezentacja danych

Program wysyła do portu szeregowego dane w postaci:
- napięcie LOW
- napięcie HIGH
- napięcie SAFE
- stany przekaźników

Na wyświetlaczu OLED pokazuje:
- nazwy kanałów
- wartości napięć
- stany przekaźników

Dzięki temu możliwe jest sprawdzenie, czy czujniki pracują poprawnie, czy sygnał jest stabilny i czy przekaźniki odpowiednio reagują na zmiany poziomu.

---

## 9. Podsumowanie

Program dla TLE4946 działa jako system monitorujący trzy kanały czujników Hall z wykorzystaniem analogowych odczytów napięcia. W praktyce program:

- odczytuje napięcia z czujników,
- skaluję je względem aktualnego VCC,
- filtruje zakłócenia i niestabilności,
- stosuje histerezę,
- steruje przekaźnikami w zależności od stanu czujników.

Dzięki temu układ jest odporny na krótkotrwałe niestabilności sygnału, a stan przekaźników pozostaje stabilny nawet przy niewielkich wahaniach napięcia.

To sprawia, że system jest odpowiedni do prostego monitorowania poziomu pola magnetycznego oraz do realizacji prostego sterowania wyjściami na podstawie odczytu z czujników hallowskich.
