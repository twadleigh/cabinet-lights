#pragma once
#include "led_driver.h"
#include "button_manager.h"

enum DoorState {
  DOOR_STATE_CLOSED = 0,
  DOOR_STATE_OPEN   = 1
};

enum AmbientMode : uint8_t {
  AMBIENT_MODE_HUE_WALK    = 0, // 30s continuous S^1 random hue walk
  AMBIENT_MODE_STATIC_HUE  = 1, // Holds current hue (tap Mode again to step +60 deg or exit)
  AMBIENT_MODE_WARM_LOUNGE = 2, // Soft warm white + amber blend
  AMBIENT_MODE_COUNT       = 3
};

void doorEngineInit(bool initialDoorOpen);
void doorEngineSetDoorState(bool isOpen);
void doorEngineHandleButton(ButtonEvent event);
void doorEngineUpdate(StripColor& outS1, StripColor& outS2);

bool doorEngineIsTaskOverrideActive();
float doorEngineGetAmbientBrightness();
AmbientMode doorEngineGetAmbientMode();
