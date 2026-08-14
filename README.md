# Water Detection

Repozytorium zawiera projekt systemu detekcji obecności magnesu i sterowania przekaźnikami dla różnych typów czujników wykorzystywanych w układzie wodnym.

## Cel projektu

Głównym celem jest monitorowanie stanu czujników, interpretacja sygnałów z wejść analogowych oraz sterowanie przekaźnikami w zależności od wykrycia obecności magnesu lub zmiany poziomu cieczy.

## Zawartość repozytorium

### Dokumentacja
- [APS11450_Program_Description.md](APS11450_Program_Description.md) — opis działania programu dla czujnika APS11450.
- [TLE4946_Program_Description.md](TLE4946_Program_Description.md) — opis działania programu dla czujnika TLE4946.
- [TMAG5124_Program_Description.md](TMAG5124_Program_Description.md) — opis działania programu dla czujnika TMAG5124B.
- [ReedSwitch_Program_Description.md](ReedSwitch_Program_Description.md) — opis działania programu dla czujnika Reed Switch.
- [APS11450.md](APS11450.md), [TLE4946.md](TLE4946.md), [TMAG5124B.md](TMAG5124B.md) — dokumentacja techniczna sensorów.

### Katalog src
W katalogu [src](src) znajdują się programy Arduino dla poszczególnych czujników:
- [src/Detector_APS11450](src/Detector_APS11450)
- [src/Detector_TLE4946](src/Detector_TLE4946)
- [src/Detector_TMAG5124](src/Detector_TMAG5124)
- [src/Detector_ReedSwitch](src/Detector_ReedSwitch)

Każdy program realizuje własną logikę detekcji i sterowania, dostosowaną do konkretnego typu czujnika.

### Katalog doc
W katalogu [doc](doc) znajduje się dokumentacja projektowa, modele 3D, schematy i pliki PCB związane z konstrukcją urządzenia.

### Katalog tests
Katalog [tests](tests) jest przeznaczony na testy i sprawdzenia funkcjonalne projektu.

## Wspierane typy czujników

- APS11450 — czujnik Halla używany do detekcji poziomu cieczy.
- TLE4946 — czujnik Halla analogowego typu.
- TMAG5124B — czujnik prądowy z detekcją obecności magnesu.
- Reed Switch — czujnik binarny z połączeniem typu NO/NC zależnym od obecności magnesu.

## Główne funkcje systemu

- odczyt ADC z wejść czujników,
- konwersja sygnału na napięcie i prawidłową interpretację,
- histereza i filtracja stanów przejściowych,
- sterowanie przekaźnikami,
- logowanie danych przez UART,
- wizualizacja stanu na wyświetlaczu OLED.

## Narzędzia i platforma

Projekt jest realizowany w środowisku Arduino dla mikrokontrolerów AVR, z wykorzystaniem:
- biblioteki Arduino Core,
- wyświetlacza OLED SSD1306,
- obsługi watchdoga i ADC,
- przekaźników sterujących pracą układu.

## Krótka instrukcja

1. Otwórz odpowiedni plik programu z katalogu [src](src).
2. Wgraj skrypt na mikrokontroler.
3. Sprawdź działanie za pomocą monitora szeregowego i wyświetlacza OLED.
4. W razie potrzeby korzystaj z opisów w dokumentacji programowej.

## Uwagi

Repozytorium zawiera wersje projektowe dla różnych czujników i różnych podejść do detekcji, dlatego ważne jest dobieranie odpowiedniego programu do konkretnych komponentów zastosowanych w układzie.
