#include "led_driver.h"
#include "config.h"

static const uint8_t PWM_PINS[8] = {
  PIN_PWM_W1, PIN_PWM_B1, PIN_PWM_R1, PIN_PWM_G1,
  PIN_PWM_W2, PIN_PWM_B2, PIN_PWM_R2, PIN_PWM_G2
};

// Per-channel first-order Sigma-Delta fractional error accumulators (Core 0 @ 1 kHz)
static float ditherErr[8] = {0.0f};

void ledDriverInit() {
  analogWriteFreq(PWM_FREQ_HZ);   // 20 kHz ultrasonic PWM
  analogWriteRange(PWM_MAX_DUTY); // 7500 native clock ticks (150 MHz / 20 kHz)

  for (uint8_t i = 0; i < 8; ++i) {
    ditherErr[i] = 0.0f;
    pinMode(PWM_PINS[i], OUTPUT);
    analogWrite(PWM_PINS[i], 0);
  }
}

static inline uint16_t perceptualToDitheredDuty(float val, uint8_t chIdx) {
  val = constrain(val, 0.0f, 1.0f);
  if (val <= 0.0f) {
    ditherErr[chIdx] = 0.0f;
    return 0;
  }

  constexpr float activeSpan = static_cast<float>(PWM_MAX_DUTY - PWM_PEDESTAL_TICKS);
  constexpr float minPulseActiveTicks = static_cast<float>(PWM_MIN_PULSE_TICKS - PWM_PEDESTAL_TICKS);

  float activeTicks = powf(val, GAMMA_EXPONENT) * activeSpan;
  if (activeTicks >= activeSpan) {
    ditherErr[chIdx] = 0.0f;
    return PWM_MAX_DUTY;
  }

  // Sub-threshold region (exactDuty < 30 ticks):
  // Never emit a pulse narrower than PWM_MIN_PULSE_TICKS (30 ticks), where MOSFET V_GS(th)
  // variance causes one strip to drop out before the other. Instead, Sigma-Delta dither
  // at 1 kHz between 0 (off) and PWM_MIN_PULSE_TICKS (30 ticks) so both strips dim together.
  if (activeTicks < minPulseActiveTicks) {
    float subKneeFrac = activeTicks / minPulseActiveTicks;
    float accum = subKneeFrac + ditherErr[chIdx];
    if (accum >= 1.0f) {
      ditherErr[chIdx] = accum - 1.0f;
      return PWM_MIN_PULSE_TICKS;
    } else {
      ditherErr[chIdx] = accum;
      return 0;
    }
  }

  // Matched continuous region (>= 30 ticks): dither between adjacent integer ticks
  float exactDuty = static_cast<float>(PWM_PEDESTAL_TICKS) + activeTicks;
  float accum = exactDuty + ditherErr[chIdx];
  uint16_t outDuty = static_cast<uint16_t>(accum);
  ditherErr[chIdx] = accum - static_cast<float>(outDuty);
  return outDuty;
}

void setStripOutputs(const StripColor& s1, const StripColor& s2) {
  analogWrite(PIN_PWM_W1, perceptualToDitheredDuty(s1.w, 0));
  analogWrite(PIN_PWM_B1, perceptualToDitheredDuty(s1.b, 1));
  analogWrite(PIN_PWM_R1, perceptualToDitheredDuty(s1.r, 2));
  analogWrite(PIN_PWM_G1, perceptualToDitheredDuty(s1.g, 3));

  analogWrite(PIN_PWM_W2, perceptualToDitheredDuty(s2.w, 4));
  analogWrite(PIN_PWM_B2, perceptualToDitheredDuty(s2.b, 5));
  analogWrite(PIN_PWM_R2, perceptualToDitheredDuty(s2.r, 6));
  analogWrite(PIN_PWM_G2, perceptualToDitheredDuty(s2.g, 7));
}

void setAllOutputsOff() {
  for (uint8_t i = 0; i < 8; ++i) {
    ditherErr[i] = 0.0f;
    analogWrite(PWM_PINS[i], 0);
  }
}