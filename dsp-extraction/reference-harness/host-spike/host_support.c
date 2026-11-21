#include <stdint.h>
#include "Soundengine/DSPAudio/random.h"

uint32_t GetRngValue(void) {
  static uint32_t state = 0x13579BDFu;
  state = state * 1664525u + 1013904223u;
  return state;
}

float midiParser_calcDetune(uint8_t value) {
  (void)value;
  /* Link-only placeholder. Exact firmware detune behavior is not characterized here. */
  return 1.0f;
}
