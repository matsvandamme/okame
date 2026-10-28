#include "frame.h"
#include "font.h"

void Frame::invert() {
  for (auto& b : bits_) b = !b;
}

void Frame::hline(int x, int y, int w, bool on) {
  for (int i = 0; i < w; i++) set(x + i, y, on);
}

void Frame::vline(int x, int y, int h, bool on) {
  for (int i = 0; i < h; i++) set(x, y + i, on);
}

void Frame::rect(int x, int y, int w, int h, bool on) {
  hline(x, y, w, on);
  hline(x, y + h - 1, w, on);
  vline(x, y, h, on);
  vline(x + w - 1, y, h, on);
}

void Frame::fillRect(int x, int y, int w, int h, bool on) {
  for (int j = 0; j < h; j++) hline(x, y + j, w, on);
}

static inline bool bitAt(const uint8_t* rows, int w, int x, int y) {
  int stride = (w + 7) / 8;
  return rows[y * stride + x / 8] & (0x80 >> (x % 8));
}

static bool patternDot(Pattern p, int x, int y) {
  switch (p) {
    case Pattern::Solid: return true;
    case Pattern::Pearl: return (x + y) % 2 == 0;
    case Pattern::Pied: return ((x >> 1) + (y >> 1)) % 2 == 0;
    case Pattern::Scruffy: return (x * 7 + y * 3) % 5 != 0;
    case Pattern::Clear: return false;
  }
  return true;
}

void Frame::sprite(const Sprite& s, int x, int y, bool flip, Pattern pat, int scale) {
  for (int sy = 0; sy < s.h; sy++) {
    for (int sx = 0; sx < s.w; sx++) {
      if (!bitAt(s.mask, s.w, sx, sy)) continue;
      bool on = bitAt(s.ink, s.w, sx, sy);
      if (s.pat && bitAt(s.pat, s.w, sx, sy)) on = patternDot(pat, sx, sy);
      int dx = flip ? (s.w - 1 - sx) : sx;
      for (int j = 0; j < scale; j++)
        for (int i = 0; i < scale; i++) set(x + dx * scale + i, y + sy * scale + j, on);
    }
  }
}

void Frame::spriteInverse(const Sprite& s, int x, int y, bool flip) {
  for (int sy = 0; sy < s.h; sy++) {
    for (int sx = 0; sx < s.w; sx++) {
      if (!bitAt(s.mask, s.w, sx, sy)) continue;
      int dx = flip ? (s.w - 1 - sx) : sx;
      set(x + dx, y + sy, !bitAt(s.ink, s.w, sx, sy));
    }
  }
}

int Frame::text(const char* s, int x, int y, int scale, bool on) {
  for (; *s; s++) {
    if (*s == ' ') {
      x += 3 * scale;
      continue;
    }
    uint16_t g = glyph3x5(*s);
    for (int gy = 0; gy < 5; gy++)
      for (int gx = 0; gx < 3; gx++)
        if (g & (1 << (14 - (gy * 3 + gx))))
          for (int j = 0; j < scale; j++)
            for (int i = 0; i < scale; i++) set(x + gx * scale + i, y + gy * scale + j, on);
    x += 4 * scale;
  }
  return x;
}

int Frame::textWidth(const char* s, int scale) {
  int w = 0;
  for (; *s; s++) w += (*s == ' ' ? 3 : 4) * scale;
  return w > 0 ? w - scale : 0;
}

void Frame::pack(uint8_t out[LCD_W * LCD_H / 8]) const {
  memset(out, 0, LCD_W * LCD_H / 8);
  for (int i = 0; i < LCD_W * LCD_H; i++)
    if (bits_[i]) out[i / 8] |= 0x80 >> (i % 8);
}
