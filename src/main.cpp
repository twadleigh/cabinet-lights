#include <Arduino.h>
#include "config.h"
#include "led_driver.h"
#include "door_engine.h"
#include "button_manager.h"
#include "audio_dsp.h"
#include "ota_manager.h"

// ============================================================================
// Core 0: Orchestration, Inputs, Kinematics, PWM Driving, OTA Services (Transfers Block)
// ============================================================================
void setup() {
  Serial.begin(115200);

  ledDriverInit();
  buttonManagerInit();

  // Always assume door is CLOSED on init
  doorEngineInit(false);

  // Mount LittleFS and start non-blocking Wi-Fi immediately (before Serial wait & Core 1 I2S init)
  otaManagerInit();

  // Wait up to 3.5 seconds for PlatformIO serial monitor to attach
  uint32_t start = millis();
  while (!Serial && (uint32_t(millis() - start) < 3500)) {
    delay(10);
  }

  Serial.println(F("\n=============================================="));
  Serial.println(F("Cabinet Controller: Core 0 Engine Online"));
  Serial.println(F("Target: Raspberry Pi Pico 2 W (RP2350)"));
  Serial.println(F("Initial Door State: CLOSED (Ambient)"));
  Serial.println(F("=============================================="));
}

void loop() {
  static uint32_t previousLoop = millis();
  static uint32_t maxLoopMs = 0;
  static uint64_t uptimeMs = previousLoop;
  const uint32_t now = millis();
  const uint32_t elapsed = now - previousLoop;
  previousLoop = now;
  uptimeMs += elapsed;
  if (elapsed > maxLoopMs) maxLoopMs = elapsed;
  // 0. Service Wi-Fi & OTA (an active transfer blocks controls) at ~200 Hz (every 5 ms)
  static uint32_t lastOtaServiceMs = 0;
  if (uint32_t(millis() - lastOtaServiceMs) >= 5) {
    lastOtaServiceMs = millis();
    otaManagerUpdate();
  }

  // 1. Monitor Core 1 Audio DSP Status (Core 0 owns all Serial/UDP telemetry)
  static AudioDspStatus lastDspStatus = AUDIO_DSP_UNINIT;
  AudioDspStatus currentDspStatus = getAudioDspStatus();
  if (currentDspStatus != lastDspStatus) {
    lastDspStatus = currentDspStatus;
    if (currentDspStatus == AUDIO_DSP_ONLINE) {
      telemetryLogLine("[CORE 1] INMP441 I2S Task Running");
    } else if (currentDspStatus == AUDIO_DSP_FAULT) {
      telemetryLogLine("[CORE 1] Failed to initialize I2S interface!");
    }
  }

  // 2. Process Debounced Inputs
  ButtonEvents events = buttonManagerUpdate();
  for (uint8_t value = BTN_EVENT_MODE_NEXT; value <= BTN_EVENT_BRIGHT_DOWN; ++value) {
    auto event = static_cast<ButtonEvent>(value);
    if (events & buttonEventBit(event)) doorEngineHandleButton(event);
  }

  // 3. Evaluate Door Contacts
  bool doorOpen = getDoorSwitchAggregateOpen();
  doorEngineSetDoorState(doorOpen);

  // 4. Compute Color Blending & Kinematics
  StripColor s1, s2;
  doorEngineUpdate(s1, s2);

  // 5. Commit Outputs to MOSFETs via 74AHCT541 (20 kHz hardware-timed Sigma-Delta dither)
  setStripOutputs(s1, s2);

  // 6. Periodic Diagnostic Telemetry (Every 1 second over Serial + UDP)
  static uint32_t lastTelemetry = 0;
  if (uint32_t(millis() - lastTelemetry) >= 1000) {
    lastTelemetry = millis();
    AudioEnergy audioSnap;
    getAudioEnergySnapshot(audioSnap);

    int briTenths = (int)(doorEngineGetAmbientBrightness() * 1000.0f + 0.5f);
    int micPerm   = (int)(audioSnap.subBass * 1000.0f + 0.5f);
    int w1Pct     = (int)(s1.w * 100.0f + 0.5f);
    int r1Pct     = (int)(s1.r * 100.0f + 0.5f);
    int g1Pct     = (int)(s1.g * 100.0f + 0.5f);
    int b1Pct     = (int)(s1.b * 100.0f + 0.5f);
    int w2Pct     = (int)(s2.w * 100.0f + 0.5f);
    int r2Pct     = (int)(s2.r * 100.0f + 0.5f);
    int g2Pct     = (int)(s2.g * 100.0f + 0.5f);
    int b2Pct     = (int)(s2.b * 100.0f + 0.5f);

    char logBuf[220];
    snprintf(logBuf, sizeof(logBuf),
             "[RUNNING] Door: %s | Mode: %s | Bri: %d.%d%% | Mic: %d.%03d | S1(%d.%02d,%d.%02d,%d.%02d,%d.%02d) S2(%d.%02d,%d.%02d,%d.%02d,%d.%02d)",
             doorOpen ? "OPEN" : "CLOSED",
             doorEngineGetModeLabel(),
             briTenths / 10, briTenths % 10,
             micPerm / 1000, micPerm % 1000,
             w1Pct / 100, w1Pct % 100,
             r1Pct / 100, r1Pct % 100,
             g1Pct / 100, g1Pct % 100,
             b1Pct / 100, b1Pct % 100,
             w2Pct / 100, w2Pct % 100,
             r2Pct / 100, r2Pct % 100,
             g2Pct / 100, g2Pct % 100,
             b2Pct / 100, b2Pct % 100);
    telemetryLogLine(logBuf);
    static uint32_t reportSequence = 0;
    char health[320];
    snprintf(health, sizeof(health),
             "[HEALTH] Build: %s | Seq: %lu | UptimeMs: %llu | MaxLoopMs: %lu | DSP: %u | Samples: %lu | FS: %s | OTA: %s | Reconnects: %lu",
             __DATE__ " " __TIME__, static_cast<unsigned long>(++reportSequence),
             static_cast<unsigned long long>(uptimeMs), static_cast<unsigned long>(maxLoopMs),
             static_cast<unsigned>(currentDspStatus), static_cast<unsigned long>(audioDspProcessedSamples()),
             otaManagerIsFsHealthy() ? "OK" : "FAULT", otaManagerState(),
             static_cast<unsigned long>(otaManagerReconnectCount()));
    telemetryLogLine(health);
  }

  delay(5); // ~200 Hz control loop; PWM ISR independently dithers at 20 kHz
}

// ============================================================================
// Core 1: Background Audio Pipeline (Independent Thread)
// ============================================================================
void setup1() {
  while (!otaManagerIsBootReady()) {
    delay(10); // Wait for boot flash work to finish, including a storage failure
  }
  audioDspCore1Init();
}

void loop1() {
  audioDspCore1Update();
  delay(5);
}
