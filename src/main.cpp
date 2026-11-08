#include <Arduino.h>
#include "config.h"
#include "led_driver.h"
#include "door_engine.h"
#include "button_manager.h"
#include "audio_dsp.h"
#include "ota_manager.h"

// ============================================================================
// Core 0: Orchestration, Inputs, Kinematics, PWM Driving, Non-Blocking OTA
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
  unsigned long start = millis();
  while (!Serial && (millis() - start < 3500)) {
    delay(10);
  }

  Serial.println(F("\n=============================================="));
  Serial.println(F("Cabinet Controller: Core 0 Engine Online"));
  Serial.println(F("Target: Raspberry Pi Pico 2 W (RP2350)"));
  Serial.println(F("Initial Door State: CLOSED (Ambient)"));
  Serial.println(F("=============================================="));
}

void loop() {
  // 0. Service Non-Blocking Wi-Fi & ArduinoOTA at ~200 Hz (every 5 ms)
  static unsigned long lastOtaServiceMs = 0;
  if (millis() - lastOtaServiceMs >= 5) {
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
  ButtonEvent event = buttonManagerUpdate();
  if (event != BTN_EVENT_NONE) {
    doorEngineHandleButton(event);
  }

  // 3. Evaluate Door Contacts
  bool doorOpen = getDoorSwitchAggregateOpen();
  doorEngineSetDoorState(doorOpen);

  // 4. Compute Color Blending & Kinematics
  StripColor s1, s2;
  doorEngineUpdate(s1, s2);

  // 5. Commit Outputs to MOSFETs via 74AHCT541 (1 kHz Sigma-Delta Temporal Dither)
  setStripOutputs(s1, s2);

  // 6. Periodic Diagnostic Telemetry (Every 1 second over Serial + UDP)
  static unsigned long lastTelemetry = 0;
  if (millis() - lastTelemetry >= 1000) {
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
  }

  delay(1); // ~1000 Hz update loop on Core 0 for flicker-free Sigma-Delta PWM dithering
}

// ============================================================================
// Core 1: Background Audio Pipeline (Independent Thread)
// ============================================================================
void setup1() {
  while (!otaManagerIsFsReady()) {
    delay(10); // Wait for Core 0 to mount/format LittleFS before starting I2S DMA
  }
  audioDspCore1Init();
}

void loop1() {
  audioDspCore1Update();
  delay(5);
}
