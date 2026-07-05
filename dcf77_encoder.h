#ifndef DCF77_ENCODER_H
#define DCF77_ENCODER_H

#include <time.h>
#include <Arduino.h>

extern bool dcfFrame[60];

void encodeBCD(int value, int startBit, int length);
bool calculateParity(int startBit, int endBit);
void prepareDCFFrame(struct tm* t);

#endif