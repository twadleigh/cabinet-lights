#pragma once
#include <Arduino.h>

// Initializes non-blocking Wi-Fi state machine and ArduinoOTA hostname/port.
// Never blocks Core 0 if Wi-Fi is unconfigured or the router is unreachable.
void otaManagerInit();

// Polls Wi-Fi connection status and services ArduinoOTA when connected.
// Call on Core 0; an OTA pause handshake can wait up to 200 ms.
// An abortable pre-update guard also protects direct clients before flash work.
void otaManagerUpdate();

// Returns true when connected to Wi-Fi with a valid IP address.
bool otaManagerIsConnected();

// Boot work complete is independent of storage health; failure must not hang Core 1.
bool otaManagerIsBootReady();
bool otaManagerIsFsHealthy();
uint32_t otaManagerReconnectCount();
const char* otaManagerState();

// Broadcasts a single line of telemetry over UDP (local subnet broadcast:UDP_TELEMETRY_PORT)
// when Wi-Fi is connected, in addition to printing to USB Serial.
void telemetryLogLine(const char* line);
