# Algorytm sterowania pompą

## Założenia systemu

Czujnik bez pamięci typu **APS11450** lub **Reed Switch** reaguje na obecność magnesu:

- pojawia się zbocze narastające, a następnie stan stabilny wysoki (`H`),
- po opuszczeniu pola detekcji pojawia się zbocze opadające, a następnie stan stabilny niski (`L`).

Czujnik nie zapamiętuje faktu przejścia magnesu.

W układzie są dwa poziomy i dwa czujniki magnetyczne:

- **Op Lev 2** – poziom dolny,
- **Op Lev 1** – poziom górny.

Magnes w pływaku jest krótki, więc nie podtrzymuje stanu czujnika `Op Lev 2` podczas przejścia w kierunku `Op Lev 1`.

---

## Scenariusz 1 – normalna praca po włączeniu

1. Poziom wody w grupie dolnej (`Bottom`) podnosi się.
2. Aktywuje się `Op Lev 2` → system wykrywa zbocze narastające (`L → H`).
3. Pompa zostaje wyłączona (jeśli była wcześniej załączona).
4. Poziom wody rośnie dalej.
5. Deaktywuje się `Op Lev 2` → system wykrywa zbocze opadające (`H → L`).
6. Poziom wody rośnie dalej.
7. Aktywuje się `Op Lev 1` → system wykrywa zbocze narastające (`L → H`).
8. Pompa zostaje załączona.
9. Poziom wody zaczyna opadać.
10. Deaktywuje się `Op Lev 1` → system wykrywa zbocze opadające (`H → L`).
11. Poziom wody opada dalej.
12. Aktywuje się `Op Lev 2` → system wykrywa zbocze narastające (`L → H`).
13. Pompa zostaje wyłączona.
14. Poziom wody ponownie rośnie (kondensacja).

System pracuje cyklicznie pomiędzy `Op Lev 2` i `Op Lev 1`.

### Kluczowe zdarzenia sterujące

1. **Załączenie pompy** na zboczu narastającym z `Op Lev 1`.
2. **Wyłączenie pompy** na zboczu narastającym z `Op Lev 2`.

Na podstawie obserwacji zboczy i ich sekwencji system estymuje położenie pływaka i poziom wody.

---

## Scenariusz 2 – restart (intencjonalny lub losowy)

Po ponownym uruchomieniu poziom wody może znajdować się w dowolnym położeniu.

- System oczekuje na zdarzenie (zbocze) z czujnika `Op Lev 1` lub `Op Lev 2`.
- W momencie startu położenie pływaka i poziomu wody jest nieznane.
- Każde poprawne przejście pływaka przez czujnik (ze zboczem) aktualizuje estymowany stan układu.

---

## Zagrożenia

1. **Poziom wody znacznie powyżej `Op Lev 1`**  
   Magnes nie aktywuje żadnego czujnika, więc nie pojawi się zbocze narastające.  
   Skutek: pompa może nie zostać załączona → ryzyko przepełnienia grupy dolnej.

2. **Uszkodzenie `Op Lev 1`**  
   Skutek: pompa może nie zostać załączona → ryzyko przepełnienia grupy dolnej.

3. **Uszkodzenie `Op Lev 2`**  
   Skutek: pompa może nie zostać wyłączona → ryzyko przegrzania pompy.

4. **Zakłócenia elektromagnetyczne (EM)**  
   Chwilowe zmiany stanów czujników powinny zostać wykryte i odfiltrowane.

### Obsługa zagrożeń

#### Zagrożenie 1

Tego przypadku nie da się bezpośrednio zdiagnozować bez procedury inicjalizacyjnej. Dlatego należy zastosować **wymuszone pompowanie po starcie** aż do wykrycia aktywacji dowolnego czujnika.

Wymuszone pompowanie musi mieć ograniczenie czasowe.

Jeśli w zadanym oknie czasowym nie wystąpi aktywacja żadnego czujnika, system:

- zatrzymuje pompowanie,
- przyjmuje roboczo, że pływak jest poniżej `Op Lev 2`.

#### Zagrożenia 2 i 3

- Dla `APS11450` mogą być rozpoznane diagnostycznie i zgłoszone jako błąd.
- Dla `Reed Switch` brak diagnostyki własnej czujnika, więc zagrożenia pozostają.

#### Zagrożenie 4

Chwilowe zmiany stanów czujników powinny być filtrowane. Jeśli system zna aktualne położenie pływaka/wody, powinien:

- odrzucać stany niestabilne,
- odrzucać zdarzenia niezgodne z bieżącym stanem automatu `FSM`.

