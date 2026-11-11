#pragma once
#include <WiFi.h>
#define U_FLASH 0
#define U_FS 100
#define U_AUTH 200
inline unsigned testUpdateBegins=0, testUpdateClosed=0;
inline bool testUpdateBeginOk=true, testUpdateEndOk=false;
class UpdaterClass {
 bool opened=false, finished=false;
 public:
 ~UpdaterClass() { if(opened) { if(testFlashCheck) testFlashCheck(); ++testUpdateClosed; } }
 bool begin(int,int) { if(testFlashCheck) testFlashCheck(); ++testUpdateBegins; opened=true; return testUpdateBeginOk; }
 void setMD5(const char*) {} bool isFinished() { return finished; }
 unsigned write(WiFiClient&) { finished=true; return testClientBytes; }
 bool end() { if(testFlashCheck) testFlashCheck(); return testUpdateEndOk; }
 template<class T> void printError(T&) {}
};
