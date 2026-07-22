# Detektor poziomu wody — Reed Switch

Ten opis dotyczy działania programu w pliku [src/Detector_ReedSwitch/Detector_ReedSwitch.ino](src/Detector_ReedSwitch/Detector_ReedSwitch.ino).

## 1. Co robi program

Program odczytuje 3 kontaktrony (`LOW`, `HIGH`, `SAFETY`) i emuluje klasyczny układ 3 elektrod (`E1`, `E2`, `E3`) na 3 wyjściach przekaźnikowych (`R1`, `R2`, `R3`).

Architektura:

- `Sensor Manager` — odczyt wejść + filtracja drgań styków (`debounce`),
- `Level Resolver` — wyznaczanie poziomu i obsługa stanów przejściowych,
- `Relay Manager` — mapowanie poziomu na wyjścia przekaźników.

## 2. Wejścia i wyjścia

### Wejścia (kontaktrony)

- `REED_LOW_PIN = 2`
- `REED_HIGH_PIN = 3`
- `REED_SAFE_PIN = 4`

Wejścia pracują jako `INPUT_PULLUP`, więc:

- stan aktywny kontaktronu = `LOW` (zwarty do GND),
- stan nieaktywny = `HIGH`.

### Wyjścia (przekaźniki)

- `RELAY_LOW_PIN = A0` (`E1`)
- `RELAY_HIGH_PIN = A1` (`E2`)
- `RELAY_SAFE_PIN = A2` (`E3`)

## 3. Logika poziomów

Surowy stan wejść jest kodowany jako `raw = (low, high, safety)`:

- `000` → `EMPTY`
- `100` → `LEVEL1`
- `110` → `LEVEL2`
- `111` → `LEVEL3`

Mapowanie na elektrody/przekaźniki:

- `EMPTY`  → `E=000`
- `LEVEL1` → `E=100`
- `LEVEL2` → `E=110`
- `LEVEL3` → `E=111`

## 4. Filtrowanie i stabilizacja

Program ma dwa poziomy filtracji:

1. **Debounce wejść** (`DEBOUNCE_MS = 35 ms`) — eliminuje drgania styków.
2. **Stabilizacja poziomu** (`STABLE_MS = 1000 ms`) — nowy poziom musi utrzymać się przez zadany czas, zanim zostanie zatwierdzony.

Dzięki temu przekaźniki nie przełączają się od krótkich zakłóceń.

## 5. Stany przejściowe i diagnostyka

Niedozwolone kombinacje `raw`:

- `010`, `001`, `101`, `011`

Są traktowane jako stany przejściowe (bez natychmiastowej zmiany poziomu).

Kody diagnostyczne:

- `E00` — brak błędu,
- `E30` — kombinacja niedozwolona,
- `E31` — przeskok poziomu (zmiana o więcej niż 1 poziom),
- `E32` — zbyt długi stan przejściowy (`TRANSIENT_TIMEOUT_MS = 5000 ms`).

Resolver dopuszcza tylko przejścia sąsiednie:

- `EMPTY ↔ LEVEL1 ↔ LEVEL2 ↔ LEVEL3`.

## 6. Co dzieje się w `setup()`

1. Wyłączenie `WDT` na czas inicjalizacji.
2. Start `Serial` (`115200`).
3. Konfiguracja przekaźników jako `OUTPUT` i ustawienie stanu początkowego `OFF`.
4. Inicjalizacja wejść kontaktronów jako `INPUT_PULLUP`.
5. Start OLED (`SSD1306`, adres `0x3C`) i ekran startowy.
6. Próbkowanie wejść przez `INIT_SAMPLE_MS = 1500 ms`.
7. Głosowanie większościowe na najbardziej prawdopodobny poziom startowy.
8. Ustawienie poziomu początkowego i przekaźników.
9. Włączenie `WDT` (`WDTO_1S`).

## 7. Co dzieje się w `loop()`

W każdej iteracji:

1. `wdt_reset()`.
2. Odczyt wejść (`readSensors()`).
3. Wyznaczenie poziomu (`resolveLevel()`).
4. Aktualizacja przekaźników (`applyLevel()`).
5. Co `LOG_INTERVAL_MS = 500 ms`:
	- log przez `Serial` (`RAW`, `LVL`, `E`, `CODE`),
	- odświeżenie OLED tymi samymi informacjami.

## 8. Podsumowanie

Kod jest odporny na:

- drgania styków,
- chwilowe niespójne kombinacje wejść,
- skoki poziomu niezgodne z fizyką ruchu pływaka.

Jednocześnie zachowuje kompatybilność funkcjonalną z klasycznym układem trzech elektrod, bo na wyjściach generuje stabilne sygnały `E1/E2/E3`.
