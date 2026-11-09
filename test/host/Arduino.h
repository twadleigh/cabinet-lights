#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#define F_CPU 150000000UL
#define __not_in_flash_func(name) name
inline unsigned long testMillis = 0;
inline unsigned long millis() { return testMillis; }
inline void (*testDelayHook)() = nullptr;
inline void delay(unsigned long ms) {
  testMillis += ms;
  static bool pumping = false;
  if (testDelayHook && !pumping) {
    pumping = true;
    testDelayHook();
    pumping = false;
  }
  std::this_thread::yield();
}
inline void __dmb() { std::atomic_thread_fence(std::memory_order_seq_cst); }
template<class T> T constrain(T v, T lo, T hi) { return std::clamp(v, lo, hi); }
inline long random(long lo, long hi) { return lo + std::rand() % (hi - lo); }
inline void randomSeed(unsigned long seed) { std::srand(seed); }
struct TestRp2040 { unsigned long hwrand32() { return 42; } };
inline TestRp2040 rp2040;
