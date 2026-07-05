## [Nowa Wersja] - Wprowadzenie LittleFS, ElegantOTA i optymalizacje wydajności

### Nowości (Features)
* **Pamięć trwała ustawień (LittleFS):** Wprowadzono zapisywanie całej konfiguracji (sieć Wi-Fi, strefa czasowa, serwer NTP, hasła dostępowe) w nieulotnej pamięci flash (plik `/config.txt`). Ustawienia nie resetują się już po zaniku zasilania.
* **Aktualizacje OTA (Over-The-Air):** Zintegrowano bibliotekę `ElegantOTA`. Od teraz aktualizacja oprogramowania układowego (firmware) jest możliwa bezpośrednio z poziomu przeglądarki internetowej bez użycia kabla USB.

### Ulepszenia (Enhancements)
* **Optymalizacja pamięci RAM (Zabezpieczenie przed fragmentacją):** Generowanie statusu JSON w kontrolerze zoptymalizowano używając stałego bufora `char` oraz funkcji `snprintf`, eliminując problematyczne łączenie wielu obiektów klasy `String`.
* **Rozbudowane nagłówki HTTP HTTP Anti-Caching:** Dodano odpowiednie nagłówki i tagi meta zapobiegające niepożądanemu buforowaniu (cache) strony konfiguracyjnej przez przeglądarkę internetową.

### Poprawki błędów (Bugfixes)
* **Fix dla Safari / iOS:** Wprowadzono skrypt wymuszający odświeżenie strony po przejściu wstecz w przeglądarce, co rozwiązuje błąd z mechanizmem BFCache (wyświetlanie nieaktualnych danych systemowych).
* Drobne poprawki w kodzie HTML/CSS (dodanie brakującego tagu `lang="pl"`, nowe przyciski nawigacyjne).