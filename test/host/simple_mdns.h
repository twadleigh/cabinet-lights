#pragma once
struct TestMDNS { void begin(const char*) {} void enableArduino(int,bool=false) {} void update() {} void end() {} };
inline TestMDNS MDNS;
