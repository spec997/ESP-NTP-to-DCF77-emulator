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

void prepareDCFFrame(struct tm* t) {
  for (int i = 0; i < 60; i++) dcfFrame[i] = false;
  
  if (t->tm_isdst > 0) {
    dcfFrame[17] = true;  dcfFrame[18] = false; 
  } else {
    dcfFrame[17] = false; dcfFrame[18] = true;  
  }
  
  dcfFrame[20] = true; 
  encodeBCD(t->tm_min, 21, 7);
  dcfFrame[28] = calculateParity(21, 27); 
  encodeBCD(t->tm_hour, 29, 6);
  dcfFrame[35] = calculateParity(29, 34); 
  encodeBCD(t->tm_mday, 36, 6);
  int wday = t->tm_wday == 0 ? 7 : t->tm_wday; 
  encodeBCD(wday, 42, 3);
  encodeBCD(t->tm_mon + 1, 45, 5); 
  encodeBCD(t->tm_year % 100, 50, 8);
  dcfFrame[58] = calculateParity(36, 57); 
}