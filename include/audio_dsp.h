#pragma once
#include <Arduino.h>

struct AudioEnergy {
  float subBass;
  float lowMid;
  float highMid;
  float treble;
};

void audioDspCore1Init();
void audioDspCore1Update();
void getAudioEnergySnapshot(AudioEnergy& snapshot);