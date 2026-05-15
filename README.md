# Water Detection - TLE4946

Projekt detekcji poziomu wody oparty na czujnikach Halla TLE4946.

## 📋 Opis projektu

System wykrywania poziomu wody wykorzystujący trzy czujniki Halla TLE4946. Projekt składa się z modułu mikrokontrolera (Arduino) oraz dedykowanych płytek PCB do konwersji sygnału z czujników Halla na sygnał elektrodowy.

## 🔧 Komponenty

### Hardware
- **Czujniki Halla TLE4946** - 3 sztuki do detekcji poziomu wody
- **Arduino** - mikrokontroler do przetwarzania sygnałów
- **LED** - 3 diody sygnalizacyjne (piny 2, 3, 4)
- **PCB Czujnik Wody** - płytka główna czujnika
- **PCB Konwerter Hal-Elektrody** - konwerter sygnału

### Parametry elektryczne
- Napięcie zasilania: **4.5V**
- Wejścia analogowe: **A0, A1, A2**
- Wyjścia LED: **D2, D3, D4**

## 📊 Zasada działania

System odczytuje napięcie z trzech czujników Halla i klasyfikuje stan każdego czujnika:

| Stan | Zakres napięcia | Opis |
|------|-----------------|------|
| **LOW** | 0% - 10% VCC | Brak detekcji wody |
| **HIGH** | 90% - 100% VCC | Wykryto wodę |
| **ERROR** | 10% - 90% VCC | Błąd czujnika (miganie LED) |

### Sygnalizacja LED
- **LED włączony** - stan HIGH (woda wykryta)
- **LED wyłączony** - stan LOW (brak wody)
- **LED migający (300ms)** - błąd czujnika

## 📁 Struktura projektu

```
my_project/
├── Detector_TLE4946/
│   └── Detektor_TLE4946.ino    # Kod Arduino
├── PCB_project/
│   ├── 3D_model/               # Modele 3D płytek
│   ├── EasyEda_Project/        # Projekty schematów i PCB
│   └── Gerber/                 # Pliki do produkcji PCB
└── Hal_Elec_Converter/         # Konwerter Hal-Elektrody
```

## 🚀 Uruchomienie

1. **Wgraj kod na Arduino:**
   - Otwórz plik `my_project/Detector_TLE4946/Detektor_TLE4946.ino` w Arduino IDE
   - Wybierz odpowiednią płytkę i port
   - Wgraj szkic

2. **Podłącz czujniki:**
   - Czujnik 1 → A0
   - Czujnik 2 → A1
   - Czujnik 3 → A2

3. **Podłącz LED:**
   - LED 1 → D2
   - LED 2 → D3
   - LED 3 → D4

4. **Monitor szeregowy:**
   - Otwórz monitor szeregowy (9600 baud)
   - Odczytuj napięcia i stany czujników

## 📦 Pliki PCB

### Gerber (do produkcji)
- `Gerber_Czujnik-Wody_PCB_Czujnik-Wody_2026-02-04.zip`
- `Gerber_Konwerter-Hal-Elektrody_PCB_Konwerter-Hal-Elektrody_2026-02-06.zip`

### Modele 3D
- `OBJ_PCB_Konwerter-Hal-Elektrody_2026-02-06.zip`

## 🛠️ Narzędzia

- **Arduino IDE** - programowanie mikrokontrolera
- **EasyEDA** - projektowanie schematów i PCB

## 📝 Licencja

Projekt open-source. Szczegóły w pliku LICENSE.

## 👤 Autor

Projekt Water Detection
