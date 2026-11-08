#pragma once
#include "led_driver.h"

enum DoorState {
  DOOR_STATE_CLOSED = 0,
  DOOR_STATE_OPEN   = 1
};

void doorEngineInit(bool initialDoorOpen);
void doorEngineSetDoorState(bool isOpen);
void doorEngineUpdate(StripColor& outS1, StripColor& outS2);