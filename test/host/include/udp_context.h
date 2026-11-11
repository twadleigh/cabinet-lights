#pragma once
#include <WiFi.h>
class UdpContext {
 public:
 static inline UdpContext* last=nullptr;
 std::string input,output;
 bool pending=false;
 IPAddress peer{192,0,2,1};
 uint16_t peerPort=5555;
 std::function<void()> callback;
 UdpContext() { last=this; }
 void ref() {} void unref() {}
 bool listen(int,int) { return true; }
 void onRx(std::function<void()> cb) { callback=cb; }
 void inject(const std::string& s) { input=s; pending=true; callback(); }
 bool next() { bool p=pending; pending=false; return p; }
 int peek() { return input.empty() ? -1 : input.front(); }
 int read() { if(input.empty()) return -1; int c=input.front(); input.erase(0,1); return c; }
 void flush() { input.clear(); }
 IPAddress getRemoteAddress() { return peer; } uint16_t getRemotePort() { return peerPort; }
 void append(const char* s,size_t n) { output.append(s,n); }
 void send(IPAddress,int) {}
};
