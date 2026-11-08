#include "door_engine.h"
#include "config.h"

static DoorState systemDoorState = DOOR_STATE_CLOSED;
static float doorProgress = 0.0f; // 0.0 = fully closed, 1.0 = fully open
static float rampStartProgress = 0.0f;
static unsigned long lastDoorTransitionTime = 0;

static float currentHue = 0.0f;
static float sourceHue = 0.0f;
static float targetHue = 0.0f;
static unsigned long lastHueSampleTime = 0;

static float shortestAngularDelta(float from, float to) {
  float diff = fmodf(to - from + 540.0f, 360.0f) - 180.0f;
  return diff;
}

static void hueToRGB(float h, float v, float &r, float &g, float &b) {
  float c = v;
  float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
  float m = 0.0f;

  if (h < 60.0f)       { r = c; g = x; b = 0; }
  else if (h < 120.0f) { r = x; g = c; b = 0; }
  else if (h < 180.0f) { r = 0; g = c; b = x; }
  else if (h < 240.0f) { r = 0; g = x; b = c; }
  else if (h < 300.0f) { r = x; g = 0; b = c; }
  else                 { r = c; g = 0; b = x; }

  r += m; g += m; b += m;
}

void doorEngineInit(bool initialDoorOpen) {
  systemDoorState = initialDoorOpen ? DOOR_STATE_OPEN : DOOR_STATE_CLOSED;
  doorProgress = initialDoorOpen ? 1.0f : 0.0f;
  rampStartProgress = doorProgress;
  lastDoorTransitionTime = millis();
  lastHueSampleTime = millis();

  sourceHue = (float)random(0, 360);
  targetHue = (float)random(0, 360);
  currentHue = sourceHue;
}

void doorEngineSetDoorState(bool isOpen) {
  DoorState newState = isOpen ? DOOR_STATE_OPEN : DOOR_STATE_CLOSED;
  if (newState != systemDoorState) {
    systemDoorState = newState;
    rampStartProgress = doorProgress; // Lock progress for smooth mid-fade reversals
    lastDoorTransitionTime = millis();
    Serial.printf("[DOOR] Transition -> %s\n", isOpen ? "OPEN" : "CLOSED");
  }
}

void doorEngineUpdate(StripColor& outS1, StripColor& outS2) {
  unsigned long now = millis();

  // 1. Advance the 30-second ambient hue walk
  unsigned long elapsedSinceSample = now - lastHueSampleTime;
  if (elapsedSinceSample >= HUE_CYCLE_PERIOD_MS) {
    sourceHue = targetHue;
    targetHue = (float)random(0, 360);
    lastHueSampleTime = now;
    elapsedSinceSample = 0;
  }

  float tHue = (float)elapsedSinceSample / (float)HUE_CYCLE_PERIOD_MS;
  if (tHue > 1.0f) tHue = 1.0f;

  float delta = shortestAngularDelta(sourceHue, targetHue);
  currentHue = fmodf(sourceHue + delta * tHue + 360.0f, 360.0f);

  StripColor ambientColor = {0.0f, 0.0f, 0.0f, 0.0f};
  hueToRGB(currentHue, DIM_PERCEPTUAL, ambientColor.r, ambientColor.g, ambientColor.b);

  // 2. Compute door ramp progress u in [0.0, 1.0]
  unsigned long elapsedDoor = now - lastDoorTransitionTime;
  float tDoor = (elapsedDoor >= DOOR_FADE_RAMP_MS) ? 1.0f : ((float)elapsedDoor / (float)DOOR_FADE_RAMP_MS);
  float targetProgress = (systemDoorState == DOOR_STATE_OPEN) ? 1.0f : 0.0f;
  doorProgress = rampStartProgress + (targetProgress - rampStartProgress) * tDoor;

  // 3. Linear cross-fade in normalized perceptual lightness space
  float u = constrain(doorProgress, 0.0f, 1.0f);
  StripColor activeColor;
  activeColor.w = u * OPEN_PERCEPTUAL_W;
  activeColor.r = (1.0f - u) * ambientColor.r + u * OPEN_PERCEPTUAL_R;
  activeColor.g = (1.0f - u) * ambientColor.g + u * OPEN_PERCEPTUAL_G;
  activeColor.b = (1.0f - u) * ambientColor.b + u * OPEN_PERCEPTUAL_B;

  // Strips 1 and 2 currently track identical uniform color
  outS1 = activeColor;
  outS2 = activeColor;
}