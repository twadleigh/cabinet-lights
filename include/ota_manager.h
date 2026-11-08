#pragma once
#include <Arduino.h>

// Initializes non-blocking Wi-Fi state machine and ArduinoOTA hostname/port.
// Never blocks Core 0 if Wi-Fi is unconfigured or the router is unreachable.
void otaManagerInit();

// Polls Wi-Fi connection status and services ArduinoOTA when connected.
// Safe to call every 5 ms on Core 0 without stalling the 200 Hz lighting loop.
void otaManagerUpdate();

// Returns true when connected to Wi-Fi with a valid IP address.
bool otaManagerIsConnected();

// Returns true once LittleFS is mounted at boot so Core 1 can safely start I2S DMA.
bool otaManagerIsFsReady();

// Broadcasts a single line of telemetry over UDP (255.255.255.255:UDP_TELEMETRY_PORT)
// when Wi-Fi is connected, in addition to printing to USB Serial.
void telemetryLogLine(const char* line);
