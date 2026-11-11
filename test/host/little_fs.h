#pragma once
#include "Arduino.h"
inline bool testFsSuccess=true, testRemoveSuccess=true, testStaleFile=false;
inline unsigned testFsCalls=0;
inline std::function<void()> testFlashCheck;
struct TestFS {
 bool begin() { ++testFsCalls; if(testFlashCheck) testFlashCheck(); return testFsSuccess; }
 bool exists(const char*) { return testStaleFile; }
 bool remove(const char*) { if(testFlashCheck) testFlashCheck(); return testRemoveSuccess; }
 void end() {}
};
inline TestFS LittleFS;
