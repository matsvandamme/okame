#pragma once
#include <stdint.h>

// 1-bit sprite: rows packed MSB-first, (w + 7) / 8 bytes per row.
// ink = dark dots, mask = dots the sprite owns (ink, forced-clear, pattern),
// pat = wing dots filled according to the bird's colour variant.
struct Sprite {
  uint8_t w, h;
  const uint8_t* ink;
  const uint8_t* mask;
  const uint8_t* pat;
};
