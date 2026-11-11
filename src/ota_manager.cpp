#include "ota_manager.h"
#include "config.h"
#include "audio_dsp.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <cabinet_ota.h>
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
static std::atomic<bool> bootReady{false};
static std::atomic<bool> fsHealthy{false};
static bool telemetryStarted = false;
static uint32_t connectCount = 0;
static bool otaPrepPending = false;
static bool otaUploadActive = false;
static bool otaResumePending = false;
static uint32_t otaPrepMs = 0;
static constexpr unsigned long OTA_PREP_TIMEOUT_MS = 15000UL;
static uint32_t lastConnectAttemptMs = 0;
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
  if (!telemetryStarted) {
    telemetryStarted = udpTelemetry.begin(UDP_TELEMETRY_PORT) != 0;
  }
  if (otaStarted || !fsHealthy) return;

  CabinetOTA.setPort(OTA_PORT);
  CabinetOTA.setHostname(OTA_HOSTNAME);
#ifdef OTA_PASSWORD
  if (strlen(OTA_PASSWORD) > 0) CabinetOTA.setPassword(OTA_PASSWORD);
#endif

  CabinetOTA.onBeforeUpdate([]() {
    // This hook runs after authentication but BEFORE LittleFS/Updater access.
    if (!fsHealthy || !audioDspStopForOta()) {
      telemetryLogLine("[OTA] Update rejected: storage unavailable or audio pause timed out.");
      return false;
    }
    otaUploadActive = true;
    otaPrepPending = false;
    return true;
  });
  CabinetOTA.onStart([]() {
    telemetryLogLine("[OTA] Core 1 I2S stopped. Receiving firmware...");
  });

  CabinetOTA.onEnd([]() {
    telemetryLogLine("[OTA] Firmware staged to LittleFS. Rebooting...");
  });

  CabinetOTA.onError([](ota_error_t error) {
    char msg[64];
    snprintf(msg, sizeof(msg), "[OTA] Error (%u)", static_cast<unsigned>(error));
    telemetryLogLine(msg);
    otaUploadActive = false;
    otaPrepPending = false;
    // ArduinoOTA may still close/write its Update file after this callback.
    // Restart DMA only once handle() has returned and flash cleanup is done.
    otaResumePending = true;
  });

  CabinetOTA.begin();
  otaStarted = true;
}

void otaManagerInit() {
  // LittleFS.begin auto-formats on mount failure in the pinned core. All boot
  // flash work finishes before bootReady releases Core 1, even on failure.
  fsHealthy = LittleFS.begin();
  if (fsHealthy && LittleFS.exists("/firmware.bin")) {
    fsHealthy = LittleFS.remove("/firmware.bin");
  }
  if (!fsHealthy) Serial.println(F("[FS] Initialization/cleanup failed; OTA disabled."));
  bootReady = true;

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
  if (otaPrepPending && !otaUploadActive && uint32_t(millis() - otaPrepMs) >= OTA_PREP_TIMEOUT_MS) {
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
      ++connectCount;
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
        if (n == 8 && pktSize == 8 && strcmp(cmdBuf, "OTA_PREP") == 0 && fsHealthy && otaStarted && !otaUploadActive) {
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

    if (otaStarted) CabinetOTA.handle();
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

    if (uint32_t(millis() - lastConnectAttemptMs) >= WIFI_RETRY_MS) {
      WiFi.disconnect();
      startWifiAttempt();
    }
  }
}

bool otaManagerIsConnected() {
  return wifiState == WIFI_STATE_CONNECTED;
}

bool otaManagerIsBootReady() { return bootReady; }
bool otaManagerIsFsHealthy() { return fsHealthy; }
uint32_t otaManagerReconnectCount() { return connectCount ? connectCount - 1 : 0; }
const char* otaManagerState() {
  if (!fsHealthy) return "STORAGE_FAULT";
  if (otaUploadActive) return "UPLOADING";
  if (otaPrepPending) return "PREPARED";
  return otaStarted && wifiState == WIFI_STATE_CONNECTED ? "READY" : "OFFLINE";
}

void telemetryLogLine(const char* line) {
  if (!line) return;

  Serial.println(line);

  if (wifiState == WIFI_STATE_CONNECTED && telemetryStarted) {
    IPAddress bcast(static_cast<uint32_t>(WiFi.localIP()) |
                    ~static_cast<uint32_t>(WiFi.subnetMask()));
    if (udpTelemetry.beginPacket(bcast, UDP_TELEMETRY_PORT)) {
      udpTelemetry.write(reinterpret_cast<const uint8_t*>(line), strlen(line));
      udpTelemetry.write('\n');
      udpTelemetry.endPacket();
    }
  }
}
