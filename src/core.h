// core.h - shared math, color and random helpers.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace fps {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTau = 6.28318530717958647692f;
constexpr float kDeg2Rad = kPi / 180.0f;

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float minf(float a, float b) { return a < b ? a : b; }
inline float maxf(float a, float b) { return a > b ? a : b; }

// ---------------------------------------------------------------------------
// 2D vector
// ---------------------------------------------------------------------------
struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;

  Vec2() = default;
  Vec2(float x_, float y_) : x(x_), y(y_) {}

  Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
  Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
  Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
  Vec2 operator/(float s) const { return Vec2(x / s, y / s); }
  Vec2 operator-() const { return Vec2(-x, -y); }
  Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
  Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
  Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
};

inline float dot(const Vec2& a, const Vec2& b) { return a.x * b.x + a.y * b.y; }
inline float length(const Vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }
inline float distance(const Vec2& a, const Vec2& b) { return length(b - a); }
inline Vec2 normalize(const Vec2& v) {
  const float l = length(v);
  return l > 1e-6f ? Vec2(v.x / l, v.y / l) : Vec2(0.0f, 0.0f);
}
// Unit vector pointing along "angle" (radians, screen-space y grows downward).
inline Vec2 fromAngle(float angle) { return Vec2(std::cos(angle), std::sin(angle)); }
inline Vec2 rotated(const Vec2& v, float angle) {
  const float c = std::cos(angle), s = std::sin(angle);
  return Vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}
// Shortest signed difference between two angles.
inline float angleDelta(float from, float to) {
  float d = std::fmod(to - from + kPi, kTau);
  if (d < 0.0f) d += kTau;
  return d - kPi;
}

// ---------------------------------------------------------------------------
// Colors are packed 0xAARRGGBB, matching SDL_PIXELFORMAT_ARGB8888.
// ---------------------------------------------------------------------------
using RGBA = uint32_t;

inline RGBA rgba(int r, int g, int b, int a = 255) {
  return (RGBA(a & 0xFF) << 24) | (RGBA(r & 0xFF) << 16) | (RGBA(g & 0xFF) << 8) | RGBA(b & 0xFF);
}
inline int redOf(RGBA c) { return int((c >> 16) & 0xFF); }
inline int greenOf(RGBA c) { return int((c >> 8) & 0xFF); }
inline int blueOf(RGBA c) { return int(c & 0xFF); }
inline int alphaOf(RGBA c) { return int((c >> 24) & 0xFF); }

// Multiply RGB by a 0..1+ factor (cheap distance shading). Alpha untouched.
inline RGBA shade(RGBA c, float f) {
  if (f >= 0.999f) return c;
  if (f <= 0.0f) return c & 0xFF000000u;
  const int r = int(float(redOf(c)) * f);
  const int g = int(float(greenOf(c)) * f);
  const int b = int(float(blueOf(c)) * f);
  return (c & 0xFF000000u) | (RGBA(clampi(r, 0, 255)) << 16) | (RGBA(clampi(g, 0, 255)) << 8) |
         RGBA(clampi(b, 0, 255));
}

// Fade "c" toward "fog" by t (0 = c, 1 = fog). Keeps alpha of c.
inline RGBA fogMix(RGBA c, RGBA fog, float t) {
  t = clampf(t, 0.0f, 1.0f);
  const int r = int(lerpf(float(redOf(c)), float(redOf(fog)), t));
  const int g = int(lerpf(float(greenOf(c)), float(greenOf(fog)), t));
  const int b = int(lerpf(float(blueOf(c)), float(blueOf(fog)), t));
  return (c & 0xFF000000u) | (RGBA(clampi(r, 0, 255)) << 16) | (RGBA(clampi(g, 0, 255)) << 8) |
         RGBA(clampi(b, 0, 255));
}

inline RGBA mixColor(RGBA a, RGBA b, float t) {
  t = clampf(t, 0.0f, 1.0f);
  const int r = int(lerpf(float(redOf(a)), float(redOf(b)), t));
  const int g = int(lerpf(float(greenOf(a)), float(greenOf(b)), t));
  const int bl = int(lerpf(float(blueOf(a)), float(blueOf(b)), t));
  const int al = int(lerpf(float(alphaOf(a)), float(alphaOf(b)), t));
  return rgba(r, g, bl, al);
}

// ---------------------------------------------------------------------------
// Tiny deterministic PRNG so procedural art is identical on every machine.
// ---------------------------------------------------------------------------
struct Rng {
  uint32_t s;

  explicit Rng(uint32_t seed = 0x9E3779B9u) : s(seed ? seed : 0x1234567u) {}

  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }
  // Uniform in [0,1)
  float unit() { return float(next() >> 8) * (1.0f / 16777216.0f); }
  // Uniform in [a,b)
  float range(float a, float b) { return a + (b - a) * unit(); }
  int irange(int a, int b) { return a + int(next() % uint32_t(b - a + 1)); }
  bool chance(float p) { return unit() < p; }
};

// ---------------------------------------------------------------------------
// Small integer rectangle, used by the HUD.
// ---------------------------------------------------------------------------
struct RectI {
  int x = 0, y = 0, w = 0, h = 0;

  RectI() = default;
  RectI(int x_, int y_, int w_, int h_) : x(x_), y(y_), w(w_), h(h_) {}

  bool contains(int px, int py) const {
    return px >= x && py >= y && px < x + w && py < y + h;
  }
  int right() const { return x + w; }
  int bottom() const { return y + h; }
};

inline std::string fmtInt(int v) { return std::to_string(v); }

std::string fmt(const char* format, ...);

}  // namespace fps
