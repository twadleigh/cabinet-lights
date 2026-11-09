#include "ota_manager.h"
#include "config.h"
#include "audio_dsp.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#pragma GCC diagnostic pop
#include <LittleFS.h>
#include <string.h>
#include <atomic>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

enum WifiState : uint8_t {
  WIFI_STATE_DISABLED = 0,
  WIFI_STATE_CONNECTING,
  WIFI_STATE_CONNECTED,
  WIFI_STATE_DISCONNECTED
};

static WifiState wifiState = WIFI_STATE_DISABLED;
static bool otaStarted = false;
static std::atomic<bool> fsReady{false};
static bool otaPrepPending = false;
static bool otaUploadActive = false;
static bool otaResumePending = false;
static unsigned long otaPrepMs = 0;
static constexpr unsigned long OTA_PREP_TIMEOUT_MS = 15000UL;
static unsigned long lastConnectAttemptMs = 0;
static WiFiUDP udpTelemetry;

static bool credentialsConfigured() {
  if (strlen(WIFI_SSID) == 0) return false;
  if (strcmp(WIFI_SSID, "YOUR_WIFI_SSID") == 0) return false;
  return true;
}

static void startWifiAttempt() {
  WiFi.mode(WIFI_STA);
  WiFi.noLowPowerMode(); // Disable CYW43 power-save sleep so incoming OTA UDP packets never drop
  WiFi.setHostname(OTA_HOSTNAME);
  WiFi.beginNoBlock(WIFI_SSID, WIFI_PASSWORD);
  lastConnectAttemptMs = millis();
  wifiState = WIFI_STATE_CONNECTING;
}

static void setupArduinoOtaOnce() {
  if (otaStarted) return;

  ArduinoOTA.setPort(OTA_PORT);
  ArduinoOTA.setHostname(OTA_HOSTNAME);
#ifdef OTA_PASSWORD
  if (strlen(OTA_PASSWORD) > 0) ArduinoOTA.setPassword(OTA_PASSWORD);
#endif

  ArduinoOTA.onStart([]() {
    otaUploadActive = true;
    otaPrepPending = false;
    // PREP pauses before Update.begin() can erase flash. This second,
    // idempotent call also covers clients that skip the preflight handshake.
    telemetryLogLine("[OTA] Requesting Core 1 to shut down I2S DMA...");
    audioDspStopForOta();
    telemetryLogLine("[OTA] Core 1 I2S stopped. Receiving firmware...");
  });

  ArduinoOTA.onEnd([]() {
    telemetryLogLine("[OTA] Firmware staged to LittleFS. Rebooting...");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    char msg[64];
    snprintf(msg, sizeof(msg), "[OTA] Error (%u)", static_cast<unsigned>(error));
    telemetryLogLine(msg);
    otaUploadActive = false;
    otaPrepPending = false;
    // ArduinoOTA may still close/write its Update file after this callback.
    // Restart DMA only once handle() has returned and flash cleanup is done.
    otaResumePending = true;
  });

  ArduinoOTA.begin();
  udpTelemetry.begin(UDP_TELEMETRY_PORT);
  otaStarted = true;
}

void otaManagerInit() {
  // Mount (and auto-format if needed) the 1MB LittleFS OTA partition at boot
  // BEFORE Core 1 starts I2S DMA, and remove any stale /firmware.bin so Update.begin()
  // never has to erase flash sectors before ArduinoOTA.onStart() runs.
  if (!LittleFS.begin()) {
    LittleFS.format();
    LittleFS.begin();
  }
  if (LittleFS.exists("/firmware.bin")) {
    LittleFS.remove("/firmware.bin");
  }
  fsReady = true;

  if (!credentialsConfigured()) {
    wifiState = WIFI_STATE_DISABLED;
    Serial.println(F("[WIFI] Disabled (Set WIFI_SSID in include/secrets.h)"));
    return;
  }

  Serial.print(F("[WIFI] Connecting non-blocking to SSID: "));
  Serial.println(WIFI_SSID);
  startWifiAttempt();
}

void otaManagerUpdate() {
  // Run even after Wi-Fi disconnects so an abandoned PREP cannot silence audio.
  if (otaPrepPending && !otaUploadActive && millis() - otaPrepMs >= OTA_PREP_TIMEOUT_MS) {
    otaPrepPending = false;
    audioDspResumeAfterOta();
    telemetryLogLine("[OTA] Preparation expired; audio resumed.");
  }
  if (wifiState == WIFI_STATE_DISABLED) {
    return;
  }

  bool linkUp = (WiFi.status() == WL_CONNECTED) && (WiFi.localIP()[0] != 0);

  if (linkUp) {
    if (wifiState != WIFI_STATE_CONNECTED) {
      wifiState = WIFI_STATE_CONNECTED;
      setupArduinoOtaOnce();

      char msg[128];
      IPAddress ip = WiFi.localIP();
      snprintf(msg, sizeof(msg),
               "[WIFI] Connected! IP: %u.%u.%u.%u | Host: %s.local | OTA:%u | UDP:%u",
               ip[0], ip[1], ip[2], ip[3],
               OTA_HOSTNAME, OTA_PORT, UDP_TELEMETRY_PORT);
      telemetryLogLine(msg);
    }

    int pktSize = udpTelemetry.parsePacket();
    if (pktSize > 0) {
      char cmdBuf[32];
      int n = udpTelemetry.read(cmdBuf, sizeof(cmdBuf) - 1);
      if (n > 0) {
        cmdBuf[n] = '\0';
        if (strcmp(cmdBuf, "OTA_PREP") == 0 && !otaUploadActive) {
          IPAddress remIp = udpTelemetry.remoteIP();
          uint16_t remPort = udpTelemetry.remotePort();
          otaPrepPending = true;
          otaPrepMs = millis();
          if (!audioDspStopForOta()) {
            telemetryLogLine("[OTA] Core 1 pause timed out; withholding OTA_READY.");
            return;
          }
          udpTelemetry.beginPacket(remIp, remPort);
          udpTelemetry.write(reinterpret_cast<const uint8_t*>("OTA_READY"), 9);
          udpTelemetry.endPacket();
          telemetryLogLine("[OTA] Pre-OTA handshake: Core 1 I2S stopped.");
        }
      }
    }

    ArduinoOTA.handle();
    if (otaResumePending) {
      otaResumePending = false;
      audioDspResumeAfterOta();
    }
  } else {
    if (wifiState == WIFI_STATE_CONNECTED) {
      wifiState = WIFI_STATE_DISCONNECTED;
      Serial.println(F("[WIFI] Connection lost. Lighting loop unaffected; will retry."));
      lastConnectAttemptMs = millis();
    }

    if (millis() - lastConnectAttemptMs >= WIFI_RETRY_MS) {
      WiFi.disconnect();
      startWifiAttempt();
    }
  }
}

bool otaManagerIsConnected() {
  return wifiState == WIFI_STATE_CONNECTED;
}

bool otaManagerIsFsReady() {
  return fsReady;
}

void telemetryLogLine(const char* line) {
  if (!line) return;

  Serial.println(line);

  if (wifiState == WIFI_STATE_CONNECTED && otaStarted) {
    IPAddress bcast(static_cast<uint32_t>(WiFi.localIP()) |
                    ~static_cast<uint32_t>(WiFi.subnetMask()));
    if (udpTelemetry.beginPacket(bcast, UDP_TELEMETRY_PORT)) {
      udpTelemetry.write(reinterpret_cast<const uint8_t*>(line), strlen(line));
      udpTelemetry.write('\n');
      udpTelemetry.endPacket();
    }
  }
}
