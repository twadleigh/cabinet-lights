#include "door_engine.h"
#include "config.h"
#include "ota_manager.h"

static DoorState systemDoorState = DOOR_STATE_CLOSED;

static float doorProgress = 0.0f; // 0.0 = fully ambient, 1.0 = fully photopic task white
static unsigned long lastDoorUpdateTime = 0;

static ControllerMode currentMode = MODE_NORMAL;

// 13-step low-end-weighted perceptual brightness lookup table (clean integer percentages)
// Paired with PWM_PEDESTAL_TICKS = 20 and PWM_MIN_PULSE_TICKS = 30:
//   Step 0  (0%):     0.0 ticks (Strictly Off)
//   Step 1  (4%):    ~33% duty of 30-tick matched pulse (20 kHz 0 <-> 30 dither)
//   Step 2  (5%):    ~56% duty of 30-tick matched pulse (20 kHz 0 <-> 30 dither)
//   Step 3  (7%):    32.6 ticks (continuous >= 30-tick matched dark-room floor)
//   Step 4  (9%):    43.1 ticks (Default startup level)
//   Step 5  (12%):   66.2 ticks
//   Step 6  (16%):  112.2 ticks
//   Step 7  (22%):  218.0 ticks
//   Step 8  (30%):  436.0 ticks
//   Step 9  (40%):  850.0 ticks
//   Step 10 (55%): 1801.0 ticks
//   Step 11 (75%): 3771.0 ticks
//   Step 12 (100%): 7500.0 ticks
static constexpr uint8_t BRIGHTNESS_STEP_COUNT = 13;
static constexpr float BRIGHTNESS_TABLE[BRIGHTNESS_STEP_COUNT] = {
  0.00f, // Step 0:  0% (Off)
  0.04f, // Step 1:  4% (Sub-threshold 0 <-> 30 tick dither, ~33%)
  0.05f, // Step 2:  5% (Sub-threshold 0 <-> 30 tick dither, ~56%)
  0.07f, // Step 3:  7% (Continuous >30 ticks matched floor)
  0.09f, // Step 4:  9% (Default startup level)
  0.12f, // Step 5:  12%
  0.16f, // Step 6:  16%
  0.22f, // Step 7:  22%
  0.30f, // Step 8:  30%
  0.40f, // Step 9:  40%
  0.55f, // Step 10: 55%
  0.75f, // Step 11: 75%
  1.00f  // Step 12: 100%
};
static constexpr uint8_t DEFAULT_BRIGHTNESS_INDEX = 4; // Level 4: 9% (level 0 is off)
static uint8_t brightnessIndex = DEFAULT_BRIGHTNESS_INDEX;
static float ambientBrightness = BRIGHTNESS_TABLE[DEFAULT_BRIGHTNESS_INDEX];

static float currentHue = 0.0f;
static float sourceHue = 0.0f;
static float targetHue = 0.0f;
static unsigned long lastHueSampleTime = 0;

static uint8_t debugChannelIndex = 0;
static unsigned long lastDebugStepTime = 0;

struct DebugChannel {
  const char* name;
  const char* label;
  uint8_t strip;
  float StripColor::*component;
};
static const DebugChannel DEBUG_CHANNELS[8] = {
  {"W1 (GP16)", "DEBUG:W1", 0, &StripColor::w},
  {"R1 (GP18)", "DEBUG:R1", 0, &StripColor::r},
  {"G1 (GP19)", "DEBUG:G1", 0, &StripColor::g},
  {"B1 (GP17)", "DEBUG:B1", 0, &StripColor::b},
  {"W2 (GP20)", "DEBUG:W2", 1, &StripColor::w},
  {"R2 (GP26)", "DEBUG:R2", 1, &StripColor::r},
  {"G2 (GP27)", "DEBUG:G2", 1, &StripColor::g},
  {"B2 (GP21)", "DEBUG:B2", 1, &StripColor::b}
};

static float shortestAngularDelta(float from, float to) {
  float diff = fmodf(to - from + 540.0f, 360.0f) - 180.0f;
  return diff;
}

static void hueToRGB(float h, float v, float &r, float &g, float &b) {
  float c = v;
  float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));

  if (h < 60.0f)       { r = c; g = x; b = 0; }
  else if (h < 120.0f) { r = x; g = c; b = 0; }
  else if (h < 180.0f) { r = 0; g = c; b = x; }
  else if (h < 240.0f) { r = 0; g = x; b = c; }
  else if (h < 300.0f) { r = x; g = 0; b = c; }
  else                 { r = c; g = 0; b = x; }

}

