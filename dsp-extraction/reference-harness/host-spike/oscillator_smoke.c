#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "Soundengine/DSPAudio/Oscillator.h"

int main(void) {
  int16_t output[8] = {0};
  OscInfo osc = {0};

  osc.waveform = SINE;
  osc.phase = 0u;
  osc.phaseInc = 0x10000000u;
  calcNextOscSampleBlock(&osc, output, 8u, 1.0f, 0.5f);
  assert(osc.phase == 0x80000000u);

  osc.waveform = SAW;
  osc.phase = 0u;
  osc.phaseInc = 0x00400000u;
  osc.tableOffset = 0u;
  calcNextOscSampleBlock(&osc, output, 8u, 1.0f, 0.5f);
  assert(osc.phase == 8u * 0x00400000u);

  /* Deterministic RNG seam and phase-wrap path. */
  osc.waveform = NOISE;
  osc.phase = 0u;
  osc.phaseInc = 0x80000000u;
  calcNextOscSampleBlock(&osc, output, 2u, 1.0f, 0.5f);

  puts("oscillator smoke test passed");
  return 0;
}