Do estymacji dostępne są:

- zbocza narastające,
- zbocza opadające,
- chwilowe stany stabilne wejść.

---

## Charakterystyka stref

| Strefa | Położenie pływaka                | Op Lev 1 | Op Lev 2 | Uwagi                                |
|:------:|----------------------------------|:--------:|:--------:|--------------------------------------|
| 1      | Poniżej `Op Lev 2`               | L        | L        | Stan stabilny                        |
| 2      | W obszarze `Op Lev 2`            | L        | H        | Stan przejściowy – generowane zbocza |
| 3      | Pomiędzy `Op Lev 2` i `Op Lev 1` | L        | L        | Stan stabilny                        |
| 4      | W obszarze `Op Lev 1`            | H        | L        | Stan przejściowy – generowane zbocza |
| 5      | Powyżej `Op Lev 1`               | L        | L        | Stan stabilny                        |

---

## Koncepcja algorytmu (`FSM`)

Algorytm sterowania można zaimplementować jako automat skończony (`FSM`), który:

- estymuje położenie pływaka na podstawie sekwencji zboczy z `Op Lev 1` i `Op Lev 2`,
- steruje pompą na podstawie przejść pomiędzy stanami.

Uwzględniane są dwa kierunki ruchu:

- wzrost poziomu,
- spadek poziomu.

Przejścia między stanami są wyzwalane wyłącznie przez poprawnie zweryfikowane zdarzenia.

Akcje sterujące:

- załączenie pompy: przejście przez `Op Lev 1` (zbocze narastające),
- wyłączenie pompy: przejście przez `Op Lev 2` (zbocze narastające).

Dzięki temu decyzje nie wynikają z chwilowego stanu wejść, tylko z logiki sekwencji zdarzeń i aktualnego stanu `FSM`.

### Przykładowe stany `FSM`

- `INIT`
- `BELOW_OPLEV2`
- `PASSING_OPLEV2`
- `BETWEEN_LEVELS`
- `PASSING_OPLEV1`
- `ABOVE_OPLEV1`
- `FAULT`

---

## Sekwencja startowa (`INIT`)

1. Po uruchomieniu sterownik przechodzi do stanu `INIT`.
2. Poziom wody jest nieznany, więc system nie przechodzi od razu do normalnej pracy.
3. Uruchamiana jest procedura rozpoznania położenia pływaka.

### Etap 1 – wymuszone pompowanie

- Załącz pompę.
- Uruchom licznik czasu (np. 10–30 s, zależnie od instalacji).
- Oczekuj na pierwsze poprawne zbocze narastające:
  - `Op Lev 1 ↑` → pływak osiągnął górny poziom, przejdź do właściwego stanu `FSM` i kontynuuj pracę.
  - `Op Lev 2 ↑` → pływak osiągnął dolny poziom, przejdź do właściwego stanu `FSM` i kontynuuj pracę.

### Etap 2 – timeout

Jeżeli w zadanym czasie nie wystąpi żadne zbocze:

- wyłącz pompę,
- przyjmij roboczo, że pływak jest poniżej `Op Lev 2`,
- przejdź do stanu odpowiadającego tej strefie,
- rozpocznij normalną pracę.

---

## Dodatkowa diagnostyka

Jeżeli podczas inicjalizacji pojawi się sekwencja niemożliwa (np. jednoczesna aktywacja obu czujników, jeśli konstrukcja tego nie dopuszcza, albo seria sprzecznych zboczy), sterownik powinien:

- odrzucić zdarzenie jako zakłócenie, lub
- przejść do stanu `FAULT` i zgłosić błąd.

Brak oczekiwanego zbocza w dopuszczalnym czasie podczas normalnej pracy należy interpretować jako awarię układu (pompa, czujnik, pływak lub instalacja).

Możliwe wykrywane usterki:

- zablokowany pływak,
- brak kondensatu,
- uszkodzona pompa,
- odpadnięty magnes,
- uszkodzony czujnik.

---

## Ograniczenie informacyjne układu

Jeżeli po restarcie pływak znajduje się już powyżej `Op Lev 1`, a pompa nie jest w stanie obniżyć poziomu wody, nie pojawi się żadne zbocze, bo magnes nie przejdzie przez obszar żadnego czujnika.

W takiej sytuacji procedura z timeoutem może błędnie uznać, że pływak jest poniżej `Op Lev 2`.

Tego przypadku **nie da się jednoznacznie rozpoznać**, używając wyłącznie dwóch czujników bez pamięci. To ograniczenie informacyjne układu, a nie wada algorytmu.