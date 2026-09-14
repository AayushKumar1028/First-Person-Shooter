// image.cpp - image decoding. PNG uses zlib when available; BMP and PPM are
// decoded here directly so the game still loads custom textures without zlib.
#include "image.h"

#include <cstring>
#include <fstream>

#ifdef FPS_HAVE_ZLIB
#include <zlib.h>
#endif

namespace fps {

void Texture::finalize() {
  pow2 = w > 0 && h > 0 && (w & (w - 1)) == 0 && (h & (h - 1)) == 0;
  maskX = w - 1;
  maskY = h - 1;
}

namespace {

bool fail(std::string* err, const std::string& msg) {
  if (err) *err = msg;
  return false;
}

uint32_t readBE32(const unsigned char* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}
uint32_t readLE32(const unsigned char* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
uint16_t readLE16(const unsigned char* p) { return uint16_t(uint16_t(p[0]) | (uint16_t(p[1]) << 8)); }

#ifdef FPS_HAVE_ZLIB
bool inflateAll(const std::vector<unsigned char>& in, std::vector<unsigned char>& out,
                std::string* err) {
  if (in.empty()) return fail(err, "empty compressed stream");
  z_stream zs{};
  if (inflateInit(&zs) != Z_OK) return fail(err, "inflateInit failed");
  zs.next_in = const_cast<Bytef*>(in.data());
  zs.avail_in = uInt(in.size());

  out.clear();
  std::vector<unsigned char> chunk(65536);
  int status = Z_OK;
  do {
    zs.next_out = chunk.data();
    zs.avail_out = uInt(chunk.size());
    status = inflate(&zs, Z_NO_FLUSH);
    if (status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR) {
      inflateEnd(&zs);
      return fail(err, "corrupt zlib stream");
    }
    const size_t produced = chunk.size() - zs.avail_out;
    out.insert(out.end(), chunk.begin(), chunk.begin() + produced);
    if (status == Z_BUF_ERROR && produced == 0) break;
  } while (status != Z_STREAM_END);
  inflateEnd(&zs);
  return true;
}
#endif

int paethPredictor(int a, int b, int c) {
  const int p = a + b - c;
  const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
  if (pa <= pb && pa <= pc) return a;
  if (pb <= pc) return b;
  return c;
}

}  // namespace

// ---------------------------------------------------------------------------
// PNG
// ---------------------------------------------------------------------------
bool decodePng(const unsigned char* d, size_t n, Texture& out, std::string* err) {
#ifndef FPS_HAVE_ZLIB
  (void)d;
  (void)n;
  (void)out;
  return fail(err, "PNG support was not compiled in (zlib missing)");
#else
  static const unsigned char kSig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  if (n < 8 || std::memcmp(d, kSig, 8) != 0) return fail(err, "not a PNG file");

  int width = 0, height = 0, bitDepth = 0, colorType = 0, interlace = 0;
  bool haveHeader = false;
  std::vector<unsigned char> idat, palette, trns;

  size_t pos = 8;
  while (pos + 8 <= n) {
    const uint32_t len = readBE32(d + pos);
    const unsigned char* type = d + pos + 4;
    pos += 8;
    if (pos + len + 4 > n) break;
    const unsigned char* data = d + pos;

    if (std::memcmp(type, "IHDR", 4) == 0 && len >= 13) {
      width = int(readBE32(data));
      height = int(readBE32(data + 4));
      bitDepth = data[8];
      colorType = data[9];
      interlace = data[12];
      haveHeader = true;
    } else if (std::memcmp(type, "PLTE", 4) == 0) {
      palette.assign(data, data + len);
    } else if (std::memcmp(type, "tRNS", 4) == 0) {
      trns.assign(data, data + len);
    } else if (std::memcmp(type, "IDAT", 4) == 0) {
      idat.insert(idat.end(), data, data + len);
    } else if (std::memcmp(type, "IEND", 4) == 0) {
      break;
    }
    pos += len + 4;  // skip data + CRC
  }

  if (!haveHeader) return fail(err, "PNG has no IHDR");
  if (width <= 0 || height <= 0) return fail(err, "PNG has invalid dimensions");
  if (bitDepth != 8) return fail(err, "only 8-bit-per-channel PNGs are supported");
  if (interlace != 0) return fail(err, "interlaced PNGs are not supported");
  if (idat.empty()) return fail(err, "PNG has no image data");

  int channels = 0;
  switch (colorType) {
    case 0: channels = 1; break;  // grayscale
    case 2: channels = 3; break;  // truecolor
    case 3: channels = 1; break;  // palette
    case 4: channels = 2; break;  // gray + alpha
    case 6: channels = 4; break;  // RGBA
    default: return fail(err, "unsupported PNG color type");
  }

  std::vector<unsigned char> raw;
  if (!inflateAll(idat, raw, err)) return false;

  const size_t stride = size_t(width) * size_t(channels);
  if (raw.size() < (stride + 1) * size_t(height)) return fail(err, "PNG data is truncated");

  // Undo the per-scanline filters (PNG spec section 9).
  std::vector<unsigned char> img(stride * size_t(height));
  for (int y = 0; y < height; ++y) {
    const int filter = raw[size_t(y) * (stride + 1)];
    const unsigned char* src = &raw[size_t(y) * (stride + 1) + 1];
    unsigned char* dst = &img[size_t(y) * stride];
    const unsigned char* prev = y > 0 ? &img[size_t(y - 1) * stride] : nullptr;
    for (size_t x = 0; x < stride; ++x) {
      const int a = x >= size_t(channels) ? dst[x - channels] : 0;
      const int b = prev ? prev[x] : 0;
      const int c = (prev && x >= size_t(channels)) ? prev[x - channels] : 0;
      int value = src[x];
      switch (filter) {
        case 0: break;
        case 1: value += a; break;
        case 2: value += b; break;
        case 3: value += (a + b) / 2; break;
        case 4: value += paethPredictor(a, b, c); break;
        default: return fail(err, "unknown PNG filter type");
      }
      dst[x] = (unsigned char)(value & 0xFF);
    }
  }

  out.w = width;
  out.h = height;
  out.px.assign(size_t(width) * size_t(height), rgba(0, 0, 0, 0));

  for (int y = 0; y < height; ++y) {
    const unsigned char* src = &img[size_t(y) * stride];
    for (int x = 0; x < width; ++x) {
      RGBA c = rgba(0, 0, 0, 255);
      if (colorType == 0 || colorType == 4) {
        const int g = src[size_t(x) * size_t(channels)];
        const int a = channels == 2 ? src[size_t(x) * 2 + 1] : 255;
        c = rgba(g, g, g, a);
      } else if (colorType == 2 || colorType == 6) {
        const unsigned char* p = src + size_t(x) * size_t(channels);
        c = rgba(p[0], p[1], p[2], channels == 4 ? p[3] : 255);
      } else {  // palette
        const int idx = src[x];
        if (size_t(idx * 3 + 2) >= palette.size()) return fail(err, "PNG palette is truncated");
        const int a = size_t(idx) < trns.size() ? trns[size_t(idx)] : 255;
        c = rgba(palette[size_t(idx) * 3], palette[size_t(idx) * 3 + 1],
                 palette[size_t(idx) * 3 + 2], a);
      }
      // Truecolor/gray tRNS: a single fully transparent key color.
      if ((colorType == 0 || colorType == 2) && trns.size() >= size_t(channels * 2)) {
        bool match = true;
        for (int ch = 0; ch < channels; ++ch) {
          const unsigned char* p = src + size_t(x) * size_t(channels);
          if (p[ch] != trns[size_t(ch) * 2 + 1]) match = false;
        }
        if (match) c &= 0x00FFFFFFu;
      }
      out.px[size_t(y) * size_t(width) + size_t(x)] = c;
    }
  }
  out.finalize();
  return true;
#endif
}

// ---------------------------------------------------------------------------
// BMP (BI_RGB, 8-bit palette, 24-bit and 32-bit)
// ---------------------------------------------------------------------------
bool decodeBmp(const unsigned char* d, size_t n, Texture& out, std::string* err) {
  if (n < 54 || d[0] != 'B' || d[1] != 'M') return fail(err, "not a BMP file");
  const uint32_t dataOffset = readLE32(d + 10);
  const uint32_t dibSize = readLE32(d + 14);
  if (dibSize < 40) return fail(err, "unsupported BMP header");
  const int32_t rawW = int32_t(readLE32(d + 18));
  const int32_t rawH = int32_t(readLE32(d + 22));
  const uint16_t bitCount = readLE16(d + 28);
  const uint32_t compression = readLE32(d + 30);
  if (compression != 0) return fail(err, "compressed BMPs are not supported");

  const bool topDown = rawH < 0;
  const int width = int(rawW);
  const int height = int(topDown ? -rawH : rawH);
  if (width <= 0 || height <= 0) return fail(err, "BMP has invalid dimensions");
  if (bitCount != 8 && bitCount != 24 && bitCount != 32) {
    return fail(err, "BMP must be 8, 24 or 32 bits per pixel");
  }

  uint32_t paletteCount = 0;
  size_t paletteOffset = 14 + dibSize;
  if (bitCount == 8) {
    paletteCount = dibSize >= 40 && n >= 50 ? readLE32(d + 46) : 0;
    if (paletteCount == 0) paletteCount = 256;
    if (paletteOffset + size_t(paletteCount) * 4 > n) return fail(err, "BMP palette is truncated");
  }

  const size_t rowBytes = ((size_t(width) * bitCount + 31) / 32) * 4;
  if (dataOffset + rowBytes * size_t(height) > n) return fail(err, "BMP pixel data is truncated");

  out.w = width;
  out.h = height;
  out.px.assign(size_t(width) * size_t(height), rgba(0, 0, 0, 255));

  for (int y = 0; y < height; ++y) {
    const int srcY = topDown ? y : (height - 1 - y);
    const unsigned char* src = d + dataOffset + rowBytes * size_t(srcY);
    for (int x = 0; x < width; ++x) {
      RGBA c;
      if (bitCount == 8) {
        const unsigned int idx = src[x];
        if (idx >= paletteCount) continue;
        const unsigned char* pe = d + paletteOffset + size_t(idx) * 4;
        c = rgba(pe[2], pe[1], pe[0], 255);
      } else if (bitCount == 24) {
        const unsigned char* p = src + size_t(x) * 3;
        c = rgba(p[2], p[1], p[0], 255);
      } else {
        const unsigned char* p = src + size_t(x) * 4;
        c = rgba(p[2], p[1], p[0], 255);
      }
      out.px[size_t(y) * size_t(width) + size_t(x)] = c;
    }
  }
  out.finalize();
  return true;
}

// ---------------------------------------------------------------------------
// PPM (P6 binary / P3 ascii) - handy because it is trivial to generate.
// ---------------------------------------------------------------------------
namespace {
bool skipPpmSpaceAndComments(const unsigned char* d, size_t n, size_t& pos) {
  while (pos < n) {
    const unsigned char ch = d[pos];
    if (ch == '#') {
      while (pos < n && d[pos] != '\n') ++pos;
    } else if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' || ch == '\f') {
      ++pos;
    } else {
      return true;
    }
  }
  return false;
}

bool readPpmInt(const unsigned char* d, size_t n, size_t& pos, int& value) {
  if (!skipPpmSpaceAndComments(d, n, pos)) return false;
  int v = 0;
  bool any = false;
  while (pos < n && d[pos] >= '0' && d[pos] <= '9') {
    v = v * 10 + (d[pos] - '0');
    ++pos;
    any = true;
  }
  value = v;
  return any;
}
}  // namespace

bool decodePpm(const unsigned char* d, size_t n, Texture& out, std::string* err) {
  if (n < 2 || d[0] != 'P') return fail(err, "not a PPM file");
  const bool ascii = d[1] == '3';
  const bool binary = d[1] == '6';
  if (!ascii && !binary) return fail(err, "only P3/P6 PPM files are supported");

  size_t pos = 2;
  int width = 0, height = 0, maxVal = 255;
  if (!readPpmInt(d, n, pos, width) || !readPpmInt(d, n, pos, height) ||
      !readPpmInt(d, n, pos, maxVal)) {
    return fail(err, "malformed PPM header");
  }
  if (width <= 0 || height <= 0 || maxVal <= 0 || maxVal > 255) {
    return fail(err, "PPM has invalid dimensions or depth");
  }

  out.w = width;
  out.h = height;
  out.px.assign(size_t(width) * size_t(height), rgba(0, 0, 0, 255));
  const float scale = 255.0f / float(maxVal);

  if (ascii) {
    for (size_t i = 0; i < out.px.size(); ++i) {
      int r = 0, g = 0, b = 0;
      if (!readPpmInt(d, n, pos, r) || !readPpmInt(d, n, pos, g) || !readPpmInt(d, n, pos, b)) {
        return fail(err, "PPM ascii data is truncated");
      }
      out.px[i] = rgba(int(r * scale), int(g * scale), int(b * scale), 255);
    }
  } else {
    ++pos;  // exactly one whitespace byte separates the header from the data
    if (pos + out.px.size() * 3 > n) return fail(err, "PPM pixel data is truncated");
    const unsigned char* src = d + pos;
    for (size_t i = 0; i < out.px.size(); ++i) {
      out.px[i] = rgba(int(src[i * 3] * scale), int(src[i * 3 + 1] * scale),
                       int(src[i * 3 + 2] * scale), 255);
    }
  }
  out.finalize();
  return true;
}

// ---------------------------------------------------------------------------
// Dispatch + file loading
// ---------------------------------------------------------------------------
bool decodeImage(const unsigned char* data, size_t size, Texture& out, std::string* err) {
  if (size >= 8 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
    return decodePng(data, size, out, err);
  }
  if (size >= 2 && data[0] == 'B' && data[1] == 'M') return decodeBmp(data, size, out, err);
  if (size >= 2 && data[0] == 'P' && (data[1] == '3' || data[1] == '6')) {
    return decodePpm(data, size, out, err);
  }
  return fail(err, "unrecognised image format (expected PNG, BMP or PPM)");
}

bool loadTextureFile(const std::string& path, Texture& out, std::string* err) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return fail(err, "cannot open " + path);
  std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
  if (bytes.empty()) return fail(err, "empty file " + path);
  return decodeImage(bytes.data(), bytes.size(), out, err);
}

