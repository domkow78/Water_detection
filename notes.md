# Kroki rozwojowe

1. Implementacja na płytce mikrokontrolera sterującego kluczami MOS symulującymi elektrody, z rezystorem 1 kΩ.
   - W ten sposób pozbywamy się układu pośredniczącego.

2. Dodatkowa implementacja źródła prądowego sterowanego z mikrokontrolera, które będzie komunikować się z PU za pomocą pętli prądowej o 4 różnych poziomach prądu.
   - Zaleta: dwa przewody.
   - Sygnal analogowy prądowy.

3. Element pośredni między Smart Bus a układem z tranzystorami MOS, którego obecnie nie mamy.

4. Kolejny krok: implementacja układu Smart Bus.
   - Bezpośrednia komunikacja z masterem Smart Bus, gdy będzie dostępny.

5. Krok dodatkowy: komunikacja D-Bus z chipem CAN.
   - Jako zabezpieczenie na wypadek braku dostępności poprzednich rozwiązań.

> W każdym przypadku na płytce czujników mamy mikrokontroler MSPM0C1104 TI oraz dodatkowy układ kodujący stan czujników.

