#pragma once
#include "led_driver.h"
#include "button_manager.h"

enum DoorState {
  DOOR_STATE_CLOSED = 0,
  DOOR_STATE_OPEN   = 1
};

enum ControllerMode : uint8_t {
  MODE_NORMAL = 0, // Door closed: 30s S^1 hue walk @ ambientBrightness; Door open: 5s fade to full Task White
  MODE_DEBUG  = 1, // Cycles all 8 MOSFET LED channels (W1->R1->G1->B1->W2->R2->G2->B2) at 100% brightness
  MODE_COUNT  = 2
};

void doorEngineInit(bool initialDoorOpen);
void doorEngineSetDoorState(bool isOpen);
void doorEngineHandleButton(ButtonEvent event);
void doorEngineUpdate(StripColor& outS1, StripColor& outS2);

float doorEngineGetAmbientBrightness();
ControllerMode doorEngineGetMode();
const char* doorEngineGetModeLabel();
