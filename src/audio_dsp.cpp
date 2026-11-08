#include "audio_dsp.h"
#include "config.h"
#include <I2S.h>

static I2S i2s(INPUT);
static volatile AudioEnergy sharedEnergy = {0.0f, 0.0f, 0.0f, 0.0f};

void audioDspCore1Init() {
  i2s.setBCLK(PIN_I2S_BCLK);
  i2s.setDATA(PIN_I2S_DATA);
  i2s.setBitsPerSample(32);

  if (!i2s.begin(16000)) {
    Serial.println(F("[CORE 1] Failed to initialize I2S interface!"));
    while (1) { delay(100); }
  }
  Serial.println(F("[CORE 1] INMP441 I2S Task Running"));
}

void audioDspCore1Update() {
  int32_t leftSample = 0;
  int32_t rightSample = 0;

  // Drain frames from the PIO FIFO
  for (int i = 0; i < 64; i++) {
    i2s.read32(&leftSample, &rightSample);
  }

  // Placeholder energy integration (to be replaced with biquad filter bank)
  int16_t sample16 = abs(leftSample >> 16);
  float norm = constrain((float)sample16 / 16384.0f, 0.0f, 1.0f);

  sharedEnergy.subBass = norm;
  sharedEnergy.lowMid  = norm * 0.7f;
  sharedEnergy.highMid = norm * 0.5f;
  sharedEnergy.treble  = norm * 0.3f;
}

void getAudioEnergySnapshot(AudioEnergy& snapshot) {
  snapshot.subBass = sharedEnergy.subBass;
  snapshot.lowMid  = sharedEnergy.lowMid;
  snapshot.highMid = sharedEnergy.highMid;
  snapshot.treble  = sharedEnergy.treble;
}