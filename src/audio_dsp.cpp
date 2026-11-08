#include "audio_dsp.h"
#include "config.h"
#include <I2S.h>

static I2S i2s(INPUT);
static volatile AudioDspStatus dspStatus = AUDIO_DSP_UNINIT;
static volatile bool otaPauseRequested = false;
static volatile bool otaPausedAck = false;
static volatile AudioEnergy sharedEnergy = {0.0f, 0.0f, 0.0f, 0.0f};

void audioDspCore1Init() {
  i2s.setBCLK(PIN_I2S_BCLK);
  i2s.setDATA(PIN_I2S_DATA);
  i2s.setBitsPerSample(32);

  // Strict Dual-Core Safety: Never call Serial from Core 1.
  // Report initialization state to Core 0 via volatile status flag.
  if (!i2s.begin(16000)) {
    dspStatus = AUDIO_DSP_FAULT;
    while (1) {
      delay(100);
    }
  }
  dspStatus = AUDIO_DSP_ONLINE;
}

static float dcStateX = 0.0f;
static float dcStateY = 0.0f;
static float peakHold = 0.0f;

void audioDspCore1Update() {
  if (otaPauseRequested) {
    if (!otaPausedAck) {
      if (dspStatus == AUDIO_DSP_ONLINE) {
        // Must call i2s.end() on Core 1 (the core that called i2s.begin())
        // so irq_set_enabled(DMA_IRQ_0, false) disables DMA_IRQ_0 on Core 1's NVIC
        // and only unclaims the two I2S DMA channels (leaving CYW43 Wi-Fi DMA intact).
        i2s.end();
      }
      otaPausedAck = true;
    }
    delay(5);
    return;
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
      peakHold *= 0.9995f; // ~0.5s exponential decay at 16 kHz so 1 Hz telemetry catches claps
    }
  }

  sharedEnergy.subBass = peakHold;
  sharedEnergy.lowMid  = peakHold * 0.7f;
  sharedEnergy.highMid = peakHold * 0.5f;
  sharedEnergy.treble  = peakHold * 0.3f;
}

void audioDspStopForOta() {
  otaPauseRequested = true;
  unsigned long startMs = millis();
  while (!otaPausedAck && (millis() - startMs < 200)) {
    delay(1);
  }
}

AudioDspStatus getAudioDspStatus() {
  return dspStatus;
}

void getAudioEnergySnapshot(AudioEnergy& snapshot) {
  snapshot.subBass = sharedEnergy.subBass;
  snapshot.lowMid  = sharedEnergy.lowMid;
  snapshot.highMid = sharedEnergy.highMid;
  snapshot.treble  = sharedEnergy.treble;
}
