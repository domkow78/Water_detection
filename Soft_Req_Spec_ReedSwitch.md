# Sterownik poziomu wody oparty o kontaktrony (Reed Switch)

_Specyfikacja funkcjonalna_

## 1. Cel projektu

Celem projektu jest realizacja układu pomiaru poziomu cieczy przy użyciu trzech kontaktronów oraz pływaka z magnesem.

Dla zewnętrznego sterownika aplikacja ma zachowywać się identycznie jak klasyczny układ trzech elektrod zanurzeniowych.

---

## 2. Architektura

```text
Reed Switch x3
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

Odpowiada wyłącznie za warstwę sprzętową:

- odczyt wejść LOW/HIGH/SAFETY,
- podstawową filtrację drgań styków (debounce),
- publikację aktualnych stanów wejść.

Wyjście:

```text
SensorState
{
    low
    high
    safety
}
```

Sensor Manager nigdy nie interpretuje poziomu cieczy.

---

### Level Resolver

Jedyny moduł posiadający pamięć stanu.

Odpowiada za:

- określenie poziomu,
- inicjalizację po restarcie,
- filtrowanie stanów przejściowych,
- wykrywanie kierunku ruchu pływaka,
- emulację elektrod.

---

### Relay Manager

Wyłącznie mapuje poziom logiczny na wyjścia przekaźników.

---

## 3. Poziomy systemu

| Stan | R1 | R2 | R3 |
| --- | --- | --- | --- |
| EMPTY | 0 | 0 | 0 |
| LEVEL1 | 1 | 0 | 0 |
| LEVEL2 | 1 | 1 | 0 |
| LEVEL3 | 1 | 1 | 1 |

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
```

Zmiany stanu są możliwe wyłącznie między sąsiednimi poziomami.

---

## 6. Restart

Po restarcie:

1. Sensor Manager rozpoczyna odczyt wejść i debounce.
2. Przez określony czas zbierane są próbki stanu wejść.
3. Level Resolver wybiera najbardziej prawdopodobny poziom startowy.
4. Aktualizowane są E1-E3.
5. Relay Manager ustawia przekaźniki zgodnie z wybranym poziomem.

Do czasu zakończenia inicjalizacji zaleca się utrzymanie poprzedniego stanu wyjść.

---

## 7. Stany przejściowe

Podczas ruchu pływaka mogą występować chwilowe kombinacje stanów wejść, które nie odpowiadają stabilnemu poziomowi.

Takie stany nie powodują natychmiastowej zmiany poziomu.

Level Resolver:

- wykorzystuje poprzedni poziom,
- oczekuje stabilizacji wejść przez zdefiniowany czas,
- dopiero wtedy zatwierdza nowy poziom.

---

## 8. Tabela przejść

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

---

## 9. Kombinacje niestabilne

Surowe odczyty:

```text
010
001
101
011
```

traktowane są jako stany przejściowe i nie powodują bezpośredniej zmiany poziomu.

Ich interpretacja zależy wyłącznie od historii stanów oraz czasu stabilizacji.

---

## 10. Zasady implementacyjne

- Przekaźniki nigdy nie są sterowane bezpośrednio z kontaktronów.
- Jedynym źródłem informacji o poziomie jest Level Resolver.
- Sensor Manager nie zawiera logiki procesu.
- Relay Manager nie odczytuje czujników.
- Wszystkie zmiany poziomu powinny być logowane.
- System musi być odporny na restart oraz chwilowe stany przejściowe.

---

## 11. Skalowalność

Dodanie kolejnych poziomów wymaga:

- dodania czujnika,
- rozszerzenia konfiguracji Level Resolver,
- rozszerzenia tabeli przejść.

Pozostałe moduły pozostają bez zmian.

---

## 12. Główna zasada projektowa

Kontaktrony nie sterują bezpośrednio przekaźnikami.

Kontaktrony dostarczają wyłącznie sygnały wejściowe.

Level Resolver przekształca sygnały w logiczny stan poziomu.

Relay Manager generuje sygnały identyczne z klasycznym układem elektrod zanurzeniowych.

Dzięki temu aplikacja zachowuje zgodność funkcjonalną z układem elektrodowym i zapewnia stabilną pracę podczas ruchu pływaka oraz po restarcie.
