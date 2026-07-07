# Sterownik poziomu wody oparty o APS11450

_Ostateczna specyfikacja funkcjonalna_

## 1. Cel projektu

Celem projektu jest zastąpienie klasycznego układu elektrodowego układem wykorzystującym trzy czujniki Halla APS11450 oraz pływak z magnesem.

Dla zewnętrznego sterownika aplikacja ma zachowywać się identycznie jak trzy elektrody zanurzeniowe.

---

## 2. Architektura

```text
APS11450 x3
     │
     ▼
Sensor Manager
     │
     ▼
Level Resolver
     │
     ▼
Relay Manager
     │
     ▼
Sterownik zewnętrzny
```

### Sensor Manager

Odpowiada wyłącznie za sprzęt:

- odczyt wejść LOW/HIGH/SAFETY,
- odczyt ADC (diagnostyka APS11450),
- wykrywanie Internal Fault,
- wykrywanie External Fault.

Wyjście:

```text
SensorState
{
    low
    high
    safety
    lowFault
    highFault
    safetyFault
}
```

Nigdy nie interpretuje poziomu cieczy.

---

### Level Resolver

Jedyny moduł posiadający pamięć stanu.

Odpowiada za:

- określenie poziomu,
- inicjalizację po restarcie,
- filtrowanie stanów przejściowych,
- wykrywanie kierunku ruchu,
- diagnostykę logiczną,
- emulację elektrod.

---

### Relay Manager

Wyłącznie mapuje poziom na wyjścia przekaźników.

---

## 3. Poziomy systemu

| Stan | R1 | R2 | R3 |
| --- | --- | --- | --- |
| EMPTY | 0 | 0 | 0 |
| LEVEL1 | 1 | 0 | 0 |
| LEVEL2 | 1 | 1 | 0 |
| LEVEL3 | 1 | 1 | 1 |
| FAULT | zgodnie z konfiguracją bezpieczeństwa | | |

---

## 4. Wirtualne elektrody

| Poziom | E1 | E2 | E3 |
| --- | --- | --- | --- |
| EMPTY | 0 | 0 | 0 |
| LEVEL1 | 1 | 0 | 0 |
| LEVEL2 | 1 | 1 | 0 |
| LEVEL3 | 1 | 1 | 1 |

Relay Manager korzysta wyłącznie z E1-E3.

---

## 5. Automat stanów

```text
UNKNOWN
   │
   ▼
EMPTY <-> LEVEL1 <-> LEVEL2 <-> LEVEL3

Każdy stan -> FAULT
FAULT -> UNKNOWN
```

---

## 6. Restart

Po restarcie:

1. Sensor Manager wykonuje diagnostykę.
2. Przez określony czas zbierane są próbki.
3. Level Resolver wybiera najbardziej prawdopodobny poziom.
4. Aktualizowane są E1-E3.
5. Relay Manager ustawia przekaźniki.

Do czasu zakończenia inicjalizacji zaleca się utrzymanie poprzedniego stanu wyjść.

---

## 7. Stany przejściowe

Podczas przejazdu magnesu mogą pojawić się kombinacje nieodpowiadające rzeczywistemu poziomowi.

Nie powodują one natychmiastowej zmiany poziomu.

Level Resolver:

- wykorzystuje poprzedni poziom,
- oczekuje stabilizacji,
- dopiero wtedy zatwierdza nowy poziom.

---

## 8. Diagnostyka

### Sprzętowa

- Internal Fault APS11450
- External Fault APS11450
- błędy ADC

### Logiczna

- niedozwolona sekwencja,
- przeskok poziomu,
- zbyt długi stan przejściowy,
- utrata synchronizacji,
- niespójne stany.

---

## 9. Tabela przejść

| Aktualny stan | Potwierdzone zdarzenie | Nowy stan | Akcja |
| --- | --- | --- | --- |
| UNKNOWN | INIT_COMPLETE + EMPTY | EMPTY | R=000 |
| UNKNOWN | INIT_COMPLETE + LEVEL1 | LEVEL1 | R=100 |
| UNKNOWN | INIT_COMPLETE + LEVEL2 | LEVEL2 | R=110 |
| UNKNOWN | INIT_COMPLETE + LEVEL3 | LEVEL3 | R=111 |
| EMPTY | LEVEL1 potwierdzony | LEVEL1 | Włącz R1 |
| LEVEL1 | LEVEL2 potwierdzony | LEVEL2 | Włącz R2 |
| LEVEL2 | LEVEL3 potwierdzony | LEVEL3 | Włącz R3 |
| LEVEL3 | LEVEL2 potwierdzony | LEVEL2 | Wyłącz R3 |
| LEVEL2 | LEVEL1 potwierdzony | LEVEL1 | Wyłącz R2 |
| LEVEL1 | EMPTY potwierdzony | EMPTY | Wyłącz R1 |
| Dowolny | Internal/External Fault | FAULT | Alarm |

---

## 10. Niedozwolone kombinacje

Surowe odczyty:

```text
010
001
101
011
```

nie powodują automatycznie zmiany poziomu.

Mogą oznaczać:

- ruch pływaka,
- restart,
- uszkodzenie czujnika,
- uszkodzenie okablowania.

Ich interpretacja zależy od historii i diagnostyki.

---

## 11. Zasady implementacyjne

- Przekaźniki nigdy nie są sterowane bezpośrednio z czujników.
- Jedynym źródłem informacji o poziomie jest Level Resolver.
- Sensor Manager nie zna logiki procesu.
- Relay Manager nie odczytuje czujników.
- Wszystkie zmiany poziomu są logowane.
- Wszystkie błędy posiadają własny kod diagnostyczny.
- System musi być odporny na restart oraz stany przejściowe.

---

## 12. Skalowalność

Dodanie kolejnych poziomów wymaga:

- dodania czujnika,
- rozszerzenia konfiguracji Level Resolver,
- rozszerzenia tabeli przejść.

Pozostałe moduły pozostają bez zmian.

---

## 13. Główna zasada projektowa

Czujniki APS11450 nie sterują przekaźnikami.

Czujniki dostarczają wyłącznie dane pomiarowe.

Level Resolver przekształca dane w logiczny stan poziomu.

Relay Manager generuje sygnały identyczne z klasycznym układem elektrodowym.

Dzięki temu aplikacja jest odporna na:

- przejazd magnesu pomiędzy czujnikami,
- restart urządzenia,
- błędy pojedynczych czujników,
- zmianę technologii czujników w przyszłości.