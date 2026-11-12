#pragma once
#include "Arduino.h"
class IPAddress {
 uint32_t value;
 public:
 IPAddress(uint32_t v=0): value(v) {}
 IPAddress(uint8_t a,uint8_t b,uint8_t c,uint8_t d): value(a | uint32_t(b)<<8 | uint32_t(c)<<16 | uint32_t(d)<<24) {}
 operator uint32_t() const { return value; }
 uint8_t operator[](unsigned n) const { return value >> (8*n); }
};
#define WIFI_STA 1
#define WL_CONNECTED 3
struct TestWiFi {
 int link = 0;
 void mode(int) {} void noLowPowerMode() {} void setHostname(const char*) {}
 void beginNoBlock(const char*,const char*) {} void disconnect() { link=0; }
 int status() { return link; }
 IPAddress localIP() { return IPAddress(192,0,2,4); }
 IPAddress subnetMask() { return IPAddress(255,255,255,0); }
};
inline TestWiFi WiFi;
inline bool testClientConnect=false;
inline int testClientBytes=0;
inline uint32_t testClientReadyStart=0, testClientReadyDelay=0;
class WiFiClient {
 public:
 bool connect(IPAddress,int) { return testClientConnect; }
 void setNoDelay(bool) {} bool connected() { return testClientConnect; }
 int available() { return uint32_t(millis() - testClientReadyStart) >= testClientReadyDelay ? testClientBytes : 0; }
 void print(unsigned,int) {} void print(const char*) {} void flush() {} void stop() {}
};
