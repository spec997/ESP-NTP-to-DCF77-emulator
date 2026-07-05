#include "dcf77_encoder.h"

bool dcfFrame[60];

void encodeBCD(int value, int startBit, int length) {
  int bcd = ((value / 10) << 4) + (value % 10);
  for (int i = 0; i < length; i++) {
    dcfFrame[startBit + i] = (bcd >> i) & 1;
  }
}

bool calculateParity(int startBit, int endBit) {
  bool parity = false;
  for (int i = startBit; i <= endBit; i++) {
    parity ^= dcfFrame[i];
  }
  return parity;
}

// Przygotowuje ramkę bitową DCF77 dla NADCHODZĄCEJ minuty (zgodnie ze
// specyfikacją - ramka jest nadawana w trakcie minuty POPRZEDZAJĄCEJ czas,
// który opisuje). Funkcja wywoływana jest w sekundzie 0 bieżącej minuty
// z argumentem "now + 60".
void prepareDCFFrame(struct tm* t) {
  for (int i = 0; i < 60; i++) dcfFrame[i] = false;

  // Bit 0: zawsze 0 (znacznik początku minuty - bez impulsu w sekundzie 59)
  // Bity 1-14: zarezerwowane / ostrzeżenia pogodowe - nieużywane (0)
  // Bity 15: bit wywołania (call bit) - nieużywany (0)
  // Bit 16: zapowiedź zmiany czasu letni/zimowy - nieobsługiwana w tej wersji (0)

  // Bity 17-18: aktualnie obowiązujący czas (dokładnie jeden z nich = 1)
  if (t->tm_isdst > 0) {
    dcfFrame[17] = true;  dcfFrame[18] = false;
  } else {
    dcfFrame[17] = false; dcfFrame[18] = true;
  }

  // Bit 19: zapowiedź sekundy przestępnej - nieobsługiwana (0)
  // Bit 20: zawsze 1 - początek zakodowanego czasu
  dcfFrame[20] = true;

  // Bity 21-27: minuty w BCD, bit 28: parzystość
  encodeBCD(t->tm_min, 21, 7);
  dcfFrame[28] = calculateParity(21, 27);

  // Bity 29-34: godziny w BCD, bit 35: parzystość
  encodeBCD(t->tm_hour, 29, 6);
  dcfFrame[35] = calculateParity(29, 34);

  // Bity 36-41: dzień miesiąca w BCD
  encodeBCD(t->tm_mday, 36, 6);

  // Bity 42-44: dzień tygodnia w BCD (1=poniedziałek ... 7=niedziela)
  int wday = t->tm_wday == 0 ? 7 : t->tm_wday;
  encodeBCD(wday, 42, 3);

  // Bity 45-49: miesiąc w BCD
  encodeBCD(t->tm_mon + 1, 45, 5);

  // Bity 50-57: rok (dwie ostatnie cyfry) w BCD
  encodeBCD(t->tm_year % 100, 50, 8);

  // Bit 58: parzystość dla bloku daty (bity 36-57)
  dcfFrame[58] = calculateParity(36, 57);

  // Bit 59: brak impulsu - znacznik końca minuty
}
