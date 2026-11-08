#include "door_engine.h"
#include "config.h"

static DoorState systemDoorState = DOOR_STATE_CLOSED;
static bool taskOverrideActive = false;
static bool effectiveWhiteState = false; // True when door is OPEN or manual Task Override is active

static float doorProgress = 0.0f; // 0.0 = fully ambient, 1.0 = fully photopic task white
static float rampStartProgress = 0.0f;
static unsigned long lastDoorTransitionTime = 0;

static AmbientMode ambientMode = AMBIENT_MODE_HUE_WALK;
static float ambientBrightness = DIM_PERCEPTUAL;
static constexpr float BRIGHTNESS_STEP = 0.10f;
static constexpr float MIN_BRIGHTNESS  = 0.10f;
static constexpr float MAX_BRIGHTNESS  = 1.00f;

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

static void updateEffectiveWhiteTransition() {
  bool newEffective = (systemDoorState == DOOR_STATE_OPEN) || taskOverrideActive;
  if (newEffective != effectiveWhiteState) {
    effectiveWhiteState = newEffective;
    rampStartProgress = doorProgress; // Lock progress for smooth mid-fade reversals
    lastDoorTransitionTime = millis();
  }
}

void doorEngineInit(bool initialDoorOpen) {
  systemDoorState = initialDoorOpen ? DOOR_STATE_OPEN : DOOR_STATE_CLOSED;
  taskOverrideActive = false;
  effectiveWhiteState = initialDoorOpen;
  doorProgress = initialDoorOpen ? 1.0f : 0.0f;
  rampStartProgress = doorProgress;
  lastDoorTransitionTime = millis();
  lastHueSampleTime = millis();

  ambientMode = AMBIENT_MODE_HUE_WALK;
  ambientBrightness = DIM_PERCEPTUAL;

  sourceHue = (float)random(0, 360);
  targetHue = (float)random(0, 360);
  currentHue = sourceHue;
}

void doorEngineSetDoorState(bool isOpen) {
  DoorState newState = isOpen ? DOOR_STATE_OPEN : DOOR_STATE_CLOSED;
  if (newState != systemDoorState) {
    systemDoorState = newState;
    // Closing the physical door also clears any prior manual task override
    if (newState == DOOR_STATE_CLOSED) {
      taskOverrideActive = false;
    }
    updateEffectiveWhiteTransition();
    Serial.printf("[DOOR] Transition -> %s\n", isOpen ? "OPEN" : "CLOSED");
  }
}

void doorEngineHandleButton(ButtonEvent event) {
  switch (event) {
    case BTN_EVENT_TASK_TAP:
      taskOverrideActive = !taskOverrideActive;
      updateEffectiveWhiteTransition();
      Serial.printf("[BTN] Task White Override -> %s\n",
                    taskOverrideActive ? "ENABLED" : "DISABLED");
      break;

    case BTN_EVENT_MODE_TAP:
      if (taskOverrideActive) {
        taskOverrideActive = false;
        updateEffectiveWhiteTransition();
        Serial.println(F("[BTN] Mode Tap -> Exited Task Override to Ambient"));
      } else {
        ambientMode = static_cast<AmbientMode>((ambientMode + 1) % AMBIENT_MODE_COUNT);
        if (ambientMode == AMBIENT_MODE_HUE_WALK) {
          sourceHue = currentHue;
          targetHue = fmodf(currentHue + 60.0f + (float)random(0, 240), 360.0f);
          lastHueSampleTime = millis();
          Serial.println(F("[BTN] Ambient Mode -> HUE_WALK (30s random S^1 walk)"));
        } else if (ambientMode == AMBIENT_MODE_STATIC_HUE) {
          Serial.printf("[BTN] Ambient Mode -> STATIC_HUE (Locked at %.1f deg)\n", currentHue);
        } else {
          Serial.println(F("[BTN] Ambient Mode -> WARM_LOUNGE"));
        }
      }
      break;

    case BTN_EVENT_UP_TAP:
      ambientBrightness = constrain(ambientBrightness + BRIGHTNESS_STEP,
                                    MIN_BRIGHTNESS, MAX_BRIGHTNESS);
      Serial.printf("[BTN] Ambient Brightness UP -> %.0f%%\n", ambientBrightness * 100.0f);
      break;

    case BTN_EVENT_DOWN_TAP:
      ambientBrightness = constrain(ambientBrightness - BRIGHTNESS_STEP,
                                    MIN_BRIGHTNESS, MAX_BRIGHTNESS);
      Serial.printf("[BTN] Ambient Brightness DOWN -> %.0f%%\n", ambientBrightness * 100.0f);
      break;

    case BTN_EVENT_NONE:
    default:
      break;
  }
}

void doorEngineUpdate(StripColor& outS1, StripColor& outS2) {
  unsigned long now = millis();

  // 1. Compute ambient color based on active AmbientMode
  StripColor ambientColor = {0.0f, 0.0f, 0.0f, 0.0f};

  if (ambientMode == AMBIENT_MODE_HUE_WALK) {
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
    hueToRGB(currentHue, ambientBrightness, ambientColor.r, ambientColor.g, ambientColor.b);
  } else if (ambientMode == AMBIENT_MODE_STATIC_HUE) {
    hueToRGB(currentHue, ambientBrightness, ambientColor.r, ambientColor.g, ambientColor.b);
  } else {
    // AMBIENT_MODE_WARM_LOUNGE: Soft warm white + amber glow scaled by ambientBrightness
    ambientColor.w = ambientBrightness * 0.75f;
    ambientColor.r = ambientBrightness * 0.60f;
    ambientColor.g = ambientBrightness * 0.25f;
    ambientColor.b = 0.0f;
  }

  // 2. Compute door / task-override ramp progress u in [0.0, 1.0]
  unsigned long elapsedDoor = now - lastDoorTransitionTime;
  float tDoor = (elapsedDoor >= DOOR_FADE_RAMP_MS) ? 1.0f : ((float)elapsedDoor / (float)DOOR_FADE_RAMP_MS);
  float targetProgress = effectiveWhiteState ? 1.0f : 0.0f;
  doorProgress = rampStartProgress + (targetProgress - rampStartProgress) * tDoor;

  // 3. Linear cross-fade in normalized perceptual lightness space
  float u = constrain(doorProgress, 0.0f, 1.0f);
  StripColor activeColor;
  activeColor.w = (1.0f - u) * ambientColor.w + u * OPEN_PERCEPTUAL_W;
  activeColor.r = (1.0f - u) * ambientColor.r + u * OPEN_PERCEPTUAL_R;
  activeColor.g = (1.0f - u) * ambientColor.g + u * OPEN_PERCEPTUAL_G;
  activeColor.b = (1.0f - u) * ambientColor.b + u * OPEN_PERCEPTUAL_B;

  // Strips 1 and 2 currently track identical uniform color
  outS1 = activeColor;
  outS2 = activeColor;
}

bool doorEngineIsTaskOverrideActive() {
  return taskOverrideActive;
}

float doorEngineGetAmbientBrightness() {
  return ambientBrightness;
}

AmbientMode doorEngineGetAmbientMode() {
  return ambientMode;
}
