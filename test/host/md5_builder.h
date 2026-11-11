#pragma once
#include "Arduino.h"
class MD5Builder {
 public:
 void begin() {} void add(const String&) {} void calculate() {}
 String toString() { return "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"; }
};
