# Algorytm sterowania pompą v2
## Założenia systemu
Czujnik bez pamięć typu APS11450 lub Reed Switch. Reaguje na obecność magnesu w postaci zbocza narastającego a następnie wchodzi w stan stabilny wysoki H. Gdy magnes opuszcza pole wykrywania czujnika, czujnik generuje zbocze opadające a następnie wchodzi w stan stabilny niski L. Czujnik nie zapamiętuje faktu przejścia magnesu.
Poziom dolny Op Lev 2 i górny Op Lev 1, dwa czujniki magnetyczne
Odpowiednio dobrany magnes w pływaku, występuje nakładanie pola magnetycznego na oba czujniki w środkowym zakresie ruchu pływaka pomiędzy czujnikami.
Dodatkowo mechaniczna blokada pływaka na poziomie Op Lev 2, wymuszająca aktywacje czujnika tego poziomu. Pływak nie może opaść niżej.
 
## Charakterystyka stref
Strefa	Położenie pływaka	        Op Lev 1	Op Lev 2	Uwagi
1	Poniżej Op Lev 2	            L	        H	        Stan stabilny
2	W obszarze Op Lev 2	            L	        H	        Stan stabilny
	Przejście 2→3	                L→H	        H	        Zbocze Lev1 ↑
3	Pomiędzy Op Lev 2 i Op Lev 1   	H	        H	        Stan stabilny
	Przejście 3→4	                H	        H→L	        Zbocze Lev2 ↓
4	W obszarze Op Lev 1	            H	        L	        Stan stabilny
	Przejście 4→5	                H→L	        L	        Zbocze Lev1 ↓
5	Powyżej Op Lev 1	            L	        L	        Stan stabilny

Wszystkie strefy z wyjątkiem 1 i 2 są wyraźnie rozróżnialne na podstawie stanu czujników.
Tabela opisuje ruch pływaka do góry.

## Dopuszczalna sekwencja zmian stanów
Op Lev 1 / Op Lev 2
Ruch pływaka w górę
L/H → H/H → H/L → L/L
Ruch pływaka w dół 
L/L → H/L → H/H → L/H

## Scenariusz 1 – normalna praca po włączeniu
1. Poziom wody podnosi na pozycje Op Lev 2
2. Pływak na pozycji Op Lev 2 zablokowany mechanicznie, system widzi stan H na czujniku Lev 2
3. Poziom wody podnosi się dalej
4. Pływak na pozycji między Op Lev 2 i Op Lev 1
5. Aktywuje się Lev 1 → system wykrywa zbocze narastające na Lev 1(`L → H`)
6. Stan czujnika Lev 2 pozostaje bez zmian na H 
7. Poziom wody podnosi się dalej
8. Pływak na pozycji Op Lev 1
9. Deaktywuje się Lev 2 → system wykrywa zbocze opadające na Lev 2 (`H → L`)
10. Stan czujnika Lev 1 pozostaje bez zmian na H
11. Załączenie pompy - ON
12. Poziom wody zaczyna opadać
13. Pływak na pozycji między Op Lev 2 i Op Lev 1
14. Aktywuje się Lev 2 → system wykrywa zbocze narastające na Lev 2 (`L → H`)
15. Stan czujnika Lev 1 pozostaje bez zmian na H
16. Poziom wody opada dalej
17. Pływak na pozycji Op Lev 2 zablokowany mechanicznie
18. Deaktywuje się Lev 1 → system wykrywa zbocze opadające na Lev 1(`H → L`)
19. Stan czujnika Lev 2 pozostaje bez zmian na H
20. Wyłączenie pompy - OFF
21. Poziom wody ponownie rośnie (kondensacja).
System pracuje cyklicznie pomiędzy `Op Lev 2` i `Op Lev 1`.


### Kluczowe zdarzenia sterujące
1. **Załączenie pompy** na zboczu opadającym z `Op Lev 2`.
2. **Wyłączenie pompy** na zboczu opadającym z `Op Lev 1`.
Na podstawie stanu czujników wyraźnie rozróżnialnych dla wszystkich stref z wyjątkiem 1 i 2 system wyznacza położenie pływaka i poziom wody.

## Scenariusz 2 – restart (intencjonalny lub losowy)

Po ponownym uruchomieniu poziom wody może znajdować się w dowolnym położeniu.
- Położenie pływaka może zostać wyznaczone na podstawie aktualnych stanów czujników, o ile sygnały są poprawne i jednoznaczne.
- System oczekuje na zdarzenie (zbocze) z czujnika `Op Lev 1` lub `Op Lev 2`.

## Zagrożenia

1. **Poziom wody znacznie powyżej `Op Lev 1`**  
Akcja: 	pompa powinna zostać załączona do momentu wystąpienia zdarzenia zatrzymującego pompowanie na poziomie Op Lev 2

