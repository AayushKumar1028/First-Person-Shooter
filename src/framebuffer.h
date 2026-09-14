// framebuffer.h - software render target (ARGB8888) with small drawing helpers.
#pragma once

#include "core.h"

namespace fps {

struct Framebuffer {
  int w = 0;
  int h = 0;
  std::vector<RGBA> px;

  void resize(int newW, int newH);
  void clear(RGBA c);

  inline RGBA* row(int y) { return px.data() + size_t(y) * size_t(w); }
  inline const RGBA* row(int y) const { return px.data() + size_t(y) * size_t(w); }

  inline void set(int x, int y, RGBA c) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    px[size_t(y) * size_t(w) + size_t(x)] = c;
  }
  inline RGBA get(int x, int y) const {
    if (x < 0 || y < 0 || x >= w || y >= h) return 0;
    return px[size_t(y) * size_t(w) + size_t(x)];
  }
  // Source-over blend of "c" at coverage "a" (0..1).
  inline void blend(int x, int y, RGBA c, float a) {
    if (a <= 0.0f || x < 0 || y < 0 || x >= w || y >= h) return;
    const size_t i = size_t(y) * size_t(w) + size_t(x);
    RGBA& dst = px[i];
    if (a >= 1.0f) {
      dst = c;
      return;
    }
    const float inv = 1.0f - a;
    const int r = clampi(int(float(redOf(c)) * a + float(redOf(dst)) * inv), 0, 255);
    const int g = clampi(int(float(greenOf(c)) * a + float(greenOf(dst)) * inv), 0, 255);
    const int b = clampi(int(float(blueOf(c)) * a + float(blueOf(dst)) * inv), 0, 255);
    dst = rgba(r, g, b);
  }

  void fillRect(int x, int y, int rw, int rh, RGBA c);
  void fillRectBlend(int x, int y, int rw, int rh, RGBA c, float a);
  void hline(int x, int y, int len, RGBA c);
  void vline(int x, int y, int len, RGBA c);
  void rectOutline(int x, int y, int rw, int rh, RGBA c);
  // Vertical gradient fill, used for menus and skies.
  void vGradient(int x, int y, int rw, int rh, RGBA top, RGBA bottom);
};

}  // namespace fps
