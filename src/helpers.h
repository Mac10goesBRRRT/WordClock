#ifndef HELPERS_H
#define HELPERS_H

// Include standard Arduino libraries if necessary
#include <Arduino.h>
#include "LittleFS.h"

// File Writing
void fileWriteData(String data, String filename);
String readFileToString(String filename);

//Simulation
void simulateDisplayOutput(bool ledmatrix[], String front, int min);

#endif // HELPERS_H