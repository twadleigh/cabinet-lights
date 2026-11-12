#pragma once
#include <Arduino.h>

// ============================================================================
// 8-Channel PWM Outputs (Connected via 74AHCT541 to IRLZ44N Gates)
// ============================================================================
constexpr uint8_t PIN_PWM_WORK_SHELF_W = 16;  // GP16 (Physical Pin 21) - Work shelf (strip 1) White
constexpr uint8_t PIN_PWM_WORK_SHELF_B = 17;  // GP17 (Physical Pin 22) - Work shelf (strip 1) Blue
constexpr uint8_t PIN_PWM_WORK_SHELF_R = 18;  // GP18 (Physical Pin 24) - Work shelf (strip 1) Red
constexpr uint8_t PIN_PWM_WORK_SHELF_G = 19;  // GP19 (Physical Pin 25) - Work shelf (strip 1) Green

constexpr uint8_t PIN_PWM_CABINET_PERIMETER_W = 20;  // GP20 (Physical Pin 26) - Cabinet perimeter (strip 2) White
constexpr uint8_t PIN_PWM_CABINET_PERIMETER_B = 21;  // GP21 (Physical Pin 27) - Cabinet perimeter (strip 2) Blue
constexpr uint8_t PIN_PWM_CABINET_PERIMETER_R = 26;  // GP26 (Physical Pin 31) - Cabinet perimeter (strip 2) Red
constexpr uint8_t PIN_PWM_CABINET_PERIMETER_G = 27;  // GP27 (Physical Pin 32) - Cabinet perimeter (strip 2) Green

// ============================================================================
// Input Sensors & Buttons (Active-LOW with internal pull-ups)
// ============================================================================
constexpr uint8_t PIN_DOOR_LEFT  = 0;   // GP0  (Physical Pin 1)
constexpr uint8_t PIN_DOOR_RIGHT = 1;   // GP1  (Physical Pin 2)

constexpr uint8_t PIN_BTN_MODE_NEXT   = 8;   // GP8  (Physical Pin 11) - Button 1: Cycle Mode Forward
constexpr uint8_t PIN_BTN_MODE_PREV   = 9;   // GP9  (Physical Pin 12) - Button 2: Cycle Mode Backward
constexpr uint8_t PIN_BTN_BRIGHT_UP   = 10;  // GP10 (Physical Pin 14) - Button 3: Brighten Ambient (or Mode Param +)
constexpr uint8_t PIN_BTN_BRIGHT_DOWN = 11;  // GP11 (Physical Pin 15) - Button 4: Darken Ambient (or Mode Param -)

// ============================================================================
// INMP441 I2S Microphone (Core 1 Real-Time Audio Pipeline)
// ============================================================================
constexpr uint8_t PIN_I2S_BCLK = 13;    // GP13 (Physical Pin 17) - Bit Clock
constexpr uint8_t PIN_I2S_WS   = 14;    // GP14 (Physical Pin 19) - Word Select (Hardwired to BCLK + 1)
constexpr uint8_t PIN_I2S_DATA = 15;    // GP15 (Physical Pin 20) - Serial Data

static_assert(PIN_I2S_WS == PIN_I2S_BCLK + 1,
              "Earle Philhower I2S hardware PIO requires WS == BCLK + 1");

// ============================================================================
// Shared Timing, PWM Resolution & Color Calibration
// ============================================================================
// Gate-drive physics note: 180 Ohm series gate stopper * IRLZ44N C_iss (~1.7 nF)
// yields a ~300 ns (~45-tick) gate RC time constant. Pulses <= 20 ticks (133 ns)
// never reach V_GS(th), while pulses >= 30 ticks (200 ns) turn on both strips evenly.
constexpr uint32_t      PWM_FREQ_HZ           = 20000UL; // 20 kHz ultrasonic PWM
constexpr uint16_t      PWM_MAX_DUTY          = 7500;    // Full native 150 MHz / 20 kHz counter ticks (6.67 ns/tick)
constexpr uint16_t      PWM_PEDESTAL_TICKS    = 20;      // Zero-conduction gate-charge cutoff (20 ticks = 133 ns)
constexpr uint16_t      PWM_MIN_PULSE_TICKS   = 30;      // Minimum matched pulse width where both strips conduct evenly (30 ticks = 200 ns)
constexpr unsigned long DOOR_FADE_RAMP_MS     = 5000UL;  // 5-second full-scale fade time constant
constexpr unsigned long HUE_CYCLE_PERIOD_MS   = 30000UL; // 30-second random hue period
constexpr unsigned long DEBUG_CHANNEL_HOLD_MS = 1500UL;  // 1.5-second hold per channel in Debug Mode
constexpr float         GAMMA_EXPONENT        = 2.4f;    // Empirical perceptual exponent; not exact CIE L*

static_assert(F_CPU / PWM_FREQ_HZ == PWM_MAX_DUTY,
              "PWM_MAX_DUTY must match exact integer sysclk division (clkdiv = 1.0)");
static_assert(PWM_MIN_PULSE_TICKS > PWM_PEDESTAL_TICKS,
              "PWM_MIN_PULSE_TICKS must exceed PWM_PEDESTAL_TICKS");
static_assert(PWM_MAX_DUTY > PWM_MIN_PULSE_TICKS,
              "PWM_MAX_DUTY must exceed PWM_MIN_PULSE_TICKS");

// Ambient-only perceptual multiplier, applied before task-white blending and gamma.
constexpr float WORK_SHELF_AMBIENT_FRACTION = 0.50f;
static_assert(WORK_SHELF_AMBIENT_FRACTION >= 0.0f && WORK_SHELF_AMBIENT_FRACTION <= 1.0f,
              "Work shelf ambient fraction must be between zero and one");

constexpr float OPEN_PERCEPTUAL_W = 1.00f; // Max white level when door is open
constexpr float OPEN_PERCEPTUAL_R = 1.00f; // Photopic white red component
constexpr float OPEN_PERCEPTUAL_G = 0.90f; // Photopic white green component
constexpr float OPEN_PERCEPTUAL_B = 0.86f; // Photopic white blue component

// ============================================================================
// Network, OTA & Wireless UDP Telemetry (Core 0)
// ============================================================================
constexpr const char* OTA_HOSTNAME       = "cabinet-lights";
constexpr uint16_t    OTA_PORT           = 2040; // Default arduino-pico espota port
constexpr uint16_t    UDP_TELEMETRY_PORT = 44444;
constexpr unsigned long WIFI_RETRY_MS    = 15000UL; // Non-blocking reconnect interval