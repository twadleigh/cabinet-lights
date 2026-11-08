#include "door_engine.h"
#include "config.h"
#include "ota_manager.h"

static DoorState systemDoorState = DOOR_STATE_CLOSED;

static float doorProgress = 0.0f; // 0.0 = fully ambient, 1.0 = fully photopic task white
static float rampStartProgress = 0.0f;
static unsigned long lastDoorTransitionTime = 0;

static ControllerMode currentMode = MODE_NORMAL;

// 13-step low-end-weighted perceptual brightness lookup table (clean integer percentages)
// Paired with PWM_PEDESTAL_TICKS = 20 and PWM_MIN_PULSE_TICKS = 30:
//   Step 0  (0%):     0.0 ticks (Strictly Off)
//   Step 1  (4%):    ~33% duty of 30-tick matched pulse (1 kHz 0 <-> 30 dither)
//   Step 2  (5%):    ~56% duty of 30-tick matched pulse (1 kHz 0 <-> 30 dither)
//   Step 3  (7%):    32.6 ticks (continuous >= 30-tick matched dark-room floor)
//   Step 4  (9%):    43.1 ticks
//   Step 5  (12%):   66.2 ticks
//   Step 6  (16%):  112.2 ticks
//   Step 7  (22%):  218.0 ticks
//   Step 8  (30%):  436.0 ticks
//   Step 9  (40%):  850.0 ticks
//   Step 10 (55%): 1801.0 ticks (Default startup level)
//   Step 11 (75%): 3771.0 ticks
//   Step 12 (100%): 7500.0 ticks
static constexpr uint8_t BRIGHTNESS_STEP_COUNT = 13;
static constexpr float BRIGHTNESS_TABLE[BRIGHTNESS_STEP_COUNT] = {
  0.00f, // Step 0:  0% (Off)
  0.04f, // Step 1:  4% (Sub-threshold 0 <-> 30 tick dither, ~33%)
  0.05f, // Step 2:  5% (Sub-threshold 0 <-> 30 tick dither, ~56%)
  0.07f, // Step 3:  7% (Continuous >30 ticks matched floor)
  0.09f, // Step 4:  9%
  0.12f, // Step 5:  12%
  0.16f, // Step 6:  16%
  0.22f, // Step 7:  22%
  0.30f, // Step 8:  30%
  0.40f, // Step 9:  40%
  0.55f, // Step 10: 55% (Default startup level)
  0.75f, // Step 11: 75%
  1.00f  // Step 12: 100%
};
static constexpr uint8_t DEFAULT_BRIGHTNESS_INDEX = 10; // 50.0%
static uint8_t brightnessIndex = DEFAULT_BRIGHTNESS_INDEX;
static float ambientBrightness = BRIGHTNESS_TABLE[DEFAULT_BRIGHTNESS_INDEX];

static float currentHue = 0.0f;
static float sourceHue = 0.0f;
static float targetHue = 0.0f;
static unsigned long lastHueSampleTime = 0;

static uint8_t debugChannelIndex = 0;
static unsigned long lastDebugStepTime = 0;

static const char* const DEBUG_CHANNEL_NAMES[8] = {
  "W1 (GP16)",
  "R1 (GP18)",
  "G1 (GP19)",
  "B1 (GP17)",
  "W2 (GP20)",
  "R2 (GP26)",
  "G2 (GP27)",
  "B2 (GP21)"
};

static const char* const DEBUG_MODE_LABELS[8] = {
  "DEBUG:W1",
  "DEBUG:R1",
  "DEBUG:G1",
  "DEBUG:B1",
  "DEBUG:W2",
  "DEBUG:R2",
  "DEBUG:G2",
  "DEBUG:B2"
};

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

static void applyModeChange(ControllerMode newMode, const char* directionLabel) {
  currentMode = newMode;
  char logBuf[112];
  if (currentMode == MODE_DEBUG) {
    debugChannelIndex = 0;
    lastDebugStepTime = millis();
    snprintf(logBuf, sizeof(logBuf),
             "[BTN] Mode %s -> DEBUG (8-Ch Max Test, starting at %s)",
             directionLabel, DEBUG_CHANNEL_NAMES[debugChannelIndex]);
    telemetryLogLine(logBuf);
  } else {
    lastHueSampleTime = millis();
    snprintf(logBuf, sizeof(logBuf),
             "[BTN] Mode %s -> NORMAL (Ambient Hue Walk + Door Task White)",
             directionLabel);
    telemetryLogLine(logBuf);
  }
}

void doorEngineInit(bool initialDoorOpen) {
  systemDoorState = initialDoorOpen ? DOOR_STATE_OPEN : DOOR_STATE_CLOSED;
  doorProgress = initialDoorOpen ? 1.0f : 0.0f;
  rampStartProgress = doorProgress;
  lastDoorTransitionTime = millis();
  lastHueSampleTime = millis();

  currentMode = MODE_NORMAL;
  brightnessIndex = DEFAULT_BRIGHTNESS_INDEX;
  ambientBrightness = BRIGHTNESS_TABLE[brightnessIndex];

  sourceHue = (float)random(0, 360);
  targetHue = (float)random(0, 360);
  currentHue = sourceHue;

  debugChannelIndex = 0;
  lastDebugStepTime = millis();
}

void doorEngineSetDoorState(bool isOpen) {
  DoorState newState = isOpen ? DOOR_STATE_OPEN : DOOR_STATE_CLOSED;
  if (newState != systemDoorState) {
    systemDoorState = newState;
    rampStartProgress = doorProgress; // Lock progress for smooth mid-fade reversals
    lastDoorTransitionTime = millis();
    telemetryLogLine(isOpen ? "[DOOR] Transition -> OPEN (Fading to 100% Task White)"
                            : "[DOOR] Transition -> CLOSED (Fading to Ambient)");
  }
}

