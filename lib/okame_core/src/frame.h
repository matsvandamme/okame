#pragma once
#include <stdint.h>
#include <string.h>
#include "sprite.h"

// The logical LCD: 40 x 20 dots, one bit each.
constexpr int LCD_W = 40;
constexpr int LCD_H = 20;

enum class Pattern : uint8_t { Solid, Pearl, Pied, Scruffy, Clear };

class Frame {
 public:
  void clear() { memset(bits_, 0, sizeof bits_); }
  void fill() { memset(bits_, 0xff, sizeof bits_); }
  bool get(int x, int y) const {
    if (x < 0 || y < 0 || x >= LCD_W || y >= LCD_H) return false;
    return bits_[y * LCD_W + x];
  }
  void set(int x, int y, bool on) {
    if (x < 0 || y < 0 || x >= LCD_W || y >= LCD_H) return;
    bits_[y * LCD_W + x] = on;
  }
  void invert();
  void hline(int x, int y, int w, bool on = true);
  void vline(int x, int y, int h, bool on = true);
  void rect(int x, int y, int w, int h, bool on = true);
  void fillRect(int x, int y, int w, int h, bool on = true);

  // Draw a sprite with its top-left at (x, y); flip mirrors horizontally.
  void sprite(const Sprite& s, int x, int y, bool flip = false, Pattern pat = Pattern::Solid,
              int scale = 1);
  // Draw with ink and clear swapped (white-on-black), for use on a filled background.
  void spriteInverse(const Sprite& s, int x, int y, bool flip = false);

  // 3x5 font. Returns the x after the last glyph. scale 2 doubles each dot.
  int text(const char* s, int x, int y, int scale = 1, bool on = true);
  static int textWidth(const char* s, int scale = 1);
  void textCentered(const char* s, int y, int scale = 1, bool on = true) {
    text(s, (LCD_W - textWidth(s, scale)) / 2, y, scale, on);
  }

  // 100-byte packed copy (row-major, MSB first) for the web page mirror.
  void pack(uint8_t out[LCD_W * LCD_H / 8]) const;
  void unpack(const uint8_t in[LCD_W * LCD_H / 8]);

 private:
  uint8_t bits_[LCD_W * LCD_H];
};
