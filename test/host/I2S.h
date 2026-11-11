#pragma once
#include <cstdint>
#include <deque>
inline std::deque<int32_t> testSamples;
#define INPUT 0
inline bool testI2sStarts = true;
inline int testI2sEndCount = 0;
class I2S {
 public:
  explicit I2S(int) {}
  void setBCLK(int) {}
  void setDATA(int) {}
  void setBitsPerSample(int) {}
  bool begin(int) { return testI2sStarts; }
  void end() { ++testI2sEndCount; }
  int available() { return static_cast<int>(testSamples.size() * 8); }
  void read32(int32_t* left, int32_t* right) { *left = testSamples.front(); testSamples.pop_front(); *right = 0; }
};
