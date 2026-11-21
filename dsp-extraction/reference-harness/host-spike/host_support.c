#include <stdint.h>
#include "Soundengine/DSPAudio/random.h"

uint32_t GetRngValue(void) {
  static uint32_t state = 0x13579BDFu;
  state = state * 1664525u + 1013904223u;
  return state;
}

float midiParser_calcDetune(uint8_t value) {
  const float normalized = (float)value / 127.0f - 0.5f;
  /* Exact firmware detune law will be substituted/verified in characterization. */
  return normalized < 0.0f
    ? 1.0f + normalized * (1.0f - 0.9438743127f) / 0.5f
    : 1.0f + normalized * (1.0594630944f - 1.0f) / 0.5f;
}
