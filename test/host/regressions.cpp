#include <cassert>
#include <iostream>
#include "../../src/led_driver.cpp"
#include "../../src/door_engine.cpp"
#include "../../src/audio_dsp.cpp"
#include "../../src/button_manager.cpp"
void telemetryLogLine(const char*) {}

static void testPwm() {
  ledDriverInit();
  assert(testSliceMask == ((1u << 0) | (1u << 1) | (1u << 2) | (1u << 5)));
  for (unsigned s : {0, 1, 2, 5}) {
    assert(testConfigs[s].wrap == 7499 && testConfigs[s].div == 1);
  }
  // One complete Q0.16 cycle must conserve the commanded mean, while never
  // generating a mismatched pulse between 1 and 29 ticks.
  for (float lightness : {0.0f, 0.01f, 0.04f, 0.05f, 0.07f, 0.55f, 0.999f, 1.0f}) {
    setAllOutputsOff();
    StripColor color{lightness, lightness, lightness, lightness};
    setStripOutputs(color, color);
    uint64_t total = 0;
    for (unsigned n = 0; n < 65536; ++n) {
      pwmWrapHandler();
      uint16_t level = testLevels[0][0];
      assert(level == 0 || (level >= 30 && level <= 7500));
      for (unsigned s : {0, 1, 2, 5}) {
        assert(testLevels[s][0] == level && testLevels[s][1] == level);
      }
      total += level;
    }
    float active = std::pow(lightness, 2.4f) * 7480;
    float expected = lightness == 0 ? 0 : (active < 10 ? 30 * active / 10 : 20 + active);
    assert(std::fabs(static_cast<double>(total) / 65536 - expected) < 0.001);
  }
  // Verify public W/R/G/B layout maps to physical W/B/R/G output order.
  setStripOutputs({1, 0, 0, 0}, {0, 1, 0, 0});
  pwmWrapHandler();
  assert(testLevels[0][0] == 7500 && testLevels[0][1] == 0);
  assert(testLevels[5][0] == 7500 && testLevels[2][0] == 0);
  setAllOutputsOff();
  pwmWrapHandler();
  for (unsigned s : {0, 1, 2, 5}) assert(testLevels[s][0] == 0 && testLevels[s][1] == 0);
}

static void testDoors() {
  testMillis = 0;
  doorEngineInit(false);
  assert(doorEngineGetAmbientBrightness() == 0.09f);
  StripColor a, b;
  doorEngineSetDoorState(true);
  testMillis = 2000;
  doorEngineUpdate(a, b);
  assert(std::fabs(doorProgress - 0.4f) < 1e-6f);
  doorEngineSetDoorState(false);
  testMillis = 3000;
  doorEngineUpdate(a, b);
  assert(std::fabs(doorProgress - 0.2f) < 1e-6f);
  testMillis = 4000;
  doorEngineUpdate(a, b);
  assert(doorProgress == 0);
  doorEngineHandleButton(BTN_EVENT_MODE_NEXT);
  testMillis = 7000;
  doorEngineUpdate(a, b);
  float hueBefore = currentHue;
  doorEngineHandleButton(BTN_EVENT_MODE_PREV);
  doorEngineUpdate(a, b);
  assert(currentHue == hueBefore); // Returning from Debug must not restart hue.
  for (int i = 0; i < 20; ++i) doorEngineHandleButton(BTN_EVENT_BRIGHT_DOWN);
  assert(doorEngineGetAmbientBrightness() == 0);
  for (int i = 0; i < 20; ++i) doorEngineHandleButton(BTN_EVENT_BRIGHT_UP);
  assert(doorEngineGetAmbientBrightness() == 1);
}

static void testAudio() {
  testDelayHook = audioDspCore1Update;
  audioDspCore1Init();
  assert(getAudioDspStatus() == AUDIO_DSP_ONLINE);
  otaPauseRequested = true;
  audioDspCore1Update();
  assert(audioDspStopForOta());
  assert(testI2sEndCount == 1 && getAudioDspStatus() == AUDIO_DSP_PAUSED);
  audioDspCore1Update();
  assert(testI2sEndCount == 1); // Repeated PREP must be idempotent.
  const uint32_t oldGeneration = pausedGeneration.load();
  audioDspResumeAfterOta();
  assert(audioDspStopForOta()); // Immediate retry cannot accept the old ACK.
  assert(pausedGeneration.load() != oldGeneration);
  audioDspResumeAfterOta();
  audioDspCore1Update();
  assert(getAudioDspStatus() == AUDIO_DSP_ONLINE && !otaPausedAck);
  testI2sStarts = false;
  audioDspCore1Init();
  assert(getAudioDspStatus() == AUDIO_DSP_FAULT);
  otaPauseRequested = true;
  audioDspCore1Update(); // Faulted I2S still acknowledges a pause.
  assert(audioDspStopForOta());
  testI2sStarts = true;
  audioDspResumeAfterOta();
  audioDspCore1Update();
  assert(getAudioDspStatus() == AUDIO_DSP_ONLINE);

  std::atomic<bool> finished{false};
  std::thread writer([&] {
    for (unsigned i = 0; i < 50000; ++i) {
      peakHold = static_cast<float>(i % 1000) / 1000;
      publishEnergy();
    }
    finished = true;
  });
  do {
    AudioEnergy snapshot;
    getAudioEnergySnapshot(snapshot);
    assert(snapshot.lowMid == snapshot.subBass * 0.7f);
    assert(snapshot.highMid == snapshot.subBass * 0.5f);
    assert(snapshot.treble == snapshot.subBass * 0.3f);
  } while (!finished);
  writer.join();
}

static void testInputs() {
  testMillis = UINT32_MAX - 20;
  for (unsigned p : {0,1,8,9,10,11}) testPins[p] = HIGH;
  buttonManagerInit();
  assert(!getDoorSwitchAggregateOpen()); // Boot closed even with open contacts.
  assert(buttonManagerUpdate() == 0);
  testMillis += 36;
  assert(buttonManagerUpdate() == 0 && getDoorSwitchAggregateOpen());
  testPins[0] = LOW; buttonManagerUpdate(); testMillis += 36; buttonManagerUpdate();
  assert(getDoorSwitchAggregateOpen()); // Either door is enough.
  testPins[1] = LOW; buttonManagerUpdate(); testMillis += 36; buttonManagerUpdate();
  assert(!getDoorSwitchAggregateOpen());
  testPins[8] = testPins[10] = LOW;
  buttonManagerUpdate(); testMillis += 10;
  testPins[8] = testPins[10] = HIGH; buttonManagerUpdate(); testMillis += 10;
  testPins[8] = testPins[10] = LOW; buttonManagerUpdate(); testMillis += 36;
  assert(buttonManagerUpdate() == (buttonEventBit(BTN_EVENT_MODE_NEXT) | buttonEventBit(BTN_EVENT_BRIGHT_UP)));
  testMillis += 1000; assert(buttonManagerUpdate() == 0); // No hold-repeat.
  for (unsigned p : {8,9,10,11}) testPins[p] = HIGH;
  buttonManagerUpdate(); testMillis += 36; buttonManagerUpdate();
  for (unsigned p : {8,9,10,11}) testPins[p] = LOW;
  buttonManagerUpdate(); testMillis += 36;
  assert(buttonManagerUpdate() == 0); // Opposing edges cancel, including at limits.
  testMillis += 36; assert(buttonManagerUpdate() == 0);
}

static void testPcm() {
  dcStateX = dcStateY = peakHold = 0;
  testSamples.push_back(-4096 * 65536);
  audioDspCore1Update();
  assert(dcStateX == -4096 && peakHold == 0.5f); // Signed PCM and immediate attack.
  testSamples.push_back(32767 * 65536);
  audioDspCore1Update(); assert(peakHold == 1);
  dcStateX = dcStateY = 0; peakHold = 1;
  const auto before = audioDspProcessedSamples();
  for (int i=0;i<8000;++i) testSamples.push_back(0);
  while (!testSamples.empty()) audioDspCore1Update();
  assert(std::fabs(peakHold - std::exp(-1.0f)) < 0.0002f);
  assert(audioDspProcessedSamples() - before == 8000);
  for (int i=0;i<8000;++i) testSamples.push_back(1000 * 65536);
  while (!testSamples.empty()) audioDspCore1Update();
  assert(std::fabs(dcStateY) < 0.001f && std::isfinite(peakHold));
  testMillis = UINT32_MAX - 50;
  testDelayHook = nullptr;
  assert(!audioDspStopForOta()); // 200ms timeout crosses uint32 wrap.
  audioDspResumeAfterOta();
  testDelayHook = audioDspCore1Update;
  assert(audioDspStopForOta());
  assert(peakHold == 0);
  audioDspResumeAfterOta(); audioDspCore1Update();
  assert(peakHold == 0);
  testMillis = UINT32_MAX - 1000;
  doorEngineInit(false); doorEngineSetDoorState(true);
  testMillis += 2000;
  StripColor a,b; doorEngineUpdate(a,b);
  assert(std::fabs(doorProgress - 0.4f) < 1e-6f);
}

int main() {
  testPwm(); testDoors(); testAudio(); testInputs(); testPcm();
  std::cout << "PWM, door, and cross-core audio regression checks passed.\n";
}