2. **Uszkodzenie `Op Lev 1`**  
Skutek: 	pompa po załączeniu, może nie zostać wyłączona → ryzyko przegrzania pompy (ograniczenie czasowe działania pompy, detekcja uszkodzenia czujnika)

3. **Uszkodzenie `Op Lev 2`**  
Skutek: 	pompa może nie zostać włączona → ryzyko przepełnienia

4. **Brak zmiany stanu czujników przez zbyt długi czas podczas pracy pompy**  
Możliwe przyczyny: 	zablokowany pływak, 
uszkodzona pompa, 
brak kondensatu, 
odpadnięty magnes.

5. **Zakłócenia elektromagnetyczne (EM)**  
Chwilowe zmiany stanów czujników powinny zostać wykryte i odfiltrowane przez system. System powinien odrzucać sekwencje stanów i zboczy niezgodne z dopuszczalną sekwencją zmian stanów. 

## Obsługa zagrożeń

### Zagrożenie 1

Dzięki mechanicznej blokadzie pływaka oraz odpowiednio dobranemu magnesowi kombinacja stanów `Op Lev 1 = L` oraz `Op Lev 2 = L` może wystąpić wyłącznie dla strefy `5` (powyżej `Op Lev 1`).
System interpretuje ten stan jako przepełnienie zbiornika i:

- załącza pompę,
- prowadzi pompowanie do momentu osiągnięcia warunku wyłączenia na poziomie `Op Lev 2`.

Nie jest wymagana procedura inicjalizacji z wymuszonym pompowaniem.

### Zagrożenia 2 i 3

- Dla APS11450 możliwa jest diagnostyka czujnika i zgłoszenie błędu.
- Dla Reed Switch brak diagnostyki własnej, dlatego wykrywanie uszkodzeń opiera się na analizie sekwencji stanów i czasie przejść pomiędzy strefami.

### Zagrożenie 4

Chwilowe zmiany stanów czujników powinny być filtrowane.
System powinien:

- odrzucać krótkotrwałe zakłócenia,
- odrzucać sekwencje stanów i zboczy niezgodne z dopuszczalną kolejnością zmian stanów,
- zgłaszać błąd w przypadku trwałego naruszenia logiki działania.

Do analizy wykorzystywane są:

- aktualne stany czujników,
- zbocza narastające,
- zbocza opadające.

## Koncepcja algorytmu (FSM)

Algorytm sterowania można zaimplementować jako automat skończony (FSM), którego zadaniem jest:

- wyznaczanie aktualnej strefy położenia pływaka na podstawie stanów czujników,
- weryfikacja poprawności przejść pomiędzy strefami na podstawie wykrywanych zboczy,
- sterowanie pompą poprzez wykonywanie akcji przypisanych do określonych przejść automatu.

Stany stabilne automatu odpowiadają kolejnym strefom położenia pływaka, natomiast zbocza stanowią zdarzenia powodujące przejścia pomiędzy tymi stanami.

Akcje sterujące:

- załączenie pompy – zbocze opadające `Op Lev 2`,
- wyłączenie pompy – zbocze opadające `Op Lev 1`.

Dzięki takiej architekturze bieżące położenie pływaka określane jest na podstawie aktualnych stanów czujników, natomiast poprawność działania układu nadzorowana jest przez automat FSM, który akceptuje wyłącznie dopuszczalne sekwencje zmian stanów.

## Przykładowe stany FSM

- `LEVEL_OPLEV2`
- `BETWEEN_LEVELS`
- `LEVEL_OPLEV1`
- `ABOVE_OPLEV1`
- `FAULT`

## Diagnostyka

System powinien wykrywać nieprawidłowości zarówno na podstawie czasu przejść pomiędzy strefami, jak i poprawności sekwencji stanów.
Do wykrywanych usterek należą:

- zablokowany pływak,
- brak kondensatu podczas pracy pompy,
- uszkodzona pompa,
- odpadnięty magnes,
- uszkodzony czujnik,
- sekwencja stanów niezgodna z dopuszczalnym modelem ruchu pływaka.

W przypadku wykrycia niedozwolonej sekwencji stanów lub przekroczenia dopuszczalnego czasu przejścia pomiędzy strefami sterownik powinien przejść do stanu `FAULT` i zgłosić błąd.

## Przy nowych założeniach, w stosunku do v1, zostały usunięte

- sekcja „Sekwencja startowa (INIT)”,
- procedura wymuszonego pompowania,
- sekcja „Ograniczenie informacyjne układu”.

W poprzedniej wersji były one konieczne, ponieważ dwa czujniki bez pamięci nie pozwalały odtworzyć położenia pływaka po restarcie.
W obecnej architekturze, dzięki blokadzie mechanicznej oraz nakładaniu pól magnetycznych, wszystkie istotne stany (w tym `L/L` jako strefa `5`) są jednoznacznie interpretowalne, więc problem ten został wyeliminowany.

