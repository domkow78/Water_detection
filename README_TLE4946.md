# Water Detection - TLE4946

Projekt detekcji poziomu wody oparty na czujnikach Halla TLE4946.

System wykrywania poziomu wody wykorzystujący trzy czujniki Halla TLE4946. Projekt składa się z modułu mikrokontrolera (Arduino) oraz dedykowanych płytek PCB do konwersji sygnału z czujników Halla na sygnał elektrodowy. System steruje trzema przekaźnikami (LOW, HIGH, SAFE) na podstawie odczytów analogowych z czujników. Napięcie zasilania VCC jest mierzone dynamicznie przez wewnętrzne źródło referencyjne AVR. Aktualne pomiary i stany przekaźników są prezentowane w czasie rzeczywistym na wyświetlaczu OLED (SSD1306) oraz na porcie szeregowym.

## 🔧 Komponenty

### Hardware
- **Czujniki Halla TLE4946** - 3 sztuki do detekcji poziomu wody
- **Arduino (AVR)** - mikrokontroler do przetwarzania sygnałów
- **Przekaźniki** - 3 sztuki (LOW, HIGH, SAFE)
- **Wyświetlacz OLED SSD1306** - 128×64 px, magistrala I2C (adres 0x3C)
- **PCB Czujnik Wody** - płytka główna czujnika
- **PCB Konwerter Hal-Elektrody** - konwerter sygnału

### Wymagane biblioteki (Arduino)
- **`avr/wdt.h`** - Watchdog Timer (wbudowana)
- **`Wire.h`** - komunikacja I2C (wbudowana)
- **`Adafruit_GFX`** - biblioteka graficzna
- **`Adafruit_SSD1306`** - obsługa wyświetlacza OLED

### Mapowanie pinów

| Sygnał | Pin Arduino | Kierunek |
|--------|-------------|----------|
| HALL_LOW  | A3 | Wejście analogowe |
| HALL_HIGH | A6 | Wejście analogowe |
| HALL_SAFE | A7 | Wejście analogowe |
| RELAY_LOW  | A0 | Wyjście cyfrowe |
| RELAY_HIGH | A1 | Wyjście cyfrowe |
| RELAY_SAFE | A2 | Wyjście cyfrowe |
| OLED SDA | A4 | I2C (dane) |
| OLED SCL | A5 | I2C (zegar) |

## 📊 Zasada działania

System odczytuje napięcie z trzech czujników Halla i steruje odpowiadającymi im przekaźnikami z histerezą:

| Próg | Napięcie | Działanie |
|------|----------|-----------|
| **HIGH_THRESHOLD_V** | 1.5 V | Przekaźnik włączany (stan aktywny) |
| **LOW_THRESHOLD_V**  | 0.3 V | Przekaźnik wyłączany (stan nieaktywny) |

Hystereza zapobiega drganiom wyjść przy napięciach pomiędzy progami.

### Pomiar VCC
Napięcie zasilania jest mierzane przy starcie za pomocą wewnętrznego źródła referencyjnego AVR (bandgap 1.1 V) i używane do przeliczania wartości ADC na wolty.

### Watchdog Timer
Aktywowany Watchdog Timer (WDT) z timeout **1 sekunda** zapewnia automatyczny restart przy zawieszeniu systemu.

### Logowanie szeregowe
Co **500 ms** na port szeregowy wysyłane są zmierzone napięcia i stany wszystkich trzech kanałów.

### Wyświetlacz OLED
Wyświetlacz SSD1306 (128×64, I2C @ 0x3C) prezentuje interfejs użytkownika:
- **Ekran startowy** - komunikat inicjalizacji oraz zmierzone napięcie VCC
- **Ekran monitorowania** - odświeżany co **500 ms**, pokazuje napięcia czujników LOW/HIGH/SAFE oraz stany przekaźników (0/1)

## 📁 Struktura projektu

```
my_project/
├── Detector_TLE4946/
│   └── Detector_TLE4946.ino    # Kod Arduino
├── Detector_APS1145/
│   └── Detektor_APS1145.ino    # Alternatywny detektor
├── PCB_project/
│   ├── 3D_model/               # Modele 3D płytek
│   ├── EasyEda_Project/        # Projekty schematów i PCB
│   └── Gerber/                 # Pliki do produkcji PCB
└── Hal_Elec_Converter/         # Konwerter Hal-Elektrody
```

## 🚀 Uruchomienie

1. **Wgraj kod na Arduino:**
   - Otwórz plik `my_project/Detector_TLE4946/Detector_TLE4946.ino` w Arduino IDE
   - Wybierz odpowiednią płytkę (AVR) i port
   - Wgraj szkic

2. **Podłącz czujniki Halla:**
   - HALL LOW  → A3
   - HALL HIGH → A6
   - HALL SAFE → A7

3. **Podłącz przekaźniki:**
   - RELAY LOW  → A0
   - RELAY HIGH → A1
   - RELAY SAFE → A2

4. **Podłącz wyświetlacz OLED (I2C):**
   - SDA → A4
   - SCL → A5
   - VCC → 3.3V / 5V, GND → GND

5. **Monitor szeregowy:**
   - Otwórz monitor szeregowy (**115200 baud**)
   - Obserwuj zmierzone napięcia i stany przekaźników co 500 ms

## 📦 Pliki PCB

### Gerber (do produkcji)
- `Gerber_Czujnik-Wody_PCB_Czujnik-Wody_2026-02-04.zip`
- `Gerber_Konwerter-Hal-Elektrody_PCB_Konwerter-Hal-Elektrody_2026-02-06.zip`

### Modele 3D
- `OBJ_PCB_Konwerter-Hal-Elektrody_2026-02-06.zip`

## 🛠️ Narzędzia

- **Arduino IDE** - programowanie mikrokontrolera
- **EasyEDA** - projektowanie schematów i PCB
- **Adafruit GFX Library** - grafika na wyświetlaczu
- **Adafruit SSD1306** - sterownik wyświetlacza OLED

## 📝 Licencja

Projekt open-source. Szczegóły w pliku LICENSE.

## 👤 Autor

Projekt Water Detection
