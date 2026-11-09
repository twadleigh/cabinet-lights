#include "led_driver.h"
#include "config.h"
#include <hardware/irq.h>
#include <hardware/pwm.h>
#include <hardware/sync.h>

static constexpr uint8_t PWM_PINS[8] = {
  PIN_PWM_W1, PIN_PWM_B1, PIN_PWM_R1, PIN_PWM_G1,
  PIN_PWM_W2, PIN_PWM_B2, PIN_PWM_R2, PIN_PWM_G2
};

struct DitherTarget {
  uint16_t base;
  uint16_t fraction; // Q0.16 probability of adding step
  uint16_t step;     // 30 in the sub-threshold region; 1 for adjacent ticks
};
static volatile DitherTarget targets[8]{};
static uint16_t ditherErr[8]{};
static uint8_t slices[4]{};

// All state lives in SRAM; the handler and SDK register helpers execute without
// flash access. Only Core 0 owns these PWM slices and their wrap interrupt.
static void __not_in_flash_func(pwmWrapHandler)() {
  pwm_clear_irq(slices[0]);
  uint16_t levels[8];
  for (uint8_t i = 0; i < 8; ++i) {
    const uint16_t base = targets[i].base;
    const uint16_t fraction = targets[i].fraction;
    const uint16_t step = targets[i].step;
    uint32_t sum = static_cast<uint32_t>(ditherErr[i]) + fraction;
    ditherErr[i] = fraction ? static_cast<uint16_t>(sum) : 0;
    levels[i] = base + ((sum >> 16) ? step : 0);
  }
  for (uint8_t i = 0; i < 4; ++i) {
    pwm_set_both_levels(slices[i], levels[2 * i], levels[2 * i + 1]);
  }
}

void ledDriverInit() {
  uint32_t sliceMask = 0;
  pwm_config config = pwm_get_default_config();
  pwm_config_set_clkdiv_int(&config, 1);
  pwm_config_set_wrap(&config, PWM_MAX_DUTY - 1); // 7500 ticks, exactly 20 kHz
  for (uint8_t i = 0; i < 4; ++i) {
    slices[i] = pwm_gpio_to_slice_num(PWM_PINS[2 * i]);
    sliceMask |= 1u << slices[i];
    pwm_init(slices[i], &config, false);
    pwm_set_both_levels(slices[i], 0, 0);
    gpio_set_function(PWM_PINS[2 * i], GPIO_FUNC_PWM);
    gpio_set_function(PWM_PINS[2 * i + 1], GPIO_FUNC_PWM);
  }
  irq_set_exclusive_handler(PWM_DEFAULT_IRQ_NUM(), pwmWrapHandler);
  pwm_clear_irq(slices[0]);
  pwm_set_irq_enabled(slices[0], true);
  irq_set_enabled(PWM_DEFAULT_IRQ_NUM(), true);
  pwm_set_mask_enabled(sliceMask); // Start all four counters together.
}

static DitherTarget perceptualToTarget(float val) {
  val = constrain(val, 0.0f, 1.0f);
  if (val <= 0.0f) return {0, 0, 0};
  if (val >= 1.0f) return {PWM_MAX_DUTY, 0, 0};
  constexpr float activeSpan = PWM_MAX_DUTY - PWM_PEDESTAL_TICKS;
  constexpr float minActive = PWM_MIN_PULSE_TICKS - PWM_PEDESTAL_TICKS;
  float activeTicks = powf(val, GAMMA_EXPONENT) * activeSpan;
  if (activeTicks < minActive) {
    uint32_t probability = static_cast<uint32_t>(activeTicks / minActive * 65536.0f + 0.5f);
    if (probability >= 65536u) return {PWM_MIN_PULSE_TICKS, 0, 0};
    return {0, static_cast<uint16_t>(probability), PWM_MIN_PULSE_TICKS};
  }
  float exactDuty = PWM_PEDESTAL_TICKS + activeTicks;
  uint16_t base = static_cast<uint16_t>(exactDuty);
  uint32_t fraction = static_cast<uint32_t>((exactDuty - base) * 65536.0f + 0.5f);
  if (fraction >= 65536u) return {static_cast<uint16_t>(base + 1), 0, 0};
  return {base, static_cast<uint16_t>(fraction), 1};
}

void setStripOutputs(const StripColor& s1, const StripColor& s2) {
  const float values[8] = {s1.w, s1.b, s1.r, s1.g, s2.w, s2.b, s2.r, s2.g};
  DitherTarget next[8];
  for (uint8_t i = 0; i < 8; ++i) next[i] = perceptualToTarget(values[i]);
  // Publish a complete frame atomically with respect to the Core 0 ISR. Keep
  // powf and float conversion outside this brief interrupt-disabled section.
  uint32_t irqState = save_and_disable_interrupts();
  for (uint8_t i = 0; i < 8; ++i) {
    targets[i].base = next[i].base;
    targets[i].fraction = next[i].fraction;
    targets[i].step = next[i].step;
  }
  restore_interrupts(irqState);
}

void setAllOutputsOff() {
  uint32_t irqState = save_and_disable_interrupts();
  for (uint8_t i = 0; i < 8; ++i) {
    targets[i].base = targets[i].fraction = targets[i].step = 0;
    ditherErr[i] = 0;
  }
  for (uint8_t i = 0; i < 4; ++i) pwm_set_both_levels(slices[i], 0, 0);
  restore_interrupts(irqState);
}
