#include "render.h"

template <typename T, typename Conv>
static void renderT(const Frame& f, T* out, const LcdPalette& p, Conv conv) {
  const T bg = conv(p.bg), ink = conv(p.ink), ghost = conv(p.ghost), shadow = conv(p.shadow);
  for (int y = 0; y < PX_H; y++) {
    int ly = y / 4, sy = y % 4;
    for (int x = 0; x < PX_W; x++) {
      int lx = x / 4, sx = x % 4;
      T c;
      if (sx < 3 && sy < 3) {
        c = f.get(lx, ly) ? ink : ghost;
      } else {
        // Gap pixel: shadowed when the dot up-left of it is lit.
        bool lit = f.get(lx, ly) && (sx == 3 || sy == 3);
        c = lit ? shadow : bg;
      }
      out[y * PX_W + x] = c;
    }
  }
}

void renderRgb888(const Frame& f, uint32_t* out, const LcdPalette& p) {
  renderT(f, out, p, [](uint32_t c) { return c; });
}

void renderRgb565(const Frame& f, uint16_t* out, const LcdPalette& p) {
  renderT(f, out, p, [](uint32_t c) { return rgb565(c); });
}
