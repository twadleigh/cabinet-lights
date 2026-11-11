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
inline uint32_t testMillis = 0;
inline uint32_t millis() { return testMillis; }
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
struct TestRp2040 { unsigned long hwrand32() { return 42; } const char* getChipID() { return "test"; } void reboot() {} };
inline TestRp2040 rp2040;

#include <string>
#include <cstring>
#include <functional>
#include <vector>
#include <deque>
#define LOW 0
#define HIGH 1
#define INPUT_PULLUP 2
#define F(x) x
inline bool testPins[32]{};
inline void pinMode(uint8_t, int) {}
inline bool digitalRead(uint8_t p) { return testPins[p]; }
inline uint32_t micros() { return testMillis * 1000; }
#define PICO_UNIQUE_BOARD_ID_SIZE_BYTES 8
#define DEC 10
class String : public std::string {
 public:
  using std::string::string;
  String() = default;
  String(const std::string& s): std::string(s) {}
  String(uint32_t v): std::string(std::to_string(v)) {}
  void trim() { while (!empty() && (back() == '\r' || back() == ' ')) pop_back(); }
  bool equals(const String& other) const { return *this == other; }
};
struct TestSerial {
 std::vector<std::string> lines;
 void begin(int) {}
 operator bool() const { return true; }
 void print(const char* s) { lines.emplace_back(s); }
 void println(const char* s) { lines.emplace_back(s); }
};
inline TestSerial Serial;
