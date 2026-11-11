#pragma once
#include <Arduino.h>

enum ButtonEvent {
  BTN_EVENT_NONE = 0,
  BTN_EVENT_MODE_NEXT,    // Button 1 (GP8):  Cycle Mode Forward
  BTN_EVENT_MODE_PREV,    // Button 2 (GP9):  Cycle Mode Backward
  BTN_EVENT_BRIGHT_UP,    // Button 3 (GP10): Brighten Ambient (Door Closed)
  BTN_EVENT_BRIGHT_DOWN   // Button 4 (GP11): Darken Ambient (Door Closed)
};

void buttonManagerInit();
using ButtonEvents = uint8_t;
constexpr ButtonEvents buttonEventBit(ButtonEvent event) { return event == BTN_EVENT_NONE ? 0 : (1u << (event - 1)); }
// Independent presses survive; simultaneous opposing pairs cancel.
ButtonEvents buttonManagerUpdate();
bool getDoorSwitchAggregateOpen();