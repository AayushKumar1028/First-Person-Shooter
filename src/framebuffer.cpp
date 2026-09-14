#include "framebuffer.h"

#include <cstdarg>

namespace fps {

std::string fmt(const char* format, ...) {
  char buffer[512];
  va_list args;
  va_start(args, format);
  std::vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  return std::string(buffer);
}

void Framebuffer::resize(int newW, int newH) {
  w = newW;
  h = newH;
  px.assign(size_t(w) * size_t(h), 0);
}

void Framebuffer::clear(RGBA c) { std::fill(px.begin(), px.end(), c); }

void Framebuffer::fillRect(int x, int y, int rw, int rh, RGBA c) {
  const int x0 = clampi(x, 0, w);
  const int y0 = clampi(y, 0, h);
  const int x1 = clampi(x + rw, 0, w);
  const int y1 = clampi(y + rh, 0, h);
  for (int yy = y0; yy < y1; ++yy) {
    RGBA* dst = row(yy) + x0;
    for (int xx = x0; xx < x1; ++xx) *dst++ = c;
  }
}

void Framebuffer::fillRectBlend(int x, int y, int rw, int rh, RGBA c, float a) {
  const int x0 = clampi(x, 0, w);
  const int y0 = clampi(y, 0, h);
  const int x1 = clampi(x + rw, 0, w);
  const int y1 = clampi(y + rh, 0, h);
  for (int yy = y0; yy < y1; ++yy)
    for (int xx = x0; xx < x1; ++xx) blend(xx, yy, c, a);
}

void Framebuffer::hline(int x, int y, int len, RGBA c) {
  if (y < 0 || y >= h) return;
  const int x0 = clampi(x, 0, w);
  const int x1 = clampi(x + len, 0, w);
  RGBA* dst = row(y);
  for (int xx = x0; xx < x1; ++xx) dst[xx] = c;
}

void Framebuffer::vline(int x, int y, int len, RGBA c) {
  if (x < 0 || x >= w) return;
  const int y0 = clampi(y, 0, h);
  const int y1 = clampi(y + len, 0, h);
  for (int yy = y0; yy < y1; ++yy) row(yy)[x] = c;
}

void Framebuffer::rectOutline(int x, int y, int rw, int rh, RGBA c) {
  hline(x, y, rw, c);
  hline(x, y + rh - 1, rw, c);
  vline(x, y, rh, c);
  vline(x + rw - 1, y, rh, c);
}

void Framebuffer::vGradient(int x, int y, int rw, int rh, RGBA top, RGBA bottom) {
  for (int i = 0; i < rh; ++i) {
    const float t = rh > 1 ? float(i) / float(rh - 1) : 0.0f;
    hline(x, y + i, rw, mixColor(top, bottom, t));
  }
}

}  // namespace fps
