# TMAG5124B -- Uwagi projektowe i uzupełnienia

## 1. Warianty serii TMAG512x

Opis dotyczy wariantu **TMAG5124B**, w którym:

-   **magnes blisko czujnika** → pobór prądu około **3,5 mA**
-   **magnes daleko od czujnika** → pobór prądu około **14,5 mA**

Warto pamiętać, że rodzina TMAG512x zawiera również inne wersje, w
których logika przełączania może być odwrotna. Dlatego zawsze należy
sprawdzić dokumentację konkretnego wariantu.

------------------------------------------------------------------------

## 2. Progi przełączania

W dokumentacji najczęściej podawane są wartości typowe:

-   **BOP (Point of Operation)** ≈ 6 mT
-   **BRP (Point of Release)** ≈ 3 mT

Są to wartości **typowe**, a nie gwarantowane.

Projektując urządzenie przemysłowe należy uwzględnić również wartości
minimalne i maksymalne podane w dokumentacji producenta.

------------------------------------------------------------------------

## 3. Dobór rezystora pomiarowego

Przykład dla rezystora **100 Ω**:

  Stan                    Prąd   Napięcie
  ------------------ --------- ----------
  Magnes obecny         3,5 mA     0,35 V
  Magnes nieobecny     14,5 mA     1,45 V

Różnica napięć wynosi **1,10 V**, co zapewnia bardzo pewny odczyt.

### Uwaga

Rezystor pomiarowy nie może mieć zbyt dużej wartości.

Przykład dla **470 Ω**:

-   3,5 mA → 1,65 V
-   14,5 mA → 6,82 V

Przy zasilaniu 5 V drugi stan nie będzie możliwy do osiągnięcia,
ponieważ zabraknie napięcia na sam czujnik.

Rezystor należy dobrać tak, aby przy maksymalnym prądzie czujnik nadal
pracował w dopuszczalnym zakresie napięcia zasilania.

------------------------------------------------------------------------

## 4. Odczyt sygnału

Do odczytu nie jest wymagany przetwornik ADC.

Ponieważ występują jedynie dwa poziomy prądu, można wykorzystać:

-   komparator,
-   wejście z histerezą (Schmitt Trigger),
-   prosty tranzystor,
-   wejście cyfrowe mikrokontrolera.

ADC jest potrzebny jedynie wtedy, gdy planowana jest diagnostyka lub
monitorowanie wartości prądu.

------------------------------------------------------------------------

## 5. Diagnostyka przewodów

Pętla prądowa umożliwia wykrywanie uszkodzeń instalacji.

Najbardziej typowe przypadki:

-   **0 mA** -- przerwa przewodu,
-   **wartość spoza prawidłowych poziomów prądu** -- możliwe uszkodzenie
    przewodu lub zwarcie.

Nie należy zakładać, że każde zwarcie spowoduje wzrost prądu powyżej
14,5 mA, ponieważ zależy to od miejsca zwarcia i sposobu zasilania.

------------------------------------------------------------------------

## 6. Praca na długich przewodach

Jedną z największych zalet interfejsu prądowego jest odporność na
długość przewodu.

Przykład:

-   rezystancja przewodu: około 1 Ω,
-   prąd: 14,5 mA,

spadek napięcia:

14,5 mA × 1 Ω = **14,5 mV**

Jest to wartość praktycznie pomijalna.

------------------------------------------------------------------------

## 7. Odporność EMC

Kodowanie informacji za pomocą prądu zapewnia bardzo wysoką odporność na
zakłócenia elektromagnetyczne.

Najważniejsze zalety:

-   odporność na zakłócenia wspólne (Common Mode),
-   niewielki wpływ spadków napięcia na przewodach,
-   możliwość prowadzenia przewodów obok silników, styczników i
    falowników,
-   wysoka niezawodność w środowisku przemysłowym.

------------------------------------------------------------------------

# Rekomendacja projektowa

W aplikacjach wymagających jedynie informacji o stanie czujnika warto
zastosować komparator zamiast przetwornika ADC.

Przykładowy schemat:

``` text
+24 V
  |
TMAG5124B
  |
  +------ wejście komparatora
  |
Rsense
  |
 GND
```

Przykładowy próg komparatora:

**0,9 V**

Odczyt:

-   0,35 V → magnes obecny
-   1,45 V → magnes nieobecny

Takie rozwiązanie jest:

-   prostsze,
-   szybsze,
-   bardziej odporne na zakłócenia,
-   nie wymaga wykonywania pomiarów ADC.

Jeżeli wymagane jest wykrywanie uszkodzeń przewodów, można pozostawić
pomiar ADC lub zastosować dodatkowy układ progowy.
