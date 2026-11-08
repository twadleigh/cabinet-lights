#include "led_driver.h"
#include "config.h"

static const uint8_t PWM_PINS[8] = {
  PIN_PWM_W1, PIN_PWM_B1, PIN_PWM_R1, PIN_PWM_G1,
  PIN_PWM_W2, PIN_PWM_B2, PIN_PWM_R2, PIN_PWM_G2
};

void ledDriverInit() {
  analogWriteFreq(20000); // 20 kHz ultrasonic PWM
  analogWriteRange(1023);  // 10-bit hardware resolution

  for (uint8_t pin : PWM_PINS) {
    pinMode(pin, OUTPUT);
    analogWrite(pin, 0);
  }
}

static inline uint16_t perceptualToDuty(float val) {
  val = constrain(val, 0.0f, 1.0f);
  return (uint16_t)(powf(val, GAMMA_EXPONENT) * 1023.0f + 0.5f);
}

void setStripOutputs(const StripColor& s1, const StripColor& s2) {
  analogWrite(PIN_PWM_W1, perceptualToDuty(s1.w));
  analogWrite(PIN_PWM_B1, perceptualToDuty(s1.b));
  analogWrite(PIN_PWM_R1, perceptualToDuty(s1.r));
  analogWrite(PIN_PWM_G1, perceptualToDuty(s1.g));

  analogWrite(PIN_PWM_W2, perceptualToDuty(s2.w));
  analogWrite(PIN_PWM_B2, perceptualToDuty(s2.b));
  analogWrite(PIN_PWM_R2, perceptualToDuty(s2.r));
  analogWrite(PIN_PWM_G2, perceptualToDuty(s2.g));
}

void setAllOutputsOff() {
  for (uint8_t pin : PWM_PINS) {
    analogWrite(pin, 0);
  }
}