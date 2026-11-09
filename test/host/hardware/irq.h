#pragma once
#define PWM_DEFAULT_IRQ_NUM() 0
inline void irq_set_exclusive_handler(unsigned, void (*)()) {}
inline void irq_set_enabled(unsigned, bool) {}
