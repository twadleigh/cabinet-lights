#pragma once
#include <Arduino.h>

enum ButtonEvent {
  BTN_EVENT_NONE = 0,
  BTN_EVENT_MODE_TAP,
  BTN_EVENT_UP_TAP,
  BTN_EVENT_DOWN_TAP,
  BTN_EVENT_TASK_TAP
};

void buttonManagerInit();
ButtonEvent buttonManagerUpdate();
bool getDoorSwitchAggregateOpen();