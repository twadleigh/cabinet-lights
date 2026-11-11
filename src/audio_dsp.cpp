#include "audio_dsp.h"
#include "config.h"
#pragma GCC diagnostic push
// Arduino-Pico I2S/Stream hides Print::write(uint8_t) in the dependency API.
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#include <I2S.h>
#pragma GCC diagnostic pop
#include <atomic>

static I2S i2s(INPUT);
// Atomic payload fields keep the seqlock free of C++ data races as well as
// preventing torn multi-band snapshots across the two physical cores.
static_assert(std::atomic<float>::is_always_lock_free, "Audio snapshots must be lock-free");
static_assert(std::atomic<uint32_t>::is_always_lock_free, "Audio sequence must be lock-free");
static std::atomic<AudioDspStatus> dspStatus{AUDIO_DSP_UNINIT};
static std::atomic<bool> otaPauseRequested{false};
static std::atomic<bool> otaPausedAck{false};
static std::atomic<uint32_t> pauseGeneration{0};
static std::atomic<uint32_t> pausedGeneration{0};
static std::atomic<uint32_t> energySequence{0};
static std::atomic<float> sharedEnergy[4]{};
static float dcStateX = 0.0f;
static float dcStateY = 0.0f;
static float peakHold = 0.0f;
static constexpr float PEAK_RELEASE = 0.9998750078121745f; // exp(-1/(16000 * 0.5s))
static std::atomic<uint32_t> processedSamples{0};

static void publishEnergy() {
  energySequence.fetch_add(1);
  __dmb();
  sharedEnergy[0].store(peakHold);
  sharedEnergy[1].store(peakHold * 0.7f);
  sharedEnergy[2].store(peakHold * 0.5f);
  sharedEnergy[3].store(peakHold * 0.3f);
  __dmb();
  energySequence.fetch_add(1);
}

void audioDspCore1Init() {
  i2s.setBCLK(PIN_I2S_BCLK);
  i2s.setDATA(PIN_I2S_DATA);
  i2s.setBitsPerSample(32);

  // Strict Dual-Core Safety: Never call Serial from Core 1.
  // Report initialization state to Core 0 via atomic status flag.
  if (!i2s.begin(16000)) {
    dspStatus = AUDIO_DSP_FAULT;
    return; // Keep loop1 alive to acknowledge OTA even after an I2S fault.
  }
  dspStatus = AUDIO_DSP_ONLINE;
}

void audioDspCore1Update() {
  if (otaPauseRequested) {
    const uint32_t requestedGeneration = pauseGeneration.load();
    if (!otaPausedAck || pausedGeneration.load() != requestedGeneration) {
      if (dspStatus == AUDIO_DSP_ONLINE) {
        // End on the owning core: never free AudioBufferManager buffers while
        // Core 1 is inside read32(). Also disables DMA IRQs on the correct NVIC
        // and leaves CYW43 Wi-Fi DMA channels intact.
        i2s.end();
      }
      dcStateX = dcStateY = peakHold = 0.0f;
      publishEnergy();
      dspStatus = AUDIO_DSP_PAUSED;
      pausedGeneration = requestedGeneration;
      otaPausedAck = true;
    }
    delay(5);
    return;
  }

  if (otaPausedAck) {
    audioDspCore1Init(); // Restart I2S only on its owning core.
    otaPausedAck = false;
  }

  if (dspStatus != AUDIO_DSP_ONLINE) {
    return;
  }

  int32_t leftSample = 0;
  int32_t rightSample = 0;

  // Non-blocking drain of available PIO FIFO frames
  int count = 0;
  while (i2s.available() >= 8 && count < 128) {
    i2s.read32(&leftSample, &rightSample);
    count++;
    float x = (float)(leftSample >> 16);
    float y = x - dcStateX + 0.995f * dcStateY;
    dcStateX = x;
    dcStateY = y;

    float absY = fabsf(y) / 8192.0f;
    if (absY > peakHold) {
      peakHold = (absY > 1.0f) ? 1.0f : absY;
    } else {
      peakHold *= PEAK_RELEASE; // 0.5s release; telemetry samples this envelope.
    }
  }

  processedSamples.fetch_add(static_cast<uint32_t>(count));
  publishEnergy();
}

uint32_t audioDspProcessedSamples() { return processedSamples.load(); }

bool audioDspStopForOta() {
  // A fresh generation prevents a stale ACK from a just-cancelled PREP being
  // mistaken for a new pause while Core 1 is restarting I2S.
  const uint32_t generation = pauseGeneration.fetch_add(1) + 1;
  otaPauseRequested = true;
  uint32_t startMs = millis();
  while (uint32_t(millis() - startMs) < 200) {
    if (otaPausedAck && pausedGeneration.load() == generation) return true;
    delay(1);
  }
  return false;
}

void audioDspResumeAfterOta() {
  otaPauseRequested = false;
}

AudioDspStatus getAudioDspStatus() {
  return dspStatus;
}

void getAudioEnergySnapshot(AudioEnergy& snapshot) {
  for (;;) {
    uint32_t before = energySequence.load();
    if (before & 1u) continue;
    __dmb();
    AudioEnergy candidate{sharedEnergy[0].load(), sharedEnergy[1].load(),
                          sharedEnergy[2].load(), sharedEnergy[3].load()};
    __dmb();
    if (before == energySequence.load()) {
      snapshot = candidate;
      return;
    }
  }
}