void doorEngineHandleButton(ButtonEvent event) {
  char logBuf[112];
  switch (event) {
    case BTN_EVENT_MODE_NEXT: {
      ControllerMode nextMode = static_cast<ControllerMode>((currentMode + 1) % MODE_COUNT);
      applyModeChange(nextMode, "NEXT (Btn 1)");
      break;
    }

    case BTN_EVENT_MODE_PREV: {
      ControllerMode prevMode = static_cast<ControllerMode>((currentMode + MODE_COUNT - 1) % MODE_COUNT);
      applyModeChange(prevMode, "PREV (Btn 2)");
      break;
    }

    case BTN_EVENT_BRIGHT_UP: {
      if (brightnessIndex + 1 < BRIGHTNESS_STEP_COUNT) {
        brightnessIndex++;
      }
      ambientBrightness = BRIGHTNESS_TABLE[brightnessIndex];
      int briTenths = static_cast<int>(ambientBrightness * 1000.0f + 0.5f);
      snprintf(logBuf, sizeof(logBuf),
               "[BTN] Ambient Brightness UP (Btn 3) -> %d.%d%% (Step %u/%u)",
               briTenths / 10, briTenths % 10,
               static_cast<unsigned>(brightnessIndex),
               static_cast<unsigned>(BRIGHTNESS_STEP_COUNT - 1));
      telemetryLogLine(logBuf);
      break;
    }

    case BTN_EVENT_BRIGHT_DOWN: {
      if (brightnessIndex > 0) {
        brightnessIndex--;
      }
      ambientBrightness = BRIGHTNESS_TABLE[brightnessIndex];
      int briTenths = static_cast<int>(ambientBrightness * 1000.0f + 0.5f);
      snprintf(logBuf, sizeof(logBuf),
               "[BTN] Ambient Brightness DOWN (Btn 4) -> %d.%d%% (Step %u/%u)",
               briTenths / 10, briTenths % 10,
               static_cast<unsigned>(brightnessIndex),
               static_cast<unsigned>(BRIGHTNESS_STEP_COUNT - 1));
      telemetryLogLine(logBuf);
      break;
    }

    case BTN_EVENT_NONE:
    default:
      break;
  }
}

void doorEngineUpdate(StripColor& outS1, StripColor& outS2) {
  unsigned long now = millis();

  // 1. Always keep Normal-mode hue walk & door fade state advancing smoothly in background
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

  unsigned long elapsedDoor = now - lastDoorTransitionTime;
  float tDoor = (elapsedDoor >= DOOR_FADE_RAMP_MS) ? 1.0f : ((float)elapsedDoor / (float)DOOR_FADE_RAMP_MS);
  float targetProgress = (systemDoorState == DOOR_STATE_OPEN) ? 1.0f : 0.0f;
  doorProgress = rampStartProgress + (targetProgress - rampStartProgress) * tDoor;

  // 2. If in DEBUG mode, auto-cycle all 8 MOSFET channels at 100% max brightness
  if (currentMode == MODE_DEBUG) {
    if (now - lastDebugStepTime >= DEBUG_CHANNEL_HOLD_MS) {
      lastDebugStepTime = now;
      debugChannelIndex = (debugChannelIndex + 1) % 8;
      char logBuf[64];
      snprintf(logBuf, sizeof(logBuf), "[DEBUG] Channel -> %s @ 100%%",
               DEBUG_CHANNEL_NAMES[debugChannelIndex]);
      telemetryLogLine(logBuf);
    }

    outS1 = {0.0f, 0.0f, 0.0f, 0.0f};
    outS2 = {0.0f, 0.0f, 0.0f, 0.0f};

    switch (debugChannelIndex) {
      case 0: outS1.w = 1.0f; break;
      case 1: outS1.r = 1.0f; break;
      case 2: outS1.g = 1.0f; break;
      case 3: outS1.b = 1.0f; break;
      case 4: outS2.w = 1.0f; break;
      case 5: outS2.r = 1.0f; break;
      case 6: outS2.g = 1.0f; break;
      case 7: outS2.b = 1.0f; break;
      default: break;
    }
    return;
  }

  // 3. MODE_NORMAL: Compute ambient color @ ambientBrightness and cross-fade with fixed 100% Task White
  StripColor ambientColor = {0.0f, 0.0f, 0.0f, 0.0f};
  hueToRGB(currentHue, ambientBrightness, ambientColor.r, ambientColor.g, ambientColor.b);

  float u = constrain(doorProgress, 0.0f, 1.0f);
  StripColor activeColor;
  activeColor.w = (1.0f - u) * ambientColor.w + u * OPEN_PERCEPTUAL_W;
  activeColor.r = (1.0f - u) * ambientColor.r + u * OPEN_PERCEPTUAL_R;
  activeColor.g = (1.0f - u) * ambientColor.g + u * OPEN_PERCEPTUAL_G;
  activeColor.b = (1.0f - u) * ambientColor.b + u * OPEN_PERCEPTUAL_B;

  outS1 = activeColor;
  outS2 = activeColor;
}

float doorEngineGetAmbientBrightness() {
  return ambientBrightness;
}

ControllerMode doorEngineGetMode() {
  return currentMode;
}

const char* doorEngineGetModeLabel() {
  if (currentMode == MODE_DEBUG) {
    return DEBUG_MODE_LABELS[debugChannelIndex & 0x07];
  }
  return "NORMAL";
}
