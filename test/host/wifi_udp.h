#pragma once
#include <WiFi.h>
class WiFiUDP {
 public:
 std::deque<std::string> packets;
 std::vector<std::string> sent;
 IPAddress peer{192,0,2,1};
 uint16_t port=5555;
 int begin(uint16_t) { return 1; }
 int parsePacket() { return packets.empty() ? 0 : packets.front().size(); }
 int read(char* p, size_t size) { auto s=packets.front(); packets.pop_front(); size=std::min(size,s.size()); memcpy(p,s.data(),size); return size; }
 IPAddress remoteIP() { return peer; } uint16_t remotePort() { return port; }
 bool beginPacket(IPAddress,uint16_t) { sent.emplace_back(); return true; }
 size_t write(const uint8_t* p,size_t n) { sent.back().append(reinterpret_cast<const char*>(p),n); return n; }
 size_t write(char c) { sent.back()+=c; return 1; }
 void endPacket() {}
};
