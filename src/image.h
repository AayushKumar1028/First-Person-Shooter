// image.h - RGBA texture storage plus image decoding (PNG / BMP / PPM).
//
// Textures are stored uncompressed in ARGB8888 so the software renderer can
// sample them with a single array index. Any size works; power-of-two sizes get
// a fast masking path for tiling.
#pragma once

#include "core.h"

namespace fps {

struct Texture {
  int w = 0;
  int h = 0;
  std::vector<RGBA> px;
  bool pow2 = false;
  int maskX = 0;
  int maskY = 0;

  bool empty() const { return w <= 0 || h <= 0; }

  // Recomputes the power-of-two fast path. Call after filling px.
  void finalize();

  // Wrapped (tiling) lookup - the common case for walls and floors.
  inline RGBA texel(int x, int y) const {
    if (pow2) {
      return px[size_t(y & maskY) * size_t(w) + size_t(x & maskX)];
    }
    x %= w;
    if (x < 0) x += w;
    y %= h;
    if (y < 0) y += h;
    return px[size_t(y) * size_t(w) + size_t(x)];
  }
  // Clamped lookup - used by sprites so edge columns do not wrap.
  inline RGBA texelClamped(int x, int y) const {
    return px[size_t(clampi(y, 0, h - 1)) * size_t(w) + size_t(clampi(x, 0, w - 1))];
  }
  inline RGBA texelAtUV(float u, float v) const {
    return texel(int(u * float(w)), int(v * float(h)));
  }
};

// Decoders. Each returns false and fills "err" on failure.
bool decodePng(const unsigned char* data, size_t size, Texture& out, std::string* err = nullptr);
bool decodeBmp(const unsigned char* data, size_t size, Texture& out, std::string* err = nullptr);
bool decodePpm(const unsigned char* data, size_t size, Texture& out, std::string* err = nullptr);

// Dispatches on magic bytes, so a custom texture can be .png, .bmp or .ppm.
bool decodeImage(const unsigned char* data, size_t size, Texture& out, std::string* err = nullptr);

// Reads a file from disk and decodes it. Returns false if missing/unreadable.
bool loadTextureFile(const std::string& path, Texture& out, std::string* err = nullptr);

// Encoders - used by `--dump-textures` to write editable template art.
bool writePpm(const std::string& path, const Texture& tex);
bool writePng(const std::string& path, const Texture& tex);
// Chooses the encoder from the file extension (.png falls back to PPM without zlib).
bool saveTexture(const std::string& path, const Texture& tex);

// Utility used by the procedural art generator.
inline void setTexel(Texture& t, int x, int y, RGBA c) {
  if (x < 0 || y < 0 || x >= t.w || y >= t.h) return;
  t.px[size_t(y) * size_t(t.w) + size_t(x)] = c;
}
inline RGBA getTexel(const Texture& t, int x, int y) { return t.texelClamped(x, y); }

}  // namespace fps
