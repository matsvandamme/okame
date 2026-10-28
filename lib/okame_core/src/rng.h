#pragma once
#include <stdint.h>

struct Rng {
  uint32_t s;
  explicit Rng(uint32_t seed = 0x9E3779B9) : s(seed ? seed : 1) {}
  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }
  uint32_t range(uint32_t lo, uint32_t hi) { return lo + next() % (hi - lo + 1); }  // inclusive
};