// ---------------------------------------------------------------------------
// Encoders
// ---------------------------------------------------------------------------
bool writePpm(const std::string& path, const Texture& tex) {
  if (tex.empty()) return false;
  std::ofstream out(path, std::ios::binary);
  if (!out) return false;
  out << "P6\n" << tex.w << " " << tex.h << "\n255\n";
  std::vector<unsigned char> row(size_t(tex.w) * 3);
  for (int y = 0; y < tex.h; ++y) {
    for (int x = 0; x < tex.w; ++x) {
      const RGBA c = tex.px[size_t(y) * size_t(tex.w) + size_t(x)];
      row[size_t(x) * 3 + 0] = (unsigned char)redOf(c);
      row[size_t(x) * 3 + 1] = (unsigned char)greenOf(c);
      row[size_t(x) * 3 + 2] = (unsigned char)blueOf(c);
    }
    out.write(reinterpret_cast<const char*>(row.data()), std::streamsize(row.size()));
  }
  return bool(out);
}

bool writePng(const std::string& path, const Texture& tex) {
#ifdef FPS_HAVE_ZLIB
  if (tex.empty()) return false;

  std::vector<unsigned char> raw;
  raw.reserve(size_t(tex.h) * (size_t(tex.w) * 4 + 1));
  for (int y = 0; y < tex.h; ++y) {
    raw.push_back(0);  // filter type 0 (none)
    for (int x = 0; x < tex.w; ++x) {
      const RGBA c = tex.px[size_t(y) * size_t(tex.w) + size_t(x)];
      raw.push_back((unsigned char)redOf(c));
      raw.push_back((unsigned char)greenOf(c));
      raw.push_back((unsigned char)blueOf(c));
      raw.push_back((unsigned char)alphaOf(c));
    }
  }

  uLongf bound = compressBound(uLong(raw.size()));
  std::vector<unsigned char> compressed(bound);
  if (compress2(compressed.data(), &bound, raw.data(), uLong(raw.size()), 6) != Z_OK) return false;
  compressed.resize(bound);

  std::ofstream out(path, std::ios::binary);
  if (!out) return false;

  auto be32 = [](unsigned char* p, uint32_t v) {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
  };
  auto chunk = [&](const char* type, const unsigned char* data, uint32_t len) {
    unsigned char head[8];
    be32(head, len);
    std::memcpy(head + 4, type, 4);
    out.write(reinterpret_cast<const char*>(head), 8);
    if (len > 0) out.write(reinterpret_cast<const char*>(data), len);
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, reinterpret_cast<const Bytef*>(type), 4);
    if (len > 0) crc = crc32(crc, data, len);
    unsigned char tail[4];
    be32(tail, uint32_t(crc));
    out.write(reinterpret_cast<const char*>(tail), 4);
  };

  static const unsigned char kSig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  out.write(reinterpret_cast<const char*>(kSig), 8);

  unsigned char ihdr[13];
  be32(ihdr, uint32_t(tex.w));
  be32(ihdr + 4, uint32_t(tex.h));
  ihdr[8] = 8;   // bit depth
  ihdr[9] = 6;   // color type: RGBA
  ihdr[10] = 0;  // deflate
  ihdr[11] = 0;  // adaptive filtering
  ihdr[12] = 0;  // no interlace
  chunk("IHDR", ihdr, 13);
  chunk("IDAT", compressed.data(), uint32_t(compressed.size()));
  chunk("IEND", nullptr, 0);
  return bool(out);
#else
  (void)path;
  (void)tex;
  return false;
#endif
}

bool saveTexture(const std::string& path, const Texture& tex) {
  const bool wantsPng = path.size() > 4 && path.compare(path.size() - 4, 4, ".png") == 0;
  if (wantsPng && writePng(path, tex)) return true;
  // Without zlib we still want usable files, so fall back to a PPM sibling.
  const std::string ppmPath = wantsPng ? path.substr(0, path.size() - 4) + ".ppm" : path;
  return writePpm(ppmPath, tex);
}

}  // namespace fps
