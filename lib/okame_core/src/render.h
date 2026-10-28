#pragma once
#include <stdint.h>
#include "frame.h"

// Physical LCD size: each logical dot becomes a 4x4 cell (3x3 dot + 1 px gap).
constexpr int PX_W = LCD_W * 4;
constexpr int PX_H = LCD_H * 4;

struct LcdPalette {
  uint32_t bg = 0x9EAA8A;      // reflective background
  uint32_t ink = 0x1E2418;     // lit dot
  uint32_t ghost = 0x929E7F;   // unlit dot
  uint32_t shadow = 0x8A967A;  // shadow an ink dot casts on the background
};

constexpr uint16_t rgb565(uint32_t c) {
  return ((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F);
}

// Render to a PX_W x PX_H buffer of 0xRRGGBB (sim) or RGB565 (device).
void renderRgb888(const Frame& f, uint32_t* out, const LcdPalette& p = LcdPalette());
void renderRgb565(const Frame& f, uint16_t* out, const LcdPalette& p = LcdPalette());
