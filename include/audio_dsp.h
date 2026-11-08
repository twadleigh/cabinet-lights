#pragma once
#include <Arduino.h>

enum AudioDspStatus : uint8_t {
  AUDIO_DSP_UNINIT = 0,
  AUDIO_DSP_ONLINE = 1,
  AUDIO_DSP_FAULT  = 2
};

struct AudioEnergy {
  float subBass;
  float lowMid;
  float highMid;
  float treble;
};

void audioDspCore1Init();
void audioDspCore1Update();
AudioDspStatus getAudioDspStatus();
void getAudioEnergySnapshot(AudioEnergy& snapshot);
