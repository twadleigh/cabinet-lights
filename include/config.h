#pragma once
#include <Arduino.h>

// ============================================================================
// 8-Channel PWM Outputs (Connected via 74AHCT541 to IRLZ44N Gates)
// ============================================================================
constexpr uint8_t PIN_PWM_W1 = 16;  // GP16 (Physical Pin 21) - Strip 1 White
constexpr uint8_t PIN_PWM_B1 = 17;  // GP17 (Physical Pin 22) - Strip 1 Blue
constexpr uint8_t PIN_PWM_R1 = 18;  // GP18 (Physical Pin 24) - Strip 1 Red
constexpr uint8_t PIN_PWM_G1 = 19;  // GP19 (Physical Pin 25) - Strip 1 Green

constexpr uint8_t PIN_PWM_W2 = 20;  // GP20 (Physical Pin 26) - Strip 2 White
constexpr uint8_t PIN_PWM_B2 = 21;  // GP21 (Physical Pin 27) - Strip 2 Blue
constexpr uint8_t PIN_PWM_R2 = 26;  // GP26 (Physical Pin 31) - Strip 2 Red
constexpr uint8_t PIN_PWM_G2 = 27;  // GP27 (Physical Pin 32) - Strip 2 Green

// ============================================================================
// Input Sensors & Buttons (Active-LOW with internal pull-ups)
// ============================================================================
constexpr uint8_t PIN_DOOR_LEFT  = 0;   // GP0  (Physical Pin 1)
constexpr uint8_t PIN_DOOR_RIGHT = 1;   // GP1  (Physical Pin 2)

constexpr uint8_t PIN_BTN_MODE   = 8;   // GP8  (Physical Pin 11)
constexpr uint8_t PIN_BTN_UP     = 9;   // GP9  (Physical Pin 12)
constexpr uint8_t PIN_BTN_DOWN   = 10;  // GP10 (Physical Pin 14)
constexpr uint8_t PIN_BTN_TASK   = 11;  // GP11 (Physical Pin 15)

// ============================================================================
// INMP441 I2S Microphone (Stubbed on Core 1)
// ============================================================================
constexpr uint8_t PIN_I2S_BCLK = 13;    // GP13 (Physical Pin 17) - Bit Clock
constexpr uint8_t PIN_I2S_WS   = 14;    // GP14 (Physical Pin 19) - Word Select
constexpr uint8_t PIN_I2S_DATA = 15;    // GP15 (Physical Pin 20) - Serial Data

// ============================================================================
// Shared Timing & Color Calibration
// ============================================================================
constexpr unsigned long DOOR_FADE_RAMP_MS   = 5000UL;  // 5-second fade curve
constexpr unsigned long HUE_CYCLE_PERIOD_MS = 30000UL; // 30-second random hue period
constexpr float         GAMMA_EXPONENT      = 2.4f;    // CIE L* power-law exponent

constexpr float DIM_PERCEPTUAL    = 0.50f; // Ambient brightness scalar [0.0 - 1.0]
constexpr float OPEN_PERCEPTUAL_W = 1.00f; // Max white level when door is open
constexpr float OPEN_PERCEPTUAL_R = 1.00f; // Photopic white red component
constexpr float OPEN_PERCEPTUAL_G = 0.90f; // Photopic white green component
constexpr float OPEN_PERCEPTUAL_B = 0.86f; // Photopic white blue component