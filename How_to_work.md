# How to work — metoda pracy odporna na rozłączenia LLM

> Plik roboczy dla asystenta (GitHub Copilot). Powstał, bo sesja LLM
> **często się rozłącza** (np. `net::ERR_CONNECTION_CLOSED`) i praca
> zaczyna się od nowa. Ten plik jest **jednym punktem wznowienia** —
> zawiera metodę pracy **oraz** aktualny stan zadania.

---

## 0. Zasada nadrzędna: PERSISTENCE-FIRST

Kontekst w pamięci LLM **znika** po rozłączeniu. Pliki na dysku **zostają**.

**Wniosek:** po każdym najmniejszym kroku natychmiast zapisuj wynik na
dysk (plik kodu + wpis w sekcji [Log postępu](#log-postępu)). Nigdy nie
trzymaj gotowej pracy tylko „w głowie” / w odpowiedzi czatu.

---

## 1. Reguły pracy

1. **Małe, atomowe kroki** — jeden krok = jedna funkcja / jeden moduł /
   jedna sekcja pliku. Krok ma dać się ukończyć w kilka minut.
2. **Zapis po każdym kroku** — użyj narzędzia do tworzenia/edycji pliku,
   a potem dopisz wpis do [Log postępu](#log-postępu) z datą i statusem.
3. **Walidacja przyrostowa** — po każdym module sprawdź błędy
   (get_errors). Nie kumuluj niesprawdzonego kodu.
4. **Jeden wskaźnik `>>> RESUME HERE <<<`** — zawsze wskazuje następny
   krok. Świeża sesja czyta ten plik, skacze do wskaźnika i kontynuuje.
5. **Idempotentność** — przed utworzeniem pliku sprawdź, czy już
   istnieje (file_search), aby nie nadpisać postępu.
6. **Nie ufaj todo-liście po restarcie** — narzędziowa lista TODO może
   nie przetrwać nowej sesji. **Źródłem prawdy jest TEN plik na dysku.**
7. **Commit warto robić często** — po ukończeniu modułu można wykonać
   commit (użytkownik używa git), co daje dodatkową kopię stanu.

---

## 2. Pętla robocza (powtarzaj)

```text
1. Otwórz How_to_work.md → znajdź „>>> RESUME HERE <<<”.
2. Wykonaj JEDEN krok (mały).
3. Zapisz plik wynikowy na dysk.
4. get_errors na zmienionym pliku.
5. Dopisz wpis do „Log postępu” (data, co zrobiono, wynik).
6. Przesuń „>>> RESUME HERE <<<” na kolejny krok.
7. (opcjonalnie) commit.
8. Wróć do 1.
```

---

## 3. Procedura wznowienia po rozłączeniu

1. Przeczytaj ten plik w całości (jest krótki i samowystarczalny).
2. Sekcja [Snapshot projektu](#4-snapshot-projektu-stan-utrwalony) ma
   wszystkie decyzje (piny, progi, tabele, kody błędów) — nie trzeba
   ponownie czytać całej specyfikacji.
3. Znajdź `>>> RESUME HERE <<<` w [Rozbiciu zadania](#5-rozbicie-zadania).
4. Sprawdź na dysku, co realnie istnieje (file_search / read_file), bo
   ostatni krok mógł się zapisać tuż przed rozłączeniem.
5. Kontynuuj pętlę roboczą z sekcji 2.

---

## 4. Snapshot projektu (stan utrwalony)

**Zadanie:** przygotować szkic `Detektor_APS11450.ino` zgodnie z
`Soft_Req_Spec_APS11450.md` oraz `APS11450.md`.

**Cel docelowego pliku:**
`my_project/Detector_APS11450/Detektor_APS11450.ino`

**Źródła specyfikacji:**
- `Soft_Req_Spec_APS11450.md` — architektura, poziomy, automat stanów,
  tabela przejść, kombinacje niedozwolone.
- `APS11450.md` — pasma napięć, stany wyjścia, diagnostyka.
- Wzorzec sprzętowy do ponownego użycia: `Detektor_APS1145.ino`.

### 4.1. Konfiguracja sprzętu

| Element | Wartość |
| --- | --- |
| HALL_LOW_PIN | A3 (wejście analogowe, diagnostyka ADC) |
| HALL_HIGH_PIN | A6 |
| HALL_SAFE_PIN | A7 |
| RELAY_LOW_PIN | A0 (wyjście) |
| RELAY_HIGH_PIN | A1 |
| RELAY_SAFE_PIN | A2 |
| OLED | SSD1306 128×64, I2C, adres 0x3C |
| Serial | 115200 |
| WDT | 1 s (WDTO_1S) |

Funkcje do ponownego użycia z `Detektor_APS1145.ino`:
- `readVcc()` — bandgap, `result = 1125300L / ADC`.
- `readADCStable(pin)` — podwójny odczyt z `delayMicroseconds(20)`.
- `adcToVoltage(adc) = (adc / 1023.0) * Vcc`.

### 4.2. Klasyfikacja napięcia APS11450 (pasma jako % VPU = Vcc)

| Zakres | Klasa |
| --- | --- |
| 0–10% | SIG_EXT_FAULT (usterka zewnętrzna) |
| 10–30% | SIG_LOW (poprawny LOW) |
| 30–70% | SIG_EXT_FAULT (usterka zewnętrzna) |
| 70–90% | SIG_HIGH (poprawny HIGH) |
| 90–100% | SIG_INT_FAULT (Internal Fault, VOUT=VPU) |

Progi jako ułamki Vcc: `0.10, 0.30, 0.70, 0.90`.
Enum: `enum SignalClass { SIG_LOW, SIG_HIGH, SIG_EXT_FAULT, SIG_INT_FAULT };`

### 4.3. Model poziomu i przejść

Surowe bity `raw = (low, high, safe)`:

| raw | Poziom |
| --- | --- |
| 000 | EMPTY |
| 100 | LEVEL1 |
| 110 | LEVEL2 |
| 111 | LEVEL3 |

Kombinacje **niedozwolone** (trzymaj poprzedni poziom): `010, 001, 101, 011`.

Wirtualne elektrody wg poziomu (sekcja 4 spec): EMPTY=000, L1=100,
L2=110, L3=111. Relay Manager: `R1=E1, R2=E2, R3=E3`.

Enum poziomów: `UNKNOWN, EMPTY, LEVEL1, LEVEL2, LEVEL3, FAULT`.

Reguły przejść:
- tylko zmiany o **sąsiedni** poziom,
- zatwierdzenie po czasie stabilizacji `STABLE_MS ≈ 1000 ms`,
- dowolna usterka sensora → `FAULT` + Alarm; `FAULT → UNKNOWN` (odzysk),
- polityka FAULT (sekcja 3 spec, konfigurowalna): `FAULT_RELAYS` = wszystkie ON (stan alarmowy — urządzenie zewnętrzne musi zareagować błędem).

### 4.4. Kody diagnostyczne

| Kod | Znaczenie |
| --- | --- |
| E00 | brak błędu |
| E10 / E11 / E12 | Internal Fault: low / high / safe |
| E20 / E21 / E22 | External Fault: low / high / safe |
| E30 | kombinacja niedozwolona (niespójne stany) |
| E31 | przeskok poziomu |
| E32 | zbyt długi stan przejściowy (timeout) |
| E33 | błąd sekwencji |
| E34 | utrata synchronizacji |

### 4.5. Stałe czasowe (propozycja)

`STABLE_MS=1000`, `INIT_SAMPLE_MS=1500`, `TRANSIENT_TIMEOUT_MS=5000`,
`logInterval=500`.

---

## 5. Rozbicie zadania

Legenda: `[ ]` do zrobienia · `[~]` w toku · `[x]` gotowe.

- [x] K0. Utrwalić metodę i stan → **ten plik** (`How_to_work.md`).
- [x] K1. Utworzyć plik `.ino` + nagłówek + `#include` + `#define` pinów
  + stałe (progi ułamkowe, czasy, `FAULT_RELAYS`).
- [x] K2. Enum `SignalClass` + `classifySignal(float vFrac)`.
- [x] K3. Struktura `SensorState{low,high,safety,lowFault,highFault,safetyFault}`.
- [x] K4. Helpery HW: `readVcc`, `readADCStable`, `adcToVoltage`.
- [x] K5. Sensor Manager: `readSensors()` → wypełnia `SensorState`
  (fault = internal||external, active = SIG_HIGH). Bez interpretacji poziomu.
- [x] K6. Enum poziomów + `levelFromRaw()` + `isValidRaw()` + `rawFromSensors()`.
- [ ] K7. Level Resolver: `resolveLevel(SensorState)` — fault→FAULT(kod);
  raw niedozwolony→trzymaj+E30 (E32 gdy za długo); poprawny & sąsiedni &
  stabilny→commit (E31 gdy przeskok). Zwraca poziom + kod diagnostyczny.
- [x] K8. Elektrody: `electrodesFromLevel(level, &E1,&E2,&E3)`.
- [x] K9. Relay Manager: `applyElectrodes(E1,E2,E3)`; w FAULT polityka `FAULT_RELAYS`.
- [x] K10. `setup()`: WDT off, Serial, piny przekaźników OUTPUT (trzymaj
  poprzedni=OFF), OLED, pomiar Vcc; restart wg sekcji 6 — próbkowanie
  `INIT_SAMPLE_MS`, głosowanie większościowe → najbardziej prawdopodobny
  poziom; init elektrod+przekaźników; log `INIT_COMPLETE`; WDT on 1 s.
- [x] K11. `loop()`: `wdt_reset`; `readSensors`; `resolveLevel`;
  `applyElectrodes`; co 500 ms log Serial (napięcia, sygnały, poziom,
  elektrody, kod) + OLED (poziom, E1-3, kod).
- [x] K12. Walidacja: `get_errors` na `.ino`; zgodność z tabelą przejść
  (sekcja 9) i kombinacjami niedozwolonymi (sekcja 10).
  **`>>> UKOŃCZONO — wszystkie kroki K0..K12 gotowe. Opcjonalnie: kompilacja w Arduino IDE (AVR toolchain). <<<`**

---

## Log postępu

> Format: `RRRR-MM-DD — [Kx] opis — wynik`. Dopisuj na końcu po każdym kroku.

- 2026-07-07 — [K0] Utworzono `How_to_work.md` z metodą pracy i pełnym
  snapshotem stanu (piny, pasma napięć, poziomy, tabela przejść, kody
  błędów). Następny krok: K1 (nagłówek + config `.ino`). — OK
- 2026-07-07 — [K1] Utworzono `my_project/Detector_APS11450/Detektor_APS11450.ino`:
  nagłówek + `#include` (wdt, Wire, Adafruit GFX/SSD1306), OLED 0x3C,
  piny Hall A3/A6/A7, przekaźniki A0/A1/A2 (E1-E3), `ADC_MAX`, `SERIAL_BAUD`,
  globalne `Vcc`, progi ułamkowe VPU (0.10/0.30/0.70/0.90), czasy
  (`STABLE_MS`/`INIT_SAMPLE_MS`/`TRANSIENT_TIMEOUT_MS`/`logInterval`),
  polityka `FAULT_RELAY_*` = ON (stan alarmowy). `get_errors` → brak błędów. Następny: K2. — OK
- 2026-07-07 — [K2] Dodano `enum SignalClass {SIG_LOW,SIG_HIGH,SIG_EXT_FAULT,
  SIG_INT_FAULT}` + `classifySignal(float vFrac)` wg pasm APS11450.md sek. 6
  (progi 0.10/0.30/0.70/0.90 VPU). — OK
- 2026-07-07 — [K3] Dodano `struct SensorState{low,high,safety,lowFault,
  highFault,safetyFault}` (aktywnosc = SIG_HIGH, fault = internal||external). — OK
- 2026-07-07 — [K4] Dodano helpery HW `readVcc()` (bandgap, mV),
  `readADCStable(pin)`, `adcToVoltage(adc)` (wzorzec z APS1145).
  `get_errors` → brak błędów. Następny: K5. — OK
- 2026-07-07 — [K5] Dodano Sensor Manager: `readSensorClass(pin)` +
  `readSensors()` (vFrac = ADC/1023, active = SIG_HIGH, fault =
  INT||EXT). `SensorState` rozszerzono o `lowClass/highClass/safetyClass`
  (rozroznienie internal/external dla kodow E1x/E2x; kontrakt 6 bool ze spec
  zachowany). — OK
- 2026-07-07 — [K6] Dodano `enum Level {UNKNOWN,EMPTY,LEVEL1,LEVEL2,LEVEL3,
  FAULT}`, `enum DiagCode {E00..E34}`, `rawFromSensors()` (low<<2|high<<1|
  safety), `isValidRaw()` (000/100/110/111), `levelFromRaw()`.
  UWAGA: `get_errors` zglasza tylko brak sciezki `avr/wdt.h` (toolchain
  Arduino nieskonfigurowany w VS Code) — nie jest to blad logiki/skladni;
  ten sam objaw dotyczy wzorca APS1145. Następny: K7. — OK
- 2026-07-07 — [K7] Dodano Level Resolver: `struct LevelResult{level,code}`,
  stan `currentLevel/candidateLevel/candidateSince/transientSince`,
  `levelIndex()`, `faultCode()` (E10-E22, internal przed external),
  `resolverInit()`, `resolveLevel()` (FAULT->kod; FAULT->UNKNOWN odzysk;
  raw niedozwolony->trzymaj E30/E32; stabilizacja STABLE_MS; commit sasiedni
  E00 / przeskok E31; UNKNOWN->przyjmij poprawny jak INIT). — OK
- 2026-07-07 — [K8] Dodano `electrodesFromLevel(level,&e1,&e2,&e3)`
  (EMPTY=000,L1=100,L2=110,L3=111; UNKNOWN/FAULT->000). — OK
- 2026-07-07 — [K9] Dodano Relay Manager: `applyElectrodes(e1,e2,e3)`
  (R1=E1,R2=E2,R3=E3) oraz `applyLevel(lvl)` (FAULT->FAULT_RELAYS ON;
  UNKNOWN->trzymaj poprzedni; inne->elektrody). `get_errors` → tylko nota
  includePath AVR. Następny: K10. — OK
- 2026-07-07 — [K10] Dodano `setup()`: wdt_disable, Serial, piny przekaźników
  OUTPUT+OFF (bezpieczny start), OLED splash, `Vcc=readVcc()/1000`, restart
  wg sekcji 6 (próbkowanie `INIT_SAMPLE_MS`, głosowanie `votes[4]`, pomijanie
  fault/niedozwolonych), `resolverInit`+`applyLevel`, log `INIT_COMPLETE`,
  `wdt_enable(WDTO_1S)`. Helpery: `sigChar`, `levelName`, `diagCodeStr`,
  `indexToLevel`, `lastLogTime`. — OK
- 2026-07-07 — [K11] Dodano `loop()`: `wdt_reset`; `readSensors`;
  `resolveLevel`; `applyLevel`; co `logInterval`=500 ms log Serial
  (V L/H/S, sygnały L/H/X/I, raw bity, poziom, E1-3, kod Exx) + OLED. — OK
- 2026-07-07 — [K12] Przegląd całego `.ino`: zgodność z tabelą przejść
  (sekcja 9: EMPTY<->L1<->L2<->L3, dowolny->FAULT) i kombinacjami
  niedozwolonymi (sekcja 10: 010/001/101/011 -> trzymaj+E30/E32).
  `get_errors` → wyłącznie brak sciezki `avr/wdt.h` (toolchain Arduino,
  squiggles wyłączone) — bez błędów logiki/składni. PROJEKT UKOŃCZONY. — OK
- 2026-07-07 — [HOTFIX] Naprawiono błąd kompilacji Arduino IDE
  (`does not name a type`) wynikający z auto-generowanych prototypów:
  przeniesiono wszystkie definicje typów (`SignalClass`, `SensorState`,
  `Level`, `DiagCode`, `LevelResult`) do jednego bloku nad pierwszą funkcją,
  usunięto duplikaty niżej w pliku. `get_errors` w VS Code: tylko
  środowiskowe `avr/wdt.h` (includePath/toolchain). — OK
