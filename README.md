# ESP8266 DCF77 Network Time Emulator

Emulator nadajnika radiowego **DCF77** oparty na układzie ESP8266 (np. Wemos D1 Mini). Urządzenie synchronizuje czas z serwerem NTP przez sieć Wi-Fi, a następnie generuje precyzyjną ramkę czasową na pinie GPIO w celu zsynchronizowania tradycyjnych zegarów ściennych lub budzików sterowanych radiowo.

## Funkcje projektu
* **Live Web-UI (AJAX):** Panel administracyjny odświeża czas, adresy IP i status sieci na żywo co sekundę (JavaScript) bez przeładowania strony.
* **Tryb Awaryjny (Captive AP):** Gdy ESP nie odnajdzie domowej sieci Wi-Fi, automatycznie uruchamia własny punkt dostępowy o nazwie `Televox_DCF77_Konfig`, umożliwiający rekonfigurację.
* **Pełna Zmiana Ustawień:** Możliwość zmiany serwerów NTP, stref czasowych (z uwzględnieniem czasu letniego/zimowego), haseł logowania panelu oraz hasła sieci AP bezpośrednio z poziomu WWW.
* **Bezpieczeństwo:** Dostęp do panelu chroniony protokołem HTTP Basic Authentication z funkcją natychmiastowego wylogowania.

## Struktura katalogów
Aby poprawnie skompilować szkic w Arduino IDE, upewnij się, że zachowałeś strukturę plików w jednym folderze:
```text
DCF77_Emulator/
├── DCF77_Emulator.ino
├── index_html.h
├── dcf77_encoder.h
└── dcf77_encoder.cpp