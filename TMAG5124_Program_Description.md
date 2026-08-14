# Opis działania programu dla TMAG5124

## 1. Cel programu

Program monitoruje trzy kanały czujników TMAG5124B zintegrowanych w układzie detekcji obecności magnesu. Każdy kanał jest odczytywany poprzez napięcie powstające na rezystorze pomiarowym 220 Ω. Na podstawie tego napięcia mikrokontroler decyduje, czy magnes jest obecny, czy nie, a następnie steruje odpowiednim przekaźnikiem.

Czujnik pracuje w trybie prądowym:
- przy braku magnesu: około 14,5 mA
- przy obecności magnesu: około 3,5 mA

Ponieważ prąd płynie przez rezystor 220 Ω, zmiana prądu powoduje zmianę napięcia:
- brak magnesu: około 3,19 V
- magnes obecny: około 0,77 V

W układzie założono, że przekaźnik ma być aktywny wyłącznie wtedy, gdy zostanie wykryty magnes.

---

## 2. Założenia projektowe

### 2.1. Obwód pomiarowy

Każdy czujnik TMAG5124B jest podłączony do układu z rezystorem pomiarowym 220 Ω. Na tym rezystorze powstaje napięcie proporcjonalne do prądu czujnika.

Napięcie jest mierzone przez wejścia ADC mikrokontrolera:
- SENSOR_LOW_PIN = A3
- SENSOR_HIGH_PIN = A6
- SENSOR_SAFE_PIN = A7

### 2.2. Przekaźniki

Sterowanie przekaźnikami odbywa się poprzez wyjścia cyfrowe:
- RELAY_LOW_PIN = A0
- RELAY_HIGH_PIN = A1
- RELAY_SAFE_PIN = A2

Przekaźniki są przełączane tylko na podstawie odczytu z czujników, a nie bezpośrednio przez sam czujnik.

---

## 3. Odczyt z ADC i uśrednianie

Każdy kanał jest odczytywany przez wejście ADC mikrokontrolera. Aby zmniejszyć wpływ szumu, zakłóceń i krótkich fluktuacji napięcia, program wykonuje uśrednianie kilku kolejnych odczytów dla każdego wejścia.

W praktyce funkcja odczytu działa w następujący sposób:
1. pobiera odczyt z wejścia ADC
2. wykonuje dodatkowe 7 odczytów w krótkich odstępach czasowych
3. sumuje wszystkie wyniki
4. dzieli sumę przez liczbę próbek
5. otrzymuje średnią wartość, która jest dalej przeliczana na napięcie

Taki mechanizm pozwala ograniczyć wpływ krótkotrwałych zmian sygnału i poprawia stabilność działania czujników oraz przekaźników.

Po uśrednieniu wartość ADC jest zamieniana na napięcie zgodnie z zależnością:

V = (ADC / 1023) × VCC

Następnie otrzymane napięcie jest analizowane przez logikę detekcji magnesu i diagnostyki.

### 3.1. Weryfikacja VCC

Program weryfikuje napięcie zasilania mikrokontrolera w funkcji `readVcc()`. Wewnętrzny mechanizm AVR ustawia wejście ADC do pomiaru napięcia odniesienia, a następnie odczytuje wynik z rejestru `ADC`.

Dzięki temu system nie zakłada sztywnego napięcia 5 V, tylko rzeczywiście odczytuje aktualne VCC. Wynik jest przeliczany według zależności:

VCC = 1125300 / ADC

Wartość ta jest wyrażona w milivoltach, a następnie zamieniana na wolty przez podzielenie przez 1000.

Dzięki temu każda wartość ADC jest skalowana do aktualnego napięcia zasilania układu, co poprawia dokładność pomiaru napięcia na rezystorze 220 Ω.

---

## 4. Logika detekcji magnesu

Program odczytuje napięcie z każdego kanału, a następnie stosuje histerezę:
- kiedy stan jest nieaktywny i napięcie spadnie poniżej 1,5 V, stan przechodzi do aktywnego
- kiedy stan jest aktywny i napięcie wzrośnie powyżej 2,6 V, stan przechodzi do nieaktywnego

Dzięki temu układ nie „miga” przy wartościach napięcia bliskich progowi.

### 3.1. Praktyczna interpretacja

- brak magnesu → napięcie ~3,19 V → stan nieaktywny
- magnes obecny → napięcie ~0,77 V → stan aktywny

W praktyce oznacza to, że przy obecności magnesu przekaźnik zostaje włączony.

---

## 5. Diagnostyka czujników

Dodatkowo program rozpoznaje stany awaryjne, które nie są poprawnym wykryciem magnesu:

### 5.1. Brak prądu / przerwa obwodu

Jeżeli napięcie na rezystorze jest poniżej 0,25 V, program klasyfikuje czujnik jako:
- OPEN

To oznacza brak prądu lub przerwę w obwodzie. W takim stanie przekaźnik jest wyłączany, ponieważ nie ma prawidłowego sygnału z czujnika.

### 5.2. Prąd znacznie powyżej normy

Jeżeli napięcie przekroczy 4,2 V, program klasyfikuje czujnik jako:
- OVR (over current)

To oznacza, że czujnik pobiera prąd znacznie większy niż w normalnym zakresie pracy. Taki stan jest traktowany jako awaria i też powoduje wyłączenie przekaźnika.

### 5.3. Stan prawidłowy

Dla napięcia w normalnym zakresie program przyjmuje:
- OK

---

## 6. Sterowanie przekaźnikami

Po przetworzeniu odczytu dla każdego kanału program wykonuje następujące kroki:
1. odczyt napięcia z czujnika
2. rozpoznanie diagnostyki (OK / OPEN / OVR)
3. zastosowanie histerezy przy poprawnym odczycie
4. ustawienie stanu przekaźnika
5. zapisanie stanu do wyjścia cyfrowego

Przekaźnik jest aktywowany tylko wtedy, gdy:
- czujnik jest w stanie poprawnym,
- magnes został wykryty,
- napięcie mieszczą się w zakresie aktywacji.

W przypadku stanu OPEN lub OVR przekaźnik jest wymuszony do stanu wyłączonego.

---

## 7. Wyświetlacz OLED

Program wyświetla dane na ekranie SSD1306:
- napięcie każdego czujnika w V
- status diagnostyczny każdego czujnika (OK / OPEN / OVR)
- stan przekaźników

Dodatkowo wyświetlany jest komunikat startowy oraz aktualna wartość Vcc.

---

## 8. Port szeregowy

Program wysyła dane do Serial Monitor co 500 ms. W raporcie zawarte są:
- napięcia z trzech czujników
- ich status diagnostyczny
- stany przekaźników

Dzięki temu możliwe jest łatwe monitorowanie działania układu podczas testów i kalibracji.

---

## 9. Watchdog

Program włącza watchdog o interwale 1 s. Dzięki temu układ jest chroniony przed zawieszeniem w pętli działania i automatycznie resetuje się w przypadku zablokowania programu.

---

## 10. Podsumowanie

Program działa zgodnie z następującą zasadą:

- TMAG5124 mierzy prąd zależny od obecności magnesu
- prąd jest zamieniany na napięcie na rezystorze 220 Ω
- mikrokontroler interpretuje to napięcie
- jeżeli magnes jest obecny, przekaźnik jest włączany
- jeżeli brak prądu lub prąd jest zbyt wysoki, stan jest uznawany za awarię i przekaźnik jest wyłączany

To zapewnia stabilną pracę, ochronę przed fałszywym załączeniem oraz diagnostykę stanu czujników.
