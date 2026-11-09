#pragma once
#include <Arduino.h>

enum AudioDspStatus : uint8_t {
  AUDIO_DSP_UNINIT = 0,
  AUDIO_DSP_ONLINE = 1,
  AUDIO_DSP_FAULT  = 2,
  AUDIO_DSP_PAUSED = 3
};

struct AudioEnergy {
  float subBass;
  float lowMid;
  float highMid;
  float treble;
};

void audioDspCore1Init();
void audioDspCore1Update();
// Returns true only after Core 1 has stopped I2S DMA.
bool audioDspStopForOta();
void audioDspResumeAfterOta();
AudioDspStatus getAudioDspStatus();
void getAudioEnergySnapshot(AudioEnergy& snapshot);
