# Water Detection - APS11450

Dokument opisuje działanie aplikacji [src/Detector_APS11450/Detector_APS11450.ino](src/Detector_APS11450/Detector_APS11450.ino).

## Cel

Aplikacja emuluje klasyczny układ 3 elektrod (E1/E2/E3) przy użyciu 3 czujników Halla APS11450 i pływaka z magnesem.
Wyjścia przekaźników odpowiadają poziomom cieczy:

- EMPTY -> R1=0 R2=0 R3=0
- LEVEL1 -> R1=1 R2=0 R3=0
- LEVEL2 -> R1=1 R2=1 R3=0
- LEVEL3 -> R1=1 R2=1 R3=1
- FAULT -> polityka bezpieczeństwa (w tym projekcie wszystkie OFF)

## Architektura logiczna

1. **Sensor Manager**
   - odczyt ADC kanałów LOW/HIGH/SAFE,
   - klasyfikacja sygnału (LOW/HIGH/EXT_FAULT/INT_FAULT),
   - zwraca `SensorState`.
2. **Level Resolver**
   - jedyny moduł z pamięcią stanu,
   - filtruje stany przejściowe,
   - egzekwuje przejścia poziomów,
   - generuje kod diagnostyczny.
3. **Relay Manager**
   - mapuje poziom na E1/E2/E3,
   - steruje przekaźnikami A0/A1/A2.

## Konfiguracja sprzętowa

### Piny

| Sygnał | Pin | Funkcja |
|---|---|---|
| HALL_LOW | A3 | wejście analogowe |
| HALL_HIGH | A6 | wejście analogowe |
| HALL_SAFE | A7 | wejście analogowe |
| RELAY_LOW | A0 | wyjście (R1 / E1) |
| RELAY_HIGH | A1 | wyjście (R2 / E2) |
| RELAY_SAFE | A2 | wyjście (R3 / E3) |
| OLED SDA | A4 | I2C |
| OLED SCL | A5 | I2C |

### OLED / Serial / WDT

- OLED: SSD1306 128x64, adres I2C `0x3C`
- Serial: `115200`
- Watchdog: `WDTO_1S`

## Klasyfikacja napięć APS11450

Próbka jest liczona jako ułamek zasilania: `vFrac = ADC / 1023.0`.

| Zakres VPU | Klasa |
|---|---|
| 0-10% | SIG_EXT_FAULT |
| 10-30% | SIG_LOW |
| 30-70% | SIG_EXT_FAULT |
| 70-90% | SIG_HIGH |
| 90-100% | SIG_INT_FAULT |

Progi w kodzie:
- `VFRAC_EXTLOW_MAX = 0.10`
- `VFRAC_LOW_MAX = 0.30`
- `VFRAC_EXTMID_MAX = 0.70`
- `VFRAC_HIGH_MAX = 0.90`

## Reprezentacja poziomu

Surowy stan bitowy: `raw = (low<<2) | (high<<1) | safety`

Dozwolone kombinacje:
- `000 -> EMPTY`
- `100 -> LEVEL1`
- `110 -> LEVEL2`
- `111 -> LEVEL3`

Niedozwolone kombinacje:
- `010`, `001`, `101`, `011`

Dla niedozwolonych kombinacji poziom nie jest zmieniany (utrzymanie poprzedniego stanu).

## Diagnostyka

### Kody

- `E00` brak błędu
- `E10/E11/E12` Internal Fault (LOW/HIGH/SAFE)
- `E20/E21/E22` External Fault (LOW/HIGH/SAFE)
- `E30` niedozwolona kombinacja raw
- `E31` przeskok poziomu (nie-sąsiedni)
- `E32` zbyt długi stan przejściowy
- `E33` błąd sekwencji (zarezerwowany)
- `E34` utrata synchronizacji (zarezerwowany)

### Reguły resolvera

- błąd czujnika (`*_Fault`) -> natychmiast `FAULT` + kod E1x/E2x,
- po ustąpieniu usterki: `FAULT -> UNKNOWN`,
- zmiana poziomu wymaga stabilności przez `STABLE_MS = 1000 ms`,
- niedozwolone `raw` -> `E30`, a po `TRANSIENT_TIMEOUT_MS = 5000 ms` -> `E32`,
- przeskok o więcej niż 1 poziom -> zatwierdzenie + `E31`.

## Sekwencja startu (`setup`)

1. `wdt_disable()`
2. konfiguracja przekaźników jako OUTPUT i ustawienie OFF,
3. inicjalizacja OLED,
4. pomiar `Vcc` (`readVcc()/1000.0`),
5. inicjalizacja poziomu przez głosowanie większościowe w oknie `INIT_SAMPLE_MS = 1500 ms`,
6. `resolverInit(initLevel)` i ustawienie wyjść,
7. log `INIT_COMPLETE` (Serial + OLED),
8. `wdt_enable(WDTO_1S)`.

## Pętla główna (`loop`)

1. `wdt_reset()`
2. `readSensors()`
3. `resolveLevel()`
4. `applyLevel()`
5. co `logInterval = 500 ms`:
   - log Serial: napięcia, klasy sygnałów, raw, poziom, E1/E2/E3, kod Exx,
   - aktualizacja OLED.

## Funkcje sprzętowe

- `readVcc()` – pomiar Vcc metodą bandgap AVR,
- `readADCStable(pin)` – podwójny odczyt ADC,
- `adcToVoltage(adc)` – konwersja ADC -> V.

## Wymagane biblioteki

- `avr/wdt.h`
- `Wire.h`
- `Adafruit_GFX.h`
- `Adafruit_SSD1306.h`

## Uruchomienie

1. Otwórz [src/Detector_APS11450/Detector_APS11450.ino](src/Detector_APS11450/Detector_APS11450.ino) w Arduino IDE.
2. Wybierz płytkę AVR i port COM.
3. Zainstaluj biblioteki Adafruit GFX i Adafruit SSD1306.
4. Wgraj szkic.
5. Otwórz Serial Monitor (115200).

## Uwagi

- W VS Code mogą pojawiać się ostrzeżenia includePath dla `avr/wdt.h`; kompilacja docelowo w Arduino IDE.
- Typy (`enum/struct`) są umieszczone przed pierwszą funkcją, aby uniknąć problemu auto-prototypów Arduino.
