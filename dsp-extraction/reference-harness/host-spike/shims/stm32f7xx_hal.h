#ifndef LXR_HOST_SPIKE_STM32_HAL_H
#define LXR_HOST_SPIKE_STM32_HAL_H

#include <stdint.h>

typedef struct { uint32_t unused; } RNG_HandleTypeDef;

/* Minimal APSR surface retained for the legacy single-sample noise function. */
typedef union {
  uint32_t w;
  struct { uint32_t _reserved : 28; uint32_t V : 1; uint32_t _top : 3; } b;
} APSR_Type;

static inline uint32_t __get_APSR(void) { return 0u; }

static inline uint32_t __CLZ(uint32_t value) {
  return value == 0u ? 32u : (uint32_t)__builtin_clz(value);
}

/* Match the low signed-16 lane used by this source's scalar call sites. */
static inline int32_t __QSUB16(int32_t lhs, int32_t rhs) {
  int32_t result = (int16_t)lhs - (int16_t)rhs;
  if (result > 32767) result = 32767;
  if (result < -32768) result = -32768;
  return (int16_t)result;
}

#endif
