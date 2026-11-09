#pragma once
#include <cstdint>
struct pwm_config { uint16_t wrap = 0; uint8_t div = 0; };
inline uint16_t testLevels[8][2]{};
inline pwm_config testConfigs[8]{};
inline uint32_t testSliceMask = 0;
inline pwm_config pwm_get_default_config() { return {}; }
inline void pwm_config_set_clkdiv_int(pwm_config* c, uint8_t div) { c->div = div; }
inline void pwm_config_set_wrap(pwm_config* c, uint16_t wrap) { c->wrap = wrap; }
inline uint8_t pwm_gpio_to_slice_num(uint8_t pin) { return (pin / 2) % 8; }
inline void pwm_init(uint8_t slice, const pwm_config* c, bool) { testConfigs[slice] = *c; }
inline void pwm_set_both_levels(uint8_t slice, uint16_t a, uint16_t b) {
  testLevels[slice][0] = a; testLevels[slice][1] = b;
}
inline void pwm_clear_irq(uint8_t) {}
inline void pwm_set_irq_enabled(uint8_t, bool) {}
inline void pwm_set_mask_enabled(uint32_t mask) { testSliceMask = mask; }
#define GPIO_FUNC_PWM 4
inline void gpio_set_function(uint8_t, unsigned) {}
