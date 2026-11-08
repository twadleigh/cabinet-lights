#pragma once
#include <Arduino.h>

struct StripColor {
  float w, r, g, b; // Normalized [0.0, 1.0]
};

void ledDriverInit();
void setStripOutputs(const StripColor& s1, const StripColor& s2);
void setAllOutputsOff();