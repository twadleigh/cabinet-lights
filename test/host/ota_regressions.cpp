#include <cassert>
#include <iostream>
#include "../../lib/cabinet_ota/src/cabinet_ota.cpp"
#include "../../src/audio_dsp.cpp"
#include "../../src/ota_manager.cpp"
#include "../../src/led_driver.cpp"
#include "../../src/door_engine.cpp"
#include "../../src/button_manager.cpp"
#include "../../src/main.cpp"

static void invite(int cmd = U_FLASH) {
  UdpContext::last->inject(std::to_string(cmd) + " 1234 100 " + std::string(32, 'b') + "\n");
}
static void auth(char c = 'a') {
  UdpContext::last->inject("200 " + std::string(32, 'c') + " " + std::string(32, c) + "\n");
}
static void prep() {
  udpTelemetry.packets.push_back("OTA_PREP");
  otaManagerUpdate();
  assert(otaPrepPending && getAudioDspStatus() == AUDIO_DSP_PAUSED);
}
static void online() {
  audioDspCore1Update();
  assert(getAudioDspStatus() == AUDIO_DSP_ONLINE);
  assert(!otaPauseRequested && !otaUploadActive && !otaPrepPending);
}
int main(int argc, char** argv) {
  assert(argc == 2);
  const std::string scenario = argv[1];
  if (scenario == "no_auth" || scenario == "no_guard") {
    CabinetOTAClass transport;
    if (scenario == "no_auth") transport.onBeforeUpdate([] { return true; });
    transport.begin(false);
    invite(); transport.handle();
    assert(testUpdateBegins == (scenario == "no_auth" ? 1u : 0u));
    std::cout << "Password-disabled/mandatory-guard scenario passed: " << scenario << '\n';
    return 0;
  }
  if (scenario == "fs_failure") testFsSuccess = false;
  if (scenario == "cleanup_failure") { testStaleFile = true; testRemoveSuccess = false; }
  testDelayHook = audioDspCore1Update;
  setup();
  setup1();
  WiFi.link = WL_CONNECTED;
  otaManagerUpdate();
  assert(otaManagerIsBootReady());
  if (!testFsSuccess || !testRemoveSuccess) {
    assert(!otaManagerIsFsHealthy() && !otaStarted && telemetryStarted);
    assert(getAudioDspStatus() == AUDIO_DSP_ONLINE);
    udpTelemetry.packets.push_back("OTA_PREP");
    const auto calls = testFsCalls;
    for (int i=0;i<5;++i) { testMillis += 1000; loop(); }
    assert(testFsCalls == calls && !otaPrepPending);
    assert(!Serial.lines.empty());
  } else {
    assert(otaStarted);
    testFlashCheck = [] { assert(getAudioDspStatus() == AUDIO_DSP_PAUSED && otaPauseRequested); };
    if (scenario == "prep_expiry") {
      testMillis = UINT32_MAX - 100;
      prep();
      WiFi.link = 0;
      testMillis += 15001;
      otaManagerUpdate();
      online();
      WiFi.link = WL_CONNECTED;
      otaManagerUpdate();
      assert(otaManagerReconnectCount() == 1);
    } else if (scenario == "auth_timeout" || scenario == "bad_auth") {
      prep(); invite();
      if (scenario == "bad_auth") auth('z');
      else testMillis += 15001;
      otaManagerUpdate(); online();
      assert(testUpdateBegins == 0);
      invite(); auth(); otaManagerUpdate(); online();
      assert(testUpdateBegins == 1); // Immediate valid retry reaches guarded update.
    } else if (scenario == "wrong_peer") {
      invite(); ++UdpContext::last->peerPort; auth(); otaManagerUpdate();
      assert(testUpdateBegins == 0);
      --UdpContext::last->peerPort; auth(); otaManagerUpdate(); online();
      assert(testUpdateBegins == 1);
    } else if (scenario == "main") {
      for (unsigned p : {8,9,10,11}) testPins[p] = HIGH;
      testPins[0] = HIGH;
      loop(); testMillis += 36; loop();
      assert(systemDoorState == DOOR_STATE_OPEN);
      testPins[8] = testPins[10] = LOW;
      loop(); testMillis += 36; loop();
      assert(currentMode == MODE_DEBUG && brightnessIndex == 5);
      testMillis += 1000; loop();
      bool health = false, running = false;
      for (auto& line : Serial.lines) {
        if (line.find("[HEALTH]") == 0) {
          health = true; assert(line.size() < 319);
          assert(line.find("Reconnects: 0") != std::string::npos);
        }
        if (line.find("[RUNNING] Door:") == 0) running = true;
      }
      assert(health && running);
    } else {
      if (scenario == "pause_failure") testDelayHook = nullptr;
      if (scenario == "begin_failure") testUpdateBeginOk = false;
      if (scenario == "receive_timeout") testClientConnect = true;
      if (scenario == "success" || scenario == "delayed_receive") { testClientConnect = true; testClientBytes = 100; testUpdateEndOk = true; }
      if (scenario == "delayed_receive") {
        testMillis = UINT32_MAX - 500;
        testClientReadyStart = testMillis;
        testClientReadyDelay = 2000;
      }
      const auto started = testMillis;
      const auto calls = testFsCalls;
      invite(scenario == "filesystem_upload" ? U_FS : U_FLASH);
      auth(); otaManagerUpdate();
      if (scenario == "pause_failure" || scenario == "filesystem_upload") {
        assert(testFsCalls == calls && testUpdateBegins == 0);
      } else assert(testUpdateBegins == 1 && testUpdateClosed == 1);
      if (scenario == "receive_timeout") assert(uint32_t(testMillis - started) >= 10000);
      if (scenario != "success" && scenario != "delayed_receive") online();
      else assert(otaPauseRequested); // Successful staging stays paused for reboot.
    }
  }
  std::cout << "OTA/main scenario passed: " << scenario << '\n';
}