static void applyModeChange(ControllerMode newMode, const char* directionLabel) {
  currentMode = newMode;
  char logBuf[112];
  if (currentMode == MODE_DEBUG) {
    debugChannelIndex = 0;
    lastDebugStepTime = millis();
    snprintf(logBuf, sizeof(logBuf),
             "[BTN] Mode %s -> DEBUG (8-Ch Max Test, starting at %s)",
             directionLabel, DEBUG_CHANNELS[debugChannelIndex].name);
    telemetryLogLine(logBuf);
  } else {
    snprintf(logBuf, sizeof(logBuf),
             "[BTN] Mode %s -> NORMAL (Ambient Hue Walk + Door Task White)",
             directionLabel);
    telemetryLogLine(logBuf);
  }
}

void doorEngineInit(bool initialDoorOpen) {
  randomSeed(rp2040.hwrand32());
  systemDoorState = initialDoorOpen ? DOOR_STATE_OPEN : DOOR_STATE_CLOSED;
  doorProgress = initialDoorOpen ? 1.0f : 0.0f;
  lastDoorUpdateTime = millis();
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

static void advanceDoorProgress(unsigned long now) {
  float step = static_cast<float>(now - lastDoorUpdateTime) / DOOR_FADE_RAMP_MS;
  lastDoorUpdateTime = now;
  doorProgress = constrain(doorProgress + (systemDoorState == DOOR_STATE_OPEN ? step : -step), 0.0f, 1.0f);
}

void doorEngineSetDoorState(bool isOpen) {
  DoorState newState = isOpen ? DOOR_STATE_OPEN : DOOR_STATE_CLOSED;
  if (newState != systemDoorState) {
    advanceDoorProgress(millis());
    systemDoorState = newState;
    telemetryLogLine(isOpen ? "[DOOR] Transition -> OPEN (Fading to 100% Task White)"
                            : "[DOOR] Transition -> CLOSED (Fading to Ambient)");
  }
}

static void changeBrightness(bool up) {
  if (up && brightnessIndex + 1 < BRIGHTNESS_STEP_COUNT) ++brightnessIndex;
  if (!up && brightnessIndex > 0) --brightnessIndex;
  ambientBrightness = BRIGHTNESS_TABLE[brightnessIndex];
  int briTenths = static_cast<int>(ambientBrightness * 1000.0f + 0.5f);
  char logBuf[112];
  snprintf(logBuf, sizeof(logBuf),
           "[BTN] Ambient Brightness %s (Btn %u) -> %d.%d%% (Step %u/%u)",
           up ? "UP" : "DOWN", up ? 3u : 4u, briTenths / 10, briTenths % 10,
           static_cast<unsigned>(brightnessIndex), static_cast<unsigned>(BRIGHTNESS_STEP_COUNT - 1));
  telemetryLogLine(logBuf);
}

void doorEngineHandleButton(ButtonEvent event) {
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

    case BTN_EVENT_BRIGHT_UP:
      changeBrightness(true);
      break;
    case BTN_EVENT_BRIGHT_DOWN:
      changeBrightness(false);
      break;

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

  advanceDoorProgress(now);

  // 2. If in DEBUG mode, auto-cycle all 8 MOSFET channels at 100% max brightness
  if (currentMode == MODE_DEBUG) {
    if (now - lastDebugStepTime >= DEBUG_CHANNEL_HOLD_MS) {
      lastDebugStepTime = now;
      debugChannelIndex = (debugChannelIndex + 1) % 8;
      char logBuf[64];
      snprintf(logBuf, sizeof(logBuf), "[DEBUG] Channel -> %s @ 100%%",
               DEBUG_CHANNELS[debugChannelIndex].name);
      telemetryLogLine(logBuf);
    }

    outS1 = {0.0f, 0.0f, 0.0f, 0.0f};
    outS2 = {0.0f, 0.0f, 0.0f, 0.0f};

    const DebugChannel& channel = DEBUG_CHANNELS[debugChannelIndex];
    StripColor& strip = channel.strip == 0 ? outS1 : outS2;
    strip.*(channel.component) = 1.0f;
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
    return DEBUG_CHANNELS[debugChannelIndex & 0x07].label;
  }
  return "NORMAL";
}
