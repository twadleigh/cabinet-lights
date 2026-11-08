#include <Arduino.h>
#include "config.h"
#include "led_driver.h"
#include "door_engine.h"
#include "button_manager.h"
#include "audio_dsp.h"

// ============================================================================
// Core 0: Orchestration, Inputs, Kinematics, PWM Driving
// ============================================================================
void setup() {
  Serial.begin(115200);

  // Wait up to 3.5 seconds for PlatformIO serial monitor to attach
  unsigned long start = millis();
  while (!Serial && (millis() - start < 3500)) {
    delay(10);
  }

  ledDriverInit();
  buttonManagerInit();

  // Always assume door is CLOSED on init
  doorEngineInit(false);

  Serial.println(F("\n=============================================="));
  Serial.println(F("Cabinet Controller: Core 0 Engine Online"));
  Serial.println(F("Target: Raspberry Pi Pico 2 W (RP2350)"));
  Serial.println(F("Initial Door State: CLOSED (Ambient)"));
  Serial.println(F("=============================================="));
}

void loop() {
  // 1. Process Debounced Inputs
  ButtonEvent event = buttonManagerUpdate();
  if (event != BTN_EVENT_NONE) {
    Serial.printf("[BUTTON] Event Detected: ID %d\n", (int)event);
  }

  // 2. Evaluate Door Contacts
  bool doorOpen = getDoorSwitchAggregateOpen();
  doorEngineSetDoorState(doorOpen);

  // 3. Compute Color Blending & Kinematics
  StripColor s1, s2;
  doorEngineUpdate(s1, s2);

  // 4. Commit Outputs to MOSFETs via 74AHCT541
  setStripOutputs(s1, s2);

  // 5. Periodic Diagnostic Telemetry (Every 1 second)
  static unsigned long lastTelemetry = 0;
  if (millis() - lastTelemetry >= 1000) {
    lastTelemetry = millis();
    Serial.printf("[RUNNING] Door: %s | S1 (W:%.2f R:%.2f G:%.2f B:%.2f)\n",
                  doorOpen ? "OPEN" : "CLOSED",
                  s1.w, s1.r, s1.g, s1.b);
  }

  delay(5); // ~200 Hz update loop on Core 0
}

// ============================================================================
// Core 1: Background Audio Pipeline (Independent Thread)
// ============================================================================
void setup1() {
  delay(500); // Allow Core 0 to complete hardware setups and USB boot
  audioDspCore1Init();
}

void loop1() {
  audioDspCore1Update();
  delay(5);
}