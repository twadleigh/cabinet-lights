#pragma once
#include <cstdint>
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
  int available() { return 0; }
  void read32(int32_t*, int32_t*) {}
};
