#include "audio_dsp.h"
#include "config.h"
#include <I2S.h>

static I2S i2s(INPUT);
static volatile AudioDspStatus dspStatus = AUDIO_DSP_UNINIT;
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

void audioDspCore1Update() {
  if (dspStatus != AUDIO_DSP_ONLINE) {
    return;
  }

  int32_t leftSample = 0;
  int32_t rightSample = 0;

  // Drain frames from the PIO FIFO
  for (int i = 0; i < 64; i++) {
    i2s.read32(&leftSample, &rightSample);
  }

  // Placeholder energy integration (to be replaced with biquad filter bank in Stage 2)
  int16_t sample16 = abs(leftSample >> 16);
  float norm = constrain((float)sample16 / 16384.0f, 0.0f, 1.0f);

  sharedEnergy.subBass = norm;
  sharedEnergy.lowMid  = norm * 0.7f;
  sharedEnergy.highMid = norm * 0.5f;
  sharedEnergy.treble  = norm * 0.3f;
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
