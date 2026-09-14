// image.h - palettised texture storage plus image decoding (PNG / BMP / PPM).
//
// Textures are stored the way the originals did it: one 8-bit palette index per
// pixel, plus a small colour table. That is 4x less memory than ARGB8888 for
// opaque art (and 2x for sprites, which also need an alpha plane), traded for one
// extra array lookup per sampled pixel.
//
// Authoring (procedural generators and image decoders) writes RGBA into "px".
// Texture::finalize() then quantises that into "idx"/"palette"/"alpha" and
// releases "px", so the RGBA buffer only ever exists while a texture is being
// built.
#pragma once

#include "core.h"

namespace fps {

struct Texture {
  int w = 0;
  int h = 0;
  bool pow2 = false;
  int maskX = 0;
  int maskY = 0;

  // --- runtime storage (1 byte per pixel, 2 bytes for sprites) -------------
  std::vector<uint8_t> idx;      // palette indices, w*h
  std::vector<uint8_t> alpha;    // coverage, w*h; empty when fully opaque
  std::vector<RGBA> palette;     // up to 256 colours

  // --- authoring storage (released by finalize) ----------------------------
  std::vector<RGBA> px;

  bool empty() const { return w <= 0 || h <= 0; }
  bool ready() const { return !idx.empty(); }
  bool hasAlpha() const { return !alpha.empty(); }

  // Allocates the RGBA authoring buffer and clears it to transparent.
  void beginAuthoring(int w, int h);

  // Quantises px into idx/palette/alpha and frees px. Safe to call twice.
  void finalize();

  inline const RGBA* paletteData() const { return palette.data(); }
  inline int paletteSize() const { return int(palette.size()); }

  // Offset of a wrapped (tiling) texel - the hot path for walls and floors.
  inline size_t wrappedOffset(int x, int y) const {
    if (pow2) return size_t(y & maskY) * size_t(w) + size_t(x & maskX);
    x %= w;
    if (x < 0) x += w;
    y %= h;
    if (y < 0) y += h;
    return size_t(y) * size_t(w) + size_t(x);
  }
  inline uint8_t indexAt(int x, int y) const { return idx[wrappedOffset(x, y)]; }

  // Clamped lookup - used by sprites so edge columns do not wrap.
  inline uint8_t indexClamped(int x, int y) const {
    return idx[size_t(clampi(y, 0, h - 1)) * size_t(w) + size_t(clampi(x, 0, w - 1))];
  }
  inline uint8_t alphaClamped(int x, int y) const {
    const size_t o = size_t(clampi(y, 0, h - 1)) * size_t(w) + size_t(clampi(x, 0, w - 1));
    return alpha.empty() ? uint8_t(255) : alpha[o];
  }

  // Convenience lookups that also work while a texture is still being authored.
  inline RGBA colorAt(int x, int y) const {
    const size_t off = wrappedOffset(x, y);
    if (!px.empty()) return px[off];
    if (idx.empty()) return 0;
    const RGBA c = palette[idx[off]];
    return alpha.empty() ? c : ((c & 0x00FFFFFFu) | (RGBA(alpha[off]) << 24));
  }
  inline RGBA colorClamped(int x, int y) const {
    const int cx = clampi(x, 0, w - 1), cy = clampi(y, 0, h - 1);
    if (!px.empty()) return px[size_t(cy) * size_t(w) + size_t(cx)];
    const size_t o = size_t(cy) * size_t(w) + size_t(cx);
    const RGBA c = palette[idx[o]];
    return alpha.empty() ? c : ((c & 0x00FFFFFFu) | (RGBA(alpha[o]) << 24));
  }
  inline RGBA texel(int x, int y) const { return colorAt(x, y); }
  inline RGBA texelClamped(int x, int y) const { return colorClamped(x, y); }
  inline RGBA texelAtUV(float u, float v) const {
    return colorAt(int(u * float(w)), int(v * float(h)));
  }

  // Bytes currently held for this texture.
  size_t memoryBytes() const {
    return idx.size() + alpha.size() + palette.size() * sizeof(RGBA) + px.size() * sizeof(RGBA);
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

// Encoders - used by `--dump-textures` and `--shot`. A raw RGBA buffer can be
// written directly so screenshots are not palettised.
bool writePpm(const std::string& path, const Texture& tex);
bool writePng(const std::string& path, const Texture& tex);
bool writePngRGBABuffer(const std::string& path, const RGBA* pixels, int w, int h);
bool writePpmRGBABuffer(const std::string& path, const RGBA* pixels, int w, int h);
// Chooses the encoder from the file extension (.png falls back to PPM without zlib).
bool saveTexture(const std::string& path, const Texture& tex);

// Utility used by the procedural art generator (authoring only).
inline void setTexel(Texture& t, int x, int y, RGBA c) {
  if (x < 0 || y < 0 || x >= t.w || y >= t.h) return;
  t.px[size_t(y) * size_t(t.w) + size_t(x)] = c;
}
inline RGBA getTexel(const Texture& t, int x, int y) { return t.texelClamped(x, y); }

}  // namespace fps
