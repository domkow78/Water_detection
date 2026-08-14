# Opis działania programu dla Reed Switch

## 1. Cel programu

Program sprawdza trzy kanały Reed Switch:
- `LOW`
- `HIGH`
- `SAFE`

Dla każdego kanału odczytuje stan wejścia i steruje odpowiadającym mu przekaźnikiem.

Zasada działania jest prosta i binarna:
- reed zwarty przez magnes = stan aktywny
- reed rozwarty = stan nieaktywny
- przekaźnik odzwierciedla stan danego czujnika

Nie ma tutaj mapowania poziomu na inny stan, nie ma automatu stanu i nie ma rozbudowanej diagnostyki błędów.

---

## 2. Wejścia i wyjścia

### 2.1. Wejścia

Wszystkie wejścia są ustawione jako `INPUT_PULLUP`:
- `REED_LOW_PIN = 2`
- `REED_HIGH_PIN = 3`
- `REED_SAFE_PIN = 4`

Dla takiego podłączenia:
- stan aktywny = `LOW` (kontakt zwarty, magnes w pobliżu, wejście podłączone do GND)
- stan nieaktywny = `HIGH` (kontakt rozwarty, wejście podciągnięte do VCC)

### 2.2. Wyjścia

Przekaźniki są sterowane osobno dla każdego kanału:
- `RELAY_LOW_PIN = A0`
- `RELAY_HIGH_PIN = A1`
- `RELAY_SAFE_PIN = A2`

Każdy przekaźnik ma odpowiadać stanowi swojego reed switcha.

---

## 3. Logika działania

Program odczytuje napięcie na każdym wejściu Reed Switch i interpretuje je jako dwa stany:

- napięcie bliskie 0 V → reed zwarty → przekaźnik ON
- napięcie bliskie VCC → reed rozwarty → przekaźnik OFF

To jest czysta logika binarna. Każdy kanał działa niezależnie.

---

## 4. Histereza

Aby uniknąć migania przy przejściach, program stosuje progi histerezy:

- `REED_ACTIVE_V = 1.0 V`
- `REED_RELEASED_V = 2.5 V`

Dla danego kanału:
- jeżeli stan jest nieaktywny i napięcie spadnie poniżej 1,0 V, kanał przechodzi do aktywnego,
- jeżeli stan jest aktywny i napięcie wzrośnie powyżej 2,5 V, kanał przechodzi do nieaktywnego,
- w pozostałych przypadkach stan zostaje bez zmian.

Dzięki temu krótkie oscylacje napięcia nie powodują niepotrzebnych przełączeń przekaźników.

---

## 5. Odczyt analogowy i VCC

Program mierzy napięcie na wejściach analogowo, ale robi to wyłącznie po to, aby odczytać rzeczywisty poziom sygnału i porównać go z progami histerezy. Nie ma osobnej funkcji diagnostycznej typu `OPEN`/`SHORT`.

Wartość `Vcc` jest odczytywana przez funkcję `readVcc()` i wykorzystywana do przeliczenia surowego odczytu ADC na napięcie w woltoch.

W praktyce oznacza to, że program ma dane do logowania i wizualizacji, ale nie rozpoznaje specjalnych stanów awarii, ponieważ czujnik Reed w tym układzie ma tylko dwa sensowne stany: zamknięty lub rozwarty.

---

## 6. Co dzieje się w `setup()`

W funkcji `setup()` program:
1. inicjalizuje port szeregowy,
2. ustawia wejścia Reed Switch jako `INPUT_PULLUP`,
3. ustawia wszystkie przekaźniki jako `OUTPUT`,
4. wyłącza przekaźniki na starcie,
5. inicjalizuje OLED,
6. odczytuje napięcie zasilania `Vcc`,
7. odczytuje początkowe stany wejść,
8. ustawia przekaźniki zgodnie z aktualnym stanem każdego kanału,
9. uruchamia watchdog.

---

## 7. Co dzieje się w `loop()`

W każdej iteracji programu:
1. odczytywane są wartości ADC dla `LOW`, `HIGH` i `SAFE`,
2. sygnały są zamieniane na napięcie w woltoch,
3. stosowana jest histereza dla każdego kanału,
4. przekaźniki otrzymują stan zgodny z wejściem,
5. dane są wysyłane do `Serial Monitor` i ekranu OLED.

---

## 8. Podsumowanie

Program dla Reed Switch działa w modelu 3 niezależnych kanałów binarnych:

- `LOW`: reed zwarty → LOW → przekaźnik ON
- `HIGH`: reed zwarty → LOW → przekaźnik ON
- `SAFE`: reed zwarty → LOW → przekaźnik ON

Przy rozwartym kontakcie:

- `HIGH`/`LOW`/`SAFE` = `HIGH` → przekaźnik OFF

W tym układzie największe znaczenie ma poprawna interpretacja napięcia i histereza. Nie ma rozbudowanej diagnostyki, bo czujnik ma tylko 2 realne stany logiczne: zamknięty i rozwarty.
