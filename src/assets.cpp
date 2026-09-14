// assets.cpp - procedural retro art generation, disk overrides, template dump.
#include "assets.h"

#include <cstdio>
#include <fstream>

#include "font.h"

namespace fps {

namespace {

constexpr int kTexSize = 64;
constexpr int kSpriteSize = 64;

// ---------------------------------------------------------------------------
// Drawing helpers on Texture
// ---------------------------------------------------------------------------
void newTex(Texture& t, int w, int h) {
  t.w = w;
  t.h = h;
  t.px.assign(size_t(w) * size_t(h), rgba(0, 0, 0, 0));
  t.finalize();
}

void fill(Texture& t, RGBA c) { std::fill(t.px.begin(), t.px.end(), c); }

void rect(Texture& t, int x, int y, int w, int h, RGBA c) {
  for (int yy = 0; yy < h; ++yy)
    for (int xx = 0; xx < w; ++xx) setTexel(t, x + xx, y + yy, c);
}

// Same as rect() but wraps around the edges, so wall tiles seam cleanly.
void rectWrap(Texture& t, int x, int y, int w, int h, RGBA c) {
  for (int yy = 0; yy < h; ++yy) {
    int ty = (y + yy) % t.h;
    if (ty < 0) ty += t.h;
    for (int xx = 0; xx < w; ++xx) {
      int tx = (x + xx) % t.w;
      if (tx < 0) tx += t.w;
      t.px[size_t(ty) * size_t(t.w) + size_t(tx)] = c;
    }
  }
}

void hlineT(Texture& t, int x, int y, int len, RGBA c) {
  for (int i = 0; i < len; ++i) setTexel(t, x + i, y, c);
}
void vlineT(Texture& t, int x, int y, int len, RGBA c) {
  for (int i = 0; i < len; ++i) setTexel(t, x, y + i, c);
}

void ellipseFill(Texture& t, int cx, int cy, int rx, int ry, RGBA c) {
  if (rx <= 0 || ry <= 0) return;
  for (int y = -ry; y <= ry; ++y) {
    for (int x = -rx; x <= rx; ++x) {
      const float nx = float(x) / float(rx), ny = float(y) / float(ry);
      if (nx * nx + ny * ny <= 1.0f) setTexel(t, cx + x, cy + y, c);
    }
  }
}

// Multiply every pixel by a random 0..1 factor - adds the grain that makes
// flat placeholder colors read as surfaces.
void addNoise(Texture& t, Rng& rng, float amount) {
  for (RGBA& c : t.px) {
    if (alphaOf(c) == 0) continue;
    c = shade(c, 1.0f - amount * 0.5f + rng.unit() * amount);
  }
}

// Gives sprites a crisp 1px dark silhouette so they stand out against walls.
void addOutline(Texture& t, RGBA outline) {
  const std::vector<RGBA> src = t.px;
  auto opaque = [&](int x, int y) {
    if (x < 0 || y < 0 || x >= t.w || y >= t.h) return false;
    return alphaOf(src[size_t(y) * size_t(t.w) + size_t(x)]) >= 128;
  };
  for (int y = 0; y < t.h; ++y) {
    for (int x = 0; x < t.w; ++x) {
      if (opaque(x, y)) continue;
      if (opaque(x - 1, y) || opaque(x + 1, y) || opaque(x, y - 1) || opaque(x, y + 1)) {
        t.px[size_t(y) * size_t(t.w) + size_t(x)] = outline;
      }
    }
  }
}

RGBA vary(Rng& rng, RGBA base, float amount) {
  return shade(base, 1.0f - amount + rng.unit() * amount * 2.0f);
}

// ---------------------------------------------------------------------------
// Wall / floor / ceiling generators
// ---------------------------------------------------------------------------
void genBrick(Texture& t, Rng& rng, RGBA brick, RGBA mortar, int bw, int bh, float variation,
              float grime) {
  newTex(t, kTexSize, kTexSize);
  fill(t, mortar);
  addNoise(t, rng, 0.12f);

  for (int y = 0; y < t.h; y += bh) {
    const int row = y / bh;
    const int offset = (row % 2) ? bw / 2 : 0;
    for (int x = -bw; x < t.w + bw; x += bw) {
      const int bx = x + offset;
      const RGBA face = vary(rng, brick, variation);
      rectWrap(t, bx + 1, y + 1, bw - 2, bh - 2, face);
      rectWrap(t, bx + 1, y + 1, bw - 2, 1, shade(face, 1.28f));      // top highlight
      rectWrap(t, bx + 1, y + bh - 2, bw - 2, 1, shade(face, 0.6f));  // bottom shadow
      rectWrap(t, bx + bw - 2, y + 1, 1, bh - 2, shade(face, 0.72f));
    }
  }
  // Vertical grime streaks running down the wall.
  if (grime > 0.0f) {
    for (int i = 0; i < int(grime * 26.0f); ++i) {
      const int x = rng.irange(0, t.w - 1);
      const int y0 = rng.irange(0, t.h / 2);
      const int len = rng.irange(6, 30);
      const float a = rng.range(0.05f, 0.18f);
      for (int y = y0; y < y0 + len; ++y) {
        const RGBA c = t.texel(x, y);
        setTexel(t, x, y, mixColor(c, rgba(20, 16, 12), a));
      }
    }
  }
  addNoise(t, rng, 0.14f);
}

void genPanelGrid(Texture& t, Rng& rng, RGBA base, int cols, int rows, bool rivets) {
  newTex(t, kTexSize, kTexSize);
  fill(t, shade(base, 0.75f));
  const int cw = t.w / cols, ch = t.h / rows;
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      const int x = c * cw, y = r * ch;
      // Brushed-metal vertical streaks inside each panel.
      for (int yy = 1; yy < ch - 1; ++yy) {
        for (int xx = 1; xx < cw - 1; ++xx) {
          const float band = 0.9f + 0.2f * std::sin(float(yy) * 0.9f + float(c) * 1.7f);
          setTexel(t, x + xx, y + yy, shade(vary(rng, base, 0.05f), band));
        }
      }
      hlineT(t, x + 1, y + 1, cw - 2, shade(base, 1.35f));
      vlineT(t, x + 1, y + 1, ch - 2, shade(base, 1.2f));
      hlineT(t, x + 1, y + ch - 2, cw - 2, shade(base, 0.45f));
      vlineT(t, x + cw - 2, y + 1, ch - 2, shade(base, 0.55f));
      if (rivets) {
        const RGBA rv = shade(base, 1.5f);
        ellipseFill(t, x + 2, y + 2, 1, 1, rv);
        ellipseFill(t, x + cw - 3, y + 2, 1, 1, rv);
        ellipseFill(t, x + 2, y + ch - 3, 1, 1, rv);
        ellipseFill(t, x + cw - 3, y + ch - 3, 1, 1, rv);
      }
    }
  }
  addNoise(t, rng, 0.07f);
}

void genPlanks(Texture& t, Rng& rng, RGBA wood, int planks, bool horizontal) {
  newTex(t, kTexSize, kTexSize);
  fill(t, shade(wood, 0.6f));
  const int size = horizontal ? t.h / planks : t.w / planks;
  for (int p = 0; p < planks; ++p) {
    const RGBA face = vary(rng, wood, 0.16f);
    if (horizontal) {
      rect(t, 0, p * size + 1, t.w, size - 2, face);
      hlineT(t, 0, p * size + 1, t.w, shade(face, 1.22f));
      hlineT(t, 0, p * size + size - 2, t.w, shade(face, 0.55f));
    } else {
      rect(t, p * size + 1, 0, size - 2, t.h, face);
      vlineT(t, p * size + 1, 0, t.h, shade(face, 1.22f));
      vlineT(t, p * size + size - 2, 0, t.h, shade(face, 0.55f));
    }
    // Knots and grain.
    for (int i = 0; i < 12; ++i) {
      const int gx = horizontal ? rng.irange(0, t.w - 1) : p * size + rng.irange(2, size - 3);
      const int gy = horizontal ? p * size + rng.irange(2, size - 3) : rng.irange(0, t.h - 1);
      if (horizontal) hlineT(t, gx, gy, rng.irange(4, 16), shade(face, 0.82f));
      else vlineT(t, gx, gy, rng.irange(4, 16), shade(face, 0.82f));
    }
    const int kx = horizontal ? rng.irange(4, t.w - 5) : p * size + size / 2;
    const int ky = horizontal ? p * size + size / 2 : rng.irange(4, t.h - 5);
    ellipseFill(t, kx, ky, 2, 2, shade(face, 0.6f));
  }
  addNoise(t, rng, 0.1f);
}

void genCobbleFloor(Texture& t, Rng& rng, RGBA stone, RGBA joint) {
  newTex(t, kTexSize, kTexSize);
  fill(t, joint);
  const int cell = 16;
  for (int y = 0; y < t.h; y += cell) {
    for (int x = 0; x < t.w; x += cell) {
      // Jittered, roughly round stones packed into a grid.
      const int cx = x + cell / 2 + rng.irange(-2, 2);
      const int cy = y + cell / 2 + rng.irange(-2, 2);
      const int rx = cell / 2 - rng.irange(0, 2), ry = cell / 2 - rng.irange(0, 2);
      const RGBA face = vary(rng, stone, 0.22f);
      ellipseFill(t, cx, cy, rx, ry, face);
      ellipseFill(t, cx, cy - 1, rx - 2, ry - 2, shade(face, 1.12f));
      ellipseFill(t, cx, cy + 2, rx, 1, shade(face, 0.6f));
    }
  }
  addNoise(t, rng, 0.18f);
}

void genGrateFloor(Texture& t, Rng& rng, RGBA metal) {
  newTex(t, kTexSize, kTexSize);
  fill(t, shade(metal, 0.3f));
  for (int y = 0; y < t.h; y += 8) {
    rect(t, 0, y, t.w, 5, vary(rng, metal, 0.1f));
    hlineT(t, 0, y, t.w, shade(metal, 1.3f));
    hlineT(t, 0, y + 4, t.w, shade(metal, 0.5f));
  }
  for (int x = 0; x < t.w; x += 16) {
    rect(t, x, 0, 3, t.h, shade(metal, 0.85f));
  }
  addNoise(t, rng, 0.1f);
}

void genConcrete(Texture& t, Rng& rng, RGBA base, int cracks) {
  newTex(t, kTexSize, kTexSize);
  fill(t, base);
  // Patchy stains.
  for (int i = 0; i < 26; ++i) {
    const RGBA c = vary(rng, base, 0.14f);
    ellipseFill(t, rng.irange(0, t.w), rng.irange(0, t.h), rng.irange(2, 8), rng.irange(2, 7), c);
  }
  // Random-walk cracks.
  for (int i = 0; i < cracks; ++i) {
    int x = rng.irange(0, t.w - 1), y = rng.irange(0, t.h - 1);
    const RGBA dark = shade(base, 0.42f);
    for (int step = 0; step < rng.irange(10, 30); ++step) {
      setTexel(t, x, y, dark);
      setTexel(t, x + 1, y, shade(dark, 1.3f));
      x += rng.irange(-1, 1);
      y += rng.irange(-1, 1);
      if (x < 0 || y < 0 || x >= t.w || y >= t.h) break;
    }
  }
  addNoise(t, rng, 0.15f);
}

void genTechPanel(Texture& t, Rng& rng, RGBA base, RGBA glow) {
  newTex(t, kTexSize, kTexSize);
  fill(t, shade(base, 0.8f));
  // Recessed plate.
  rect(t, 3, 3, t.w - 6, t.h - 6, shade(base, 0.62f));
  hlineT(t, 3, 3, t.w - 6, shade(base, 1.4f));
  vlineT(t, 3, 3, t.h - 6, shade(base, 1.25f));
  hlineT(t, 3, t.h - 4, t.w - 6, shade(base, 0.35f));
  vlineT(t, t.w - 4, 3, t.h - 6, shade(base, 0.45f));

  // Circuit traces: orthogonal random walks with glowing nodes.
  for (int i = 0; i < 8; ++i) {
    int x = rng.irange(6, t.w - 7), y = rng.irange(6, t.h - 7);
    for (int seg = 0; seg < 4; ++seg) {
      const bool horiz = rng.chance(0.5f);
      const int len = rng.irange(4, 14) * (rng.chance(0.5f) ? 1 : -1);
      for (int s = 0; s < std::abs(len); ++s) {
        const int px = horiz ? x + (len > 0 ? s : -s) : x;
        const int py = horiz ? y : y + (len > 0 ? s : -s);
        setTexel(t, px, py, shade(glow, 0.55f));
      }
      if (horiz) x += len; else y += len;
      x = clampi(x, 5, t.w - 6);
      y = clampi(y, 5, t.h - 6);
      rect(t, x - 1, y - 1, 3, 3, glow);
    }
  }
  // Dark vent slits along the bottom.
  for (int y = t.h - 14; y < t.h - 6; y += 3) hlineT(t, 6, y, t.w - 12, shade(base, 0.3f));
  addNoise(t, rng, 0.08f);
}

void genSupport(Texture& t, Rng& rng, RGBA metal) {
  genPanelGrid(t, rng, metal, 1, 1, false);
  // Vertical I-beam flanges.
  for (int x : {6, 26, 46}) {
    rect(t, x, 0, 12, t.h, vary(rng, metal, 0.08f));
    hlineT(t, x, 0, 12, shade(metal, 1.5f));
    for (int y = 4; y < t.h; y += 10) {
      ellipseFill(t, x + 2, y, 1, 1, shade(metal, 1.8f));
      ellipseFill(t, x + 9, y, 1, 1, shade(metal, 1.8f));
    }
  }
  addNoise(t, rng, 0.1f);
}

void genCrate(Texture& t, Rng& rng) {
  genPlanks(t, rng, rgba(126, 84, 44), 4, false);
  const RGBA brace = rgba(96, 92, 82);
  // Metal straps plus corner brackets.
  for (int y = 6; y < t.h; y += 24) rect(t, 0, y, t.w, 5, vary(rng, brace, 0.1f));
  rect(t, 0, 0, 3, t.h, shade(brace, 0.9f));
  rect(t, t.w - 3, 0, 3, t.h, shade(brace, 0.9f));
  hlineT(t, 0, 0, t.w, shade(brace, 1.4f));
  addNoise(t, rng, 0.1f);
}

void genFlag(Texture& t, Rng& rng) {
  newTex(t, kTexSize, kTexSize);
  fill(t, rgba(30, 26, 34));
  rect(t, 4, 2, t.w - 8, t.h - 8, rgba(120, 22, 22));
  // Folds in the cloth.
  for (int x = 4; x < t.w - 4; ++x) {
    const float f = 0.82f + 0.2f * std::sin(float(x) * 0.55f);
    vlineT(t, x, 2, t.h - 10, shade(rgba(120, 22, 22), f));
  }
  // A crude white emblem (skull-ish sigil).
  ellipseFill(t, t.w / 2, 24, 10, 11, rgba(226, 220, 205));
  rect(t, t.w / 2 - 5, 32, 10, 5, rgba(226, 220, 205));
  ellipseFill(t, t.w / 2 - 4, 22, 3, 3, rgba(60, 20, 20));
  ellipseFill(t, t.w / 2 + 4, 22, 3, 3, rgba(60, 20, 20));
  ellipseFill(t, t.w / 2 - 4, 34, 1, 2, rgba(60, 20, 20));
  ellipseFill(t, t.w / 2 + 4, 34, 1, 2, rgba(60, 20, 20));
  addNoise(t, rng, 0.12f);
}

// Shared door body: recessed panels, a handle bar and a bevel frame.
void genDoorBody(Texture& t, Rng& rng, RGBA metal, bool wood) {
  if (wood) {
    genPlanks(t, rng, metal, 6, true);
  } else {
    genPanelGrid(t, rng, metal, 2, 4, true);
  }
  const RGBA frame = shade(metal, 1.55f);
  rect(t, 0, 0, t.w, 2, frame);
  rect(t, 0, t.h - 2, t.w, 2, frame);
  rect(t, 0, 0, 2, t.h, frame);
  rect(t, t.w - 2, 0, 2, t.h, frame);
  rect(t, 6, 6, t.w - 12, 22, shade(metal, 0.8f));
  hlineT(t, 6, 6, t.w - 12, shade(metal, 1.3f));
  rect(t, 6, 34, t.w - 12, 22, shade(metal, 0.8f));
  hlineT(t, 6, 34, t.w - 12, shade(metal, 1.3f));
  // Handle.
  rect(t, t.w - 12, 30, 3, 6, rgba(200, 196, 180));
}

void genBloodSplatter(Texture& t, Rng& rng, RGBA base, float strength) {
  genConcrete(t, rng, base, 2);
  for (int i = 0; i < int(18 * strength); ++i) {
    const int cx = rng.irange(0, t.w - 1), cy = rng.irange(0, t.h - 1);
    const int r = rng.irange(2, 9);
    const RGBA blood = rgba(120 + rng.irange(-20, 30), 12, 14);
    for (int y = -r; y <= r; ++y) {
      for (int x = -r; x <= r; ++x) {
        const float d = std::sqrt(float(x * x + y * y)) / float(r);
        if (d > 1.0f) continue;
        const RGBA cur = t.texel(cx + x, cy + y);
        setTexel(t, cx + x, cy + y, mixColor(cur, blood, 0.75f * (1.0f - d) + 0.15f));
      }
    }
  }
  addNoise(t, rng, 0.1f);
}

// ---------------------------------------------------------------------------
// Sprite generators
// ---------------------------------------------------------------------------
enum class FoeKind { Guard, Imp, Brute };

struct FoePalette {
  RGBA skin, cloth, pants, helmet, accent, eye, weapon;
};

FoePalette paletteFor(FoeKind kind) {
  switch (kind) {
    case FoeKind::Imp:
      return {rgba(158, 82, 58), rgba(70, 40, 34), rgba(52, 30, 26), rgba(120, 40, 34),
              rgba(186, 108, 46), rgba(255, 208, 60), rgba(90, 60, 50)};
    case FoeKind::Brute:
      return {rgba(146, 146, 134), rgba(96, 106, 116), rgba(70, 76, 84), rgba(110, 118, 128),
              rgba(190, 60, 50), rgba(255, 64, 48), rgba(60, 64, 70)};
    case FoeKind::Guard:
    default:
      return {rgba(206, 168, 132), rgba(112, 92, 58), rgba(74, 66, 50), rgba(96, 102, 84),
              rgba(150, 44, 40), rgba(40, 30, 30), rgba(70, 72, 78)};
  }
}

// Squash/translate parameters driving every animation frame.
struct Pose {
  float bob = 0.0f;        // vertical body offset in pixels
  float legPhase = 0.0f;   // -1..1 stride
  float armRaise = 0.0f;   // 0..1 arms up toward the target
  float death = 0.0f;      // 0 alive .. 1 flat on the floor
  float lean = 0.0f;       // horizontal shear, used by the pain frame
  bool flash = false;      // muzzle flash while firing
};

void drawFoe(Texture& t, FoeKind kind, const Pose& pose) {
  newTex(t, kSpriteSize, kSpriteSize);
  const FoePalette p = paletteFor(kind);
  const int bottom = 63;
  const bool bulk = kind == FoeKind::Brute;
  const float squash = 1.0f - pose.death * 0.78f;

  // Map "design space" (feet at y=63, head near y=3) into the squashed pose.
  auto Y = [&](int designY) {
    const float offset = float(bottom - designY) * squash;
    return int(std::round(float(bottom) - offset + pose.bob));
  };
  // Limbs higher up shear further, which sells the pain/death lean.
  auto box = [&](int x, int y, int w, int h, RGBA c) {
    const int top = Y(y), bot = Y(y + h - 1);
    const int y0 = std::min(top, bot), y1 = std::max(top, bot);
    const int shear = int(std::round(pose.lean * float(bottom - (y + h / 2)) / 40.0f));
    rect(t, x + shear, y0, w, y1 - y0 + 1, c);
  };

  const int torsoW = bulk ? 28 : 24;
  const int torsoX = 32 - torsoW / 2;
  const int legW = bulk ? 10 : 8;

  // --- legs -------------------------------------------------------------
  const int stride = int(pose.legPhase * 6.0f);
  const int legY = bulk ? 44 : 42;
  const RGBA pants = pose.death > 0.5f ? shade(p.pants, 0.85f) : p.pants;
  box(22 - legW / 2 + stride, legY + std::abs(stride) / 2, legW, bottom - legY, pants);
  box(34 + legW / 2 - stride, legY + std::abs(stride) / 2, legW, bottom - legY, pants);
  // Boots.
  box(22 - legW / 2 + stride, 58, legW, 6, shade(p.helmet, 1.05f));
  box(34 + legW / 2 - stride, 58, legW, 6, shade(p.helmet, 1.05f));

  // --- torso ------------------------------------------------------------
  const int torsoY = bulk ? 18 : 20;
  box(torsoX, torsoY, torsoW, legY - torsoY + 2, p.cloth);
  // Shoulder plates.
  box(torsoX - 3, torsoY + 2, torsoW + 6, bulk ? 8 : 6, shade(p.cloth, 1.2f));
  if (bulk) {
    box(torsoX + 3, torsoY + 6, 6, 10, shade(p.accent, 1.1f));  // chest plate
    box(torsoX + torsoW - 9, torsoY + 6, 6, 10, shade(p.accent, 1.1f));
  }

  // --- arms -------------------------------------------------------------
  const int armW = bulk ? 8 : 7;
  const int armTop = torsoY + 4;
  if (pose.armRaise > 0.5f) {
    // Both arms extended forward, holding a weapon.
    const int shoulderY = armTop + 2;
    box(torsoX - 6, shoulderY, 8, 7, p.cloth);
    box(torsoX + torsoW - 2, shoulderY, 8, 7, p.cloth);
    box(torsoX + torsoW - 2, shoulderY + 1, 16, 5, p.weapon);
    box(torsoX + torsoW + 8, shoulderY + 2, 10, 3, shade(p.weapon, 1.35f));
  } else {
    const int swing = int(pose.legPhase * -4.0f);
    box(torsoX - 5, armTop + swing, armW, 20, p.cloth);
    box(torsoX + torsoW - 3, armTop - swing, armW, 20, p.cloth);
    box(torsoX - 5, armTop + 18 + swing, armW, 6, p.skin);
    box(torsoX + torsoW - 3, armTop + 18 - swing, armW, 6, p.skin);
  }

  // --- head -------------------------------------------------------------
  const int headY = torsoY - 12;
  if (pose.death < 0.85f) {
    box(26, headY - 4, 12, 4, p.skin);  // neck
    box(25, headY - 12, 14, 12, p.skin);
    if (kind == FoeKind::Brute) {
      // Armoured skull cap with a glowing visor.
      box(24, headY - 14, 16, 6, shade(p.helmet, 1.1f));
      box(26, headY - 8, 12, 3, p.eye);
    } else {
      box(23, headY - 15, 18, 7, p.helmet);   // helmet
      box(23, headY - 9, 18, 2, shade(p.helmet, 0.7f));  // brim
      box(27, headY - 8, 3, 2, p.eye);
      box(34, headY - 8, 3, 2, p.eye);
    }
    if (kind == FoeKind::Imp) {
      // Horns.
      for (int i = 0; i < 5; ++i) {
        const int w = 4 - i / 2;
        hlineT(t, 23 - i / 2, headY - 15 - i, w, shade(p.accent, 0.9f));
        hlineT(t, 41 + i / 2 - w + 1, headY - 15 - i, w, shade(p.accent, 0.9f));
      }
    }
  }

  // --- muzzle flash -----------------------------------------------------
  if (pose.flash && pose.armRaise > 0.5f) {
    const int fx = torsoX + torsoW + 16, fy = armTop + 3;
    ellipseFill(t, fx, fy, 5, 4, rgba(255, 240, 170));
    ellipseFill(t, fx, fy, 3, 2, rgba(255, 255, 240));
    hlineT(t, fx, fy, 8, rgba(255, 200, 90));
  }

  // --- death pooling ----------------------------------------------------
  if (pose.death > 0.6f) {
    ellipseFill(t, 32, 60, int(6 + 12 * pose.death), int(2 + 3 * pose.death), rgba(112, 14, 16));
    ellipseFill(t, 32, 60, int(3 + 7 * pose.death), int(1 + 2 * pose.death), rgba(160, 22, 24));
  }

  addOutline(t, rgba(12, 10, 12));
  Rng artRng(0x51ED27u ^ uint32_t(kind));
  addNoise(t, artRng, 0.09f);
}

void drawPickupBox(Texture& t, RGBA body, RGBA trim, int w, int h, const std::string& label,
                   RGBA labelColor) {
  newTex(t, kSpriteSize, kSpriteSize);
  const int x = (kSpriteSize - w) / 2;
  const int y = kSpriteSize - h - 1;
  rect(t, x, y, w, h, body);
  rect(t, x, y, w, 2, shade(body, 1.35f));
  rect(t, x, y + h - 2, w, 2, shade(body, 0.6f));
  rect(t, x, y, 2, h, shade(body, 1.2f));
  rect(t, x + w - 2, y, 2, h, shade(body, 0.7f));
  rect(t, x + 2, y + int(h * 0.55f), w - 4, h - int(h * 0.55f) - 2, trim);
  if (!label.empty() && h >= 18) {
    const int tw = fontTextWidth(label, 1, 1);
    fontDrawTexture(t, x + (w - tw) / 2, y + 5, label, labelColor, 1, 1);
  }
  addOutline(t, rgba(10, 8, 10));
}

void drawCrossBox(Texture& t, int w, int h, RGBA body) {
  drawPickupBox(t, body, shade(body, 0.85f), w, h, "", body);
  const int cx = kSpriteSize / 2, y = kSpriteSize - h - 1 + h / 2;
  rect(t, cx - 2, y - 8, 4, 16, rgba(226, 40, 40));
  rect(t, cx - 7, y - 3, 14, 4, rgba(226, 40, 40));
  rect(t, cx - 2, y - 8, 4, 3, rgba(255, 120, 110));
  addOutline(t, rgba(10, 8, 10));
}

void drawAmmoBox(Texture& t, Rng& rng, RGBA body, RGBA bullet, bool shells, bool rockets,
                 bool cells) {
  drawPickupBox(t, body, shade(body, 0.8f), 26, 15, "", body);
  const int x = (kSpriteSize - 26) / 2, y = kSpriteSize - 16;
  if (shells) {
    for (int i = 0; i < 5; ++i) {
      const int bx = x + 3 + i * 4;
      rect(t, bx, y - 8, 3, 9, bullet);
      rect(t, bx, y - 8, 3, 3, rgba(210, 60, 40));
    }
  } else if (rockets) {
    for (int i = 0; i < 3; ++i) {
      const int bx = x + 3 + i * 7;
      rect(t, bx, y - 12, 5, 13, bullet);
      rect(t, bx, y - 12, 5, 4, rgba(220, 80, 50));
      rect(t, bx + 1, y + 1, 3, 2, rgba(200, 200, 200));
    }
  } else if (cells) {
    for (int i = 0; i < 2; ++i) {
      const int bx = x + 4 + i * 10;
      rect(t, bx, y - 12, 8, 13, bullet);
      rect(t, bx + 1, y - 10, 6, 4, rgba(120, 220, 255));
      rect(t, bx + 1, y - 5, 6, 3, rgba(180, 240, 255));
    }
  } else {
    for (int i = 0; i < 6; ++i) {
      const int bx = x + 3 + i * 3;
      rect(t, bx, y - 7, 2, 8, bullet);
      rect(t, bx, y - 7, 2, 2, rgba(220, 200, 120));
    }
  }
  (void)rng;
  addOutline(t, rgba(10, 8, 10));
}

void drawWeaponPickup(Texture& t, Rng& rng, int kind) {
  // 0 shotgun, 1 chaingun, 2 rocket launcher, 3 plasma rifle
  newTex(t, kSpriteSize, kSpriteSize);
  const int baseY = 56;
  const RGBA metal = rgba(84, 88, 96);
  const RGBA dark = rgba(48, 50, 56);
  if (kind == 0) {
    rect(t, 8, baseY, 40, 4, metal);            // barrels
    rect(t, 8, baseY - 3, 40, 3, dark);
    rect(t, 26, baseY + 4, 20, 6, rgba(122, 82, 44));  // stock
    rect(t, 20, baseY + 4, 10, 5, dark);        // receiver
    rect(t, 24, baseY + 9, 5, 4, rgba(122, 82, 44));
  } else if (kind == 1) {
    rect(t, 10, baseY, 26, 7, dark);            // motor housing
    for (int i = 0; i < 4; ++i) rect(t, 14 + i * 7, baseY - 7, 4, 9, metal);
    rect(t, 34, baseY + 2, 16, 5, rgba(122, 82, 44));
    rect(t, 16, baseY + 7, 6, 6, rgba(60, 62, 68));
  } else if (kind == 2) {
    rect(t, 6, baseY - 2, 46, 9, rgba(96, 100, 88));  // tube
    rect(t, 6, baseY - 2, 46, 3, rgba(140, 146, 130));
    rect(t, 30, baseY + 7, 12, 6, dark);
    rect(t, 8, baseY - 6, 10, 4, rgba(190, 70, 50));  // rocket nose
  } else {
    rect(t, 8, baseY, 42, 6, rgba(52, 66, 96));
    rect(t, 8, baseY, 42, 2, rgba(120, 190, 255));
    ellipseFill(t, 22, baseY - 4, 5, 5, rgba(90, 180, 255));
    ellipseFill(t, 22, baseY - 4, 3, 3, rgba(210, 245, 255));
    rect(t, 32, baseY + 6, 14, 5, rgba(40, 48, 64));
  }
  (void)rng;
  addOutline(t, rgba(10, 8, 10));
}

void drawBarrel(Texture& t, Rng& rng, bool broken) {
  newTex(t, kSpriteSize, kSpriteSize);
  const RGBA body = rgba(126, 46, 40);
  const int x = 15, w = 34;
  if (broken) {
    // Crushed, smouldering drum.
    rect(t, x - 3, 52, w + 6, 12, shade(body, 0.7f));
    for (int i = 0; i < 14; ++i) {
      const int bx = x - 4 + rng.irange(0, w + 8);
      rect(t, bx, 48 + rng.irange(-4, 4), rng.irange(3, 9), rng.irange(2, 5), shade(body, 0.5f));
    }
    ellipseFill(t, 32, 52, 14, 4, rgba(30, 26, 26));
    ellipseFill(t, 32, 50, 10, 3, rgba(90, 40, 30));
  } else {
    rect(t, x, 22, w, 42, body);
    // Barrel rings.
    for (int y : {28, 40, 52}) rect(t, x, y, w, 3, shade(body, 1.4f));
    // Cylinder shading + top ellipse.
    for (int i = 0; i < w; ++i) {
      const float f = 0.7f + 0.6f * (1.0f - std::abs(float(i) / float(w) - 0.35f) * 1.8f);
      vlineT(t, x + i, 22, 42, shade(body, clampf(f, 0.55f, 1.35f)));
    }
    ellipseFill(t, 32, 22, w / 2, 4, shade(body, 1.25f));
    ellipseFill(t, 32, 22, w / 2 - 3, 2, rgba(70, 26, 24));
    // Hazard band.
    for (int i = 0; i < w; i += 6) {
      rect(t, x + i, 34, 3, 6, rgba(216, 196, 60));
      rect(t, x + i, 34, 3, 3, rgba(40, 36, 30));
    }
  }
  addOutline(t, rgba(14, 10, 10));
  addNoise(t, rng, 0.08f);
}

void drawLamp(Texture& t, Rng& rng) {
  newTex(t, kSpriteSize, kSpriteSize);
  rect(t, 29, 26, 6, 38, rgba(70, 72, 78));
  rect(t, 22, 58, 20, 6, rgba(60, 62, 68));
  ellipseFill(t, 32, 18, 12, 10, rgba(70, 72, 78));
  ellipseFill(t, 32, 20, 9, 8, rgba(255, 226, 130));
  ellipseFill(t, 32, 20, 5, 5, rgba(255, 250, 220));
  for (int i = 0; i < 6; ++i) {
    const int a = rng.irange(0, 6);
    rect(t, 20 + a * 4, 8 - a, 2, 10, shade(rgba(255, 226, 130), 0.8f));
  }
  addOutline(t, rgba(12, 10, 12));
}

void drawTechPillar(Texture& t, Rng& rng) {
  newTex(t, kSpriteSize, kSpriteSize);
  const RGBA body = rgba(66, 68, 78);
  rect(t, 20, 4, 24, 60, body);
  rect(t, 22, 4, 3, 60, shade(body, 1.5f));
  rect(t, 39, 4, 3, 60, shade(body, 0.6f));
  for (int y = 10; y < 58; y += 8) {
    rect(t, 25, y, 14, 4, rgba(40, 42, 50));
    rect(t, 26, y + 1, 12, 2, rng.chance(0.5f) ? rgba(110, 230, 150) : rgba(220, 80, 60));
  }
  rect(t, 16, 0, 32, 5, shade(body, 1.2f));
  rect(t, 16, 60, 32, 4, shade(body, 0.7f));
  addOutline(t, rgba(12, 10, 12));
}

// ---------------------------------------------------------------------------
// First-person view models. Canvas is 128x80 with the barrel aiming "up" into
// the screen, so the renderer can simply pin it to the bottom centre.
// ---------------------------------------------------------------------------
void drawFirstPerson(Texture& t, Rng& rng, int kind, bool firing) {
  newTex(t, 128, 80);
  const int kick = firing ? 3 : 0;  // vertical recoil
  const RGBA metal = rgba(74, 78, 88);
  const RGBA darkMetal = rgba(40, 42, 50);
  const RGBA wood = rgba(128, 84, 44);
  const RGBA skin = rgba(206, 168, 132);
  const RGBA glove = rgba(58, 62, 48);

  auto hand = [&](int x, int y, int w, int h, bool gloved) {
    rect(t, x, y, w, h, gloved ? glove : skin);
    rect(t, x, y + h - 3, w, 3, shade(gloved ? glove : skin, 0.65f));
    rect(t, x, y, w, 2, shade(gloved ? glove : skin, 1.25f));
  };
  auto flash = [&](int cx, int cy, int r) {
    ellipseFill(t, cx, cy - kick, r, int(float(r) * 0.75f), rgba(255, 236, 150));
    ellipseFill(t, cx, cy - kick, r / 2, r / 3, rgba(255, 255, 245));
    ellipseFill(t, cx, cy - kick, r + 3, int(float(r) * 0.5f), rgba(240, 180, 70));
  };

  switch (kind) {
    case 0: {  // pistol
      rect(t, 56, 30 - kick, 16, 22, darkMetal);
      rect(t, 56, 30 - kick, 16, 3, shade(darkMetal, 1.6f));
      rect(t, 58, 50 - kick, 12, 30, shade(darkMetal, 0.85f));
      rect(t, 57, 33 - kick, 4, 6, metal);
      hand(44, 58, 26, 22, false);
      hand(74, 60, 22, 20, false);
      rect(t, 60, 46 - kick, 8, 4, metal);
      if (firing) flash(64, 26, 10);
      break;
    }
    case 1: {  // shotgun, double barrels seen from behind
      rect(t, 42, 26 - kick, 44, 30, shade(metal, 0.9f));
      ellipseFill(t, 52, 30 - kick, 8, 7, rgba(18, 18, 22));
      ellipseFill(t, 76, 30 - kick, 8, 7, rgba(18, 18, 22));
      ellipseFill(t, 52, 30 - kick, 5, 4, rgba(4, 4, 6));
      ellipseFill(t, 76, 30 - kick, 5, 4, rgba(4, 4, 6));
      rect(t, 42, 46 - kick, 44, 6, shade(metal, 1.3f));
      rect(t, 50, 52 - kick, 28, 26, wood);
      rect(t, 50, 52 - kick, 28, 3, shade(wood, 1.3f));
      hand(30, 56, 28, 24, false);
      hand(72, 56, 28, 24, false);
      if (firing) {
        flash(52, 22, 11);
        flash(76, 22, 11);
      }
      break;
    }
    case 2: {  // chaingun: barrel cluster
      const int spin = firing ? 2 : 0;
      const int bx[4] = {48, 60, 72, 84};
      for (int i = 0; i < 4; ++i) {
        const int x = bx[(i + spin) % 4];
        rect(t, x, 22 - kick, 8, 34, shade(metal, 0.85f + 0.1f * float(i % 2)));
        ellipseFill(t, x + 4, 22 - kick, 4, 3, rgba(20, 20, 24));
      }
      rect(t, 40, 50 - kick, 48, 12, darkMetal);
      rect(t, 40, 50 - kick, 48, 3, shade(darkMetal, 1.5f));
      for (int i = 0; i < 6; ++i) rect(t, 46 + i * 8, 58 - kick, 4, 4, metal);
      rect(t, 56, 62 - kick, 20, 18, shade(darkMetal, 1.1f));
      hand(26, 54, 26, 26, true);
      hand(80, 54, 26, 26, true);
      if (firing) flash(68, 18, 13);
      break;
    }
    case 3: {  // rocket launcher
      rect(t, 34, 24 - kick, 60, 34, rgba(96, 100, 88));
      rect(t, 34, 24 - kick, 60, 4, rgba(140, 146, 130));
      ellipseFill(t, 64, 30 - kick, 14, 11, rgba(24, 22, 20));
      ellipseFill(t, 64, 30 - kick, 11, 8, rgba(10, 9, 8));
      rect(t, 40, 56 - kick, 48, 10, darkMetal);
      rect(t, 60, 64 - kick, 12, 16, shade(darkMetal, 1.2f));
      hand(28, 52, 26, 28, true);
      hand(84, 52, 22, 28, true);
      if (firing) flash(64, 20, 15);
      break;
    }
    default: {  // plasma rifle
      rect(t, 46, 26 - kick, 36, 28, rgba(52, 62, 92));
      rect(t, 46, 26 - kick, 36, 3, rgba(120, 160, 220));
      for (int i = 0; i < 3; ++i) {
        ellipseFill(t, 64, 34 - kick + i * 8, 10, 4, rgba(70, 130, 220));
        ellipseFill(t, 64, 34 - kick + i * 8, 6, 2, rgba(190, 235, 255));
      }
      rect(t, 48, 52 - kick, 32, 22, rgba(40, 46, 68));
      hand(30, 56, 22, 24, true);
      hand(80, 56, 22, 24, true);
      if (firing) {
        ellipseFill(t, 64, 24 - kick, 12, 10, rgba(140, 220, 255));
        ellipseFill(t, 64, 24 - kick, 6, 5, rgba(255, 255, 255));
      }
      break;
    }
  }
  (void)rng;
  addOutline(t, rgba(8, 8, 10));
}

void drawGore(Texture& t, Rng& rng) {
  newTex(t, kSpriteSize, kSpriteSize);
  ellipseFill(t, 32, 60, 18, 4, rgba(96, 12, 14));
  ellipseFill(t, 32, 60, 13, 3, rgba(140, 20, 22));
  for (int i = 0; i < 10; ++i) {
    ellipseFill(t, rng.irange(16, 48), rng.irange(52, 63), rng.irange(1, 4), rng.irange(1, 2),
                rgba(150, 24, 24));
  }
  addNoise(t, rng, 0.12f);
}

// Projectiles are drawn centred in their canvas; the renderer positions them by
// their own world-space z.
void drawFireball(Texture& t, Rng& rng, int frame) {
  newTex(t, kSpriteSize, kSpriteSize);
  const float wobble = 1.0f + 0.08f * float(frame);
  ellipseFill(t, 32, 32, int(13 * wobble), int(13 * wobble), rgba(190, 60, 20));
  ellipseFill(t, 32, 32, int(10 * wobble), int(10 * wobble), rgba(240, 130, 30));
  ellipseFill(t, 32, 32, int(6 * wobble), int(6 * wobble), rgba(255, 226, 120));
  ellipseFill(t, 30, 30, 3, 3, rgba(255, 255, 230));
  // Flame lashes.
  for (int i = 0; i < 7 + frame; ++i) {
    const float a = rng.range(0.0f, kTau);
    const int r = rng.irange(11, 16);
    ellipseFill(t, 32 + int(std::cos(a) * float(r)), 32 + int(std::sin(a) * float(r)),
                rng.irange(1, 3), rng.irange(1, 3), rgba(250, 160, 40));
  }
}

void drawRocketSprite(Texture& t, Rng& rng) {
  newTex(t, kSpriteSize, kSpriteSize);
  rect(t, 22, 22, 20, 22, rgba(120, 124, 132));
  for (int i = 0; i < 20; ++i) {
    const float f = 1.0f - std::abs(float(i) / 20.0f - 0.4f);
    vlineT(t, 22 + i, 22, 22, shade(rgba(120, 124, 132), 0.7f + 0.6f * f));
  }
  rect(t, 26, 14, 12, 9, rgba(190, 60, 46));
  rect(t, 30, 10, 4, 5, rgba(220, 200, 180));
  rect(t, 18, 40, 28, 4, rgba(90, 94, 100));
  ellipseFill(t, 32, 52, 8, 10, rgba(240, 150, 40));
  ellipseFill(t, 32, 50, 5, 7, rgba(255, 232, 150));
  (void)rng;
  addOutline(t, rgba(14, 10, 10));
}

void drawExplosion(Texture& t, Rng& rng, int frame, int frames) {
  newTex(t, kSpriteSize, kSpriteSize);
  const float p = float(frame) / float(std::max(1, frames - 1));
  const int r = int(6 + 26 * p);
  const int alpha = int(255 * (1.0f - p * 0.55f));
  auto blob = [&](int cx, int cy, int radius, RGBA c) {
    for (int y = -radius; y <= radius; ++y)
      for (int x = -radius; x <= radius; ++x) {
        const float d = std::sqrt(float(x * x + y * y)) / float(radius);
        if (d > 1.0f) continue;
        const RGBA cur = t.texel(cx + x, cy + y);
        const RGBA layered = mixColor(cur, c, (1.0f - d) * 0.9f + 0.1f);
        setTexel(t, cx + x, cy + y, (layered & 0x00FFFFFFu) | (RGBA(alpha) << 24));
      }
  };
  blob(32, 34, r, rgba(120, 30, 20));
  blob(32, 34, int(r * 0.72f), rgba(230, 110, 30));
  blob(32, 34, int(r * 0.42f), rgba(255, 220, 130));
  for (int i = 0; i < 12; ++i) {
    const float a = rng.range(0.0f, kTau);
    const int rr = rng.irange(r, r + 10);
    blob(32 + int(std::cos(a) * float(rr)), 34 + int(std::sin(a) * float(rr)), rng.irange(1, 3),
         rgba(200, 90, 30));
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// Slot names
// ---------------------------------------------------------------------------
const char* texName(Tex id) {
  switch (id) {
    case Tex::WallBrick: return "wall_brick";
    case Tex::WallStone: return "wall_stone";
    case Tex::WallMetal: return "wall_metal";
    case Tex::WallBlue: return "wall_blue";
    case Tex::WallWood: return "wall_wood";
    case Tex::WallGreen: return "wall_green";
    case Tex::WallConcrete: return "wall_concrete";
    case Tex::WallBlood: return "wall_blood";
    case Tex::WallCrate: return "wall_crate";
    case Tex::WallSupport: return "wall_support";
    case Tex::WallFlag: return "wall_flag";
    case Tex::WallExit: return "wall_exit_sign";
    case Tex::DoorMetal: return "door_metal";
    case Tex::DoorWood: return "door_wood";
    case Tex::DoorLocked: return "door_locked";
    case Tex::DoorExit: return "door_exit";
    case Tex::FloorStone: return "floor_stone";
    case Tex::FloorCobble: return "floor_cobble";
    case Tex::FloorMetal: return "floor_metal";
    case Tex::FloorTech: return "floor_tech";
    case Tex::FloorDirt: return "floor_dirt";
    case Tex::FloorBlood: return "floor_blood";
    case Tex::CeilStone: return "ceil_stone";
    case Tex::CeilRust: return "ceil_rust";
    case Tex::CeilDark: return "ceil_dark";
    case Tex::CeilTech: return "ceil_tech";
    default: return "unknown";
  }
}

const char* spriteName(SpriteId id) {
  switch (id) {
    case SpriteId::ZombieIdle: return "zombie_idle";
    case SpriteId::ZombieWalk: return "zombie_walk";
    case SpriteId::ZombieAttack: return "zombie_attack";
    case SpriteId::ZombiePain: return "zombie_pain";
    case SpriteId::ZombieDeath: return "zombie_death";
    case SpriteId::ImpIdle: return "imp_idle";
    case SpriteId::ImpWalk: return "imp_walk";
    case SpriteId::ImpAttack: return "imp_attack";
    case SpriteId::ImpPain: return "imp_pain";
    case SpriteId::ImpDeath: return "imp_death";
    case SpriteId::BruteIdle: return "brute_idle";
    case SpriteId::BruteWalk: return "brute_walk";
    case SpriteId::BruteAttack: return "brute_attack";
    case SpriteId::BrutePain: return "brute_pain";
    case SpriteId::BruteDeath: return "brute_death";
    case SpriteId::Fireball: return "fireball";
    case SpriteId::Rocket: return "rocket";
    case SpriteId::Explosion: return "explosion";
    case SpriteId::Barrel: return "barrel";
    case SpriteId::BarrelBroken: return "barrel_broken";
    case SpriteId::HealthSmall: return "pickup_health_small";
    case SpriteId::HealthLarge: return "pickup_health_large";
    case SpriteId::ArmorSmall: return "pickup_armor_small";
    case SpriteId::ArmorLarge: return "pickup_armor_large";
    case SpriteId::AmmoBullets: return "pickup_ammo_bullets";
    case SpriteId::AmmoShells: return "pickup_ammo_shells";
    case SpriteId::AmmoRockets: return "pickup_ammo_rockets";
    case SpriteId::AmmoCells: return "pickup_ammo_cells";
    case SpriteId::PickupShotgun: return "pickup_shotgun";
    case SpriteId::PickupChaingun: return "pickup_chaingun";
    case SpriteId::PickupLauncher: return "pickup_launcher";
    case SpriteId::PickupPlasma: return "pickup_plasma";
    case SpriteId::Lamp: return "decor_lamp";
    case SpriteId::TechPillar: return "decor_tech_pillar";
    case SpriteId::Gore: return "decor_gore";
    case SpriteId::WeaponPistol: return "viewmodel_pistol";
    case SpriteId::WeaponPistolFire: return "viewmodel_pistol_fire";
    case SpriteId::WeaponShotgun: return "viewmodel_shotgun";
    case SpriteId::WeaponShotgunFire: return "viewmodel_shotgun_fire";
    case SpriteId::WeaponChaingun: return "viewmodel_chaingun";
    case SpriteId::WeaponChaingunFire: return "viewmodel_chaingun_fire";
    case SpriteId::WeaponLauncher: return "viewmodel_launcher";
    case SpriteId::WeaponLauncherFire: return "viewmodel_launcher_fire";
    case SpriteId::WeaponPlasma: return "viewmodel_plasma";
    case SpriteId::WeaponPlasmaFire: return "viewmodel_plasma_fire";
    case SpriteId::Particle: return "particle";
    default: return "unknown";
  }
}

// ---------------------------------------------------------------------------
// Library build
// ---------------------------------------------------------------------------
namespace {

void genFoeSet(Assets& lib, Rng& rng, FoeKind kind, SpriteId idle, SpriteId walk, SpriteId attack,
               SpriteId pain, SpriteId death) {
  auto makePoses = [&](SpriteId slot, int frameCount, const char* suffix) {
    SpriteSet& set = lib.sprites[size_t(slot)];
    set.name = std::string(spriteName(slot)) + suffix;
    set.frames.resize(size_t(frameCount));
    for (int f = 0; f < frameCount; ++f) {
      Pose pose;
      if (slot == idle) {
        pose.bob = (f == 1) ? -1.0f : 0.0f;
      } else if (slot == walk) {
        pose.legPhase = std::sin(float(f) / float(frameCount) * kTau);
        pose.bob = (f % 2) ? -1.0f : 0.0f;
      } else if (slot == attack) {
        pose.armRaise = 1.0f;
        pose.flash = (f == frameCount - 1);
      } else if (slot == pain) {
        pose.lean = 1.5f;
      } else {
        pose.death = float(f) / float(std::max(1, frameCount - 1));
        pose.lean = -1.0f * pose.death;
      }
      drawFoe(set.frames[size_t(f)], kind, pose);
    }
  };
  makePoses(idle, 2, "");
  makePoses(walk, 4, "");
  makePoses(attack, 2, "");
  makePoses(pain, 1, "");
  makePoses(death, 5, "");
  (void)rng;
}

}  // namespace

void Assets::build(const std::vector<std::string>& dirs, bool verbose) {
  textureDirs = dirs;
  walls.assign(size_t(Tex::Count), Texture());
  sprites.assign(size_t(SpriteId::Count), SpriteSet());
  loadedFromDisk.clear();

  Rng rng(0xC0FFEEu);

  // ---- walls -------------------------------------------------------------
  genBrick(walls[size_t(Tex::WallBrick)], rng, rgba(150, 44, 32), rgba(74, 68, 62), 16, 8, 0.12f,
           1.0f);
  genBrick(walls[size_t(Tex::WallStone)], rng, rgba(124, 122, 118), rgba(62, 60, 58), 21, 10,
           0.16f, 0.6f);
  genPanelGrid(walls[size_t(Tex::WallMetal)], rng, rgba(96, 110, 134), 3, 2, true);
  genBrick(walls[size_t(Tex::WallBlue)], rng, rgba(44, 74, 156), rgba(30, 42, 82), 16, 8, 0.1f,
           0.5f);
  genPlanks(walls[size_t(Tex::WallWood)], rng, rgba(132, 88, 46), 4, true);
  genTechPanel(walls[size_t(Tex::WallGreen)], rng, rgba(44, 62, 58), rgba(96, 232, 140));
  genConcrete(walls[size_t(Tex::WallConcrete)], rng, rgba(112, 112, 106), 5);
  genBloodSplatter(walls[size_t(Tex::WallBlood)], rng, rgba(104, 100, 96), 1.0f);
  genCrate(walls[size_t(Tex::WallCrate)], rng);
  genSupport(walls[size_t(Tex::WallSupport)], rng, rgba(104, 108, 116));
  genFlag(walls[size_t(Tex::WallFlag)], rng);
  // Exit-sign wall: concrete with a glowing green EXIT plate.
  {
    Texture& t = walls[size_t(Tex::WallExit)];
    genConcrete(t, rng, rgba(88, 88, 84), 2);
    rect(t, 6, 20, 52, 24, rgba(14, 40, 20));
    rect(t, 8, 22, 48, 20, rgba(30, 96, 46));
    const int tw = fontTextWidth("EXIT", 2, 1);
    fontDrawTexture(t, (64 - tw) / 2, 27, "EXIT", rgba(190, 255, 190), 2, 1);
    rect(t, 6, 20, 52, 1, rgba(150, 200, 150));
    addNoise(t, rng, 0.05f);
  }

  // ---- doors -------------------------------------------------------------
  genDoorBody(walls[size_t(Tex::DoorMetal)], rng, rgba(108, 116, 132), false);
  genDoorBody(walls[size_t(Tex::DoorWood)], rng, rgba(130, 88, 48), true);
  {
    Texture& t = walls[size_t(Tex::DoorLocked)];
    genDoorBody(t, rng, rgba(96, 92, 76), false);
    // Hazard chevrons + gold keyhole.
    for (int i = 0; i < 64; i += 12) {
      for (int y = 0; y < 6; ++y) hlineT(t, i + y, 2 + y, 8, rgba(206, 176, 48));
    }
    ellipseFill(t, 32, 30, 5, 5, rgba(226, 190, 70));
    ellipseFill(t, 32, 30, 3, 3, rgba(40, 34, 20));
    rect(t, 30, 32, 4, 8, rgba(226, 190, 70));
  }
  {
    Texture& t = walls[size_t(Tex::DoorExit)];
    genDoorBody(t, rng, rgba(70, 96, 76), false);
    const int tw = fontTextWidth("EXIT", 2, 1);
    fontDrawTexture(t, (64 - tw) / 2, 26, "EXIT", rgba(200, 255, 200), 2, 1);
  }

  // ---- floors and ceilings ----------------------------------------------
  genCobbleFloor(walls[size_t(Tex::FloorStone)], rng, rgba(104, 102, 98), rgba(56, 55, 52));
  genCobbleFloor(walls[size_t(Tex::FloorCobble)], rng, rgba(122, 116, 104), rgba(50, 46, 42));
  genGrateFloor(walls[size_t(Tex::FloorMetal)], rng, rgba(98, 104, 112));
  genTechPanel(walls[size_t(Tex::FloorTech)], rng, rgba(38, 46, 50), rgba(80, 200, 220));
  genConcrete(walls[size_t(Tex::FloorDirt)], rng, rgba(96, 78, 56), 7);
  genBloodSplatter(walls[size_t(Tex::FloorBlood)], rng, rgba(84, 68, 54), 1.4f);
  genCobbleFloor(walls[size_t(Tex::CeilStone)], rng, rgba(66, 64, 62), rgba(38, 37, 36));
  genConcrete(walls[size_t(Tex::CeilRust)], rng, rgba(96, 66, 46), 6);
  {
    Texture& t = walls[size_t(Tex::CeilDark)];
    newTex(t, kTexSize, kTexSize);
    fill(t, rgba(26, 26, 30));
    for (int y = 0; y < 64; y += 16) hlineT(t, 0, y, 64, rgba(36, 36, 42));
    for (int x = 0; x < 64; x += 16) vlineT(t, x, 0, 64, rgba(36, 36, 42));
    addNoise(t, rng, 0.12f);
  }
  {
    Texture& t = walls[size_t(Tex::CeilTech)];
    newTex(t, kTexSize, kTexSize);
    fill(t, rgba(24, 30, 32));
    for (int y = 0; y < 64; y += 16) hlineT(t, 0, y, 64, rgba(70, 150, 130));
    for (int x = 8; x < 64; x += 32) vlineT(t, x, 0, 64, rgba(60, 120, 110));
    for (int y = 4; y < 64; y += 16)
      for (int x = 4; x < 64; x += 8) rect(t, x, y, 2, 2, rgba(120, 220, 190));
    addNoise(t, rng, 0.08f);
  }

  // ---- enemies -----------------------------------------------------------
  genFoeSet(*this, rng, FoeKind::Guard, SpriteId::ZombieIdle, SpriteId::ZombieWalk,
            SpriteId::ZombieAttack, SpriteId::ZombiePain, SpriteId::ZombieDeath);
  genFoeSet(*this, rng, FoeKind::Imp, SpriteId::ImpIdle, SpriteId::ImpWalk, SpriteId::ImpAttack,
            SpriteId::ImpPain, SpriteId::ImpDeath);
  genFoeSet(*this, rng, FoeKind::Brute, SpriteId::BruteIdle, SpriteId::BruteWalk,
            SpriteId::BruteAttack, SpriteId::BrutePain, SpriteId::BruteDeath);

  // ---- projectiles / effects --------------------------------------------
  {
    SpriteSet& set = sprites[size_t(SpriteId::Fireball)];
    set.name = spriteName(SpriteId::Fireball);
    set.frames.resize(3);
    for (int f = 0; f < 3; ++f) drawFireball(set.frames[size_t(f)], rng, f);
  }
  {
    SpriteSet& set = sprites[size_t(SpriteId::Rocket)];
    set.name = spriteName(SpriteId::Rocket);
    set.frames.resize(1);
    drawRocketSprite(set.frames[0], rng);
  }
  {
    SpriteSet& set = sprites[size_t(SpriteId::Explosion)];
    set.name = spriteName(SpriteId::Explosion);
    set.frames.resize(6);
    for (int f = 0; f < 6; ++f) drawExplosion(set.frames[size_t(f)], rng, f, 6);
  }

  // ---- props and pickups -------------------------------------------------
  {
    SpriteSet& set = sprites[size_t(SpriteId::Barrel)];
    set.name = spriteName(SpriteId::Barrel);
    set.frames.resize(1);
    drawBarrel(set.frames[0], rng, false);
  }
  {
    SpriteSet& set = sprites[size_t(SpriteId::BarrelBroken)];
    set.name = spriteName(SpriteId::BarrelBroken);
    set.frames.resize(1);
    drawBarrel(set.frames[0], rng, true);
  }
  {
    SpriteSet& set = sprites[size_t(SpriteId::HealthSmall)];
    set.name = spriteName(SpriteId::HealthSmall);
    set.frames.resize(1);
    drawCrossBox(set.frames[0], 20, 16, rgba(226, 226, 220));
  }
  {
    SpriteSet& set = sprites[size_t(SpriteId::HealthLarge)];
    set.name = spriteName(SpriteId::HealthLarge);
    set.frames.resize(1);
    drawCrossBox(set.frames[0], 28, 22, rgba(236, 236, 230));
  }
  {
    SpriteSet& set = sprites[size_t(SpriteId::ArmorSmall)];
    set.name = spriteName(SpriteId::ArmorSmall);
    set.frames.resize(1);
    Texture& t = set.frames[0];
    drawPickupBox(t, rgba(56, 132, 68), rgba(40, 96, 52), 24, 20, "", rgba(0, 0, 0));
    rect(t, 26, 48, 12, 8, rgba(80, 200, 110));
    rect(t, 24, 56, 16, 4, rgba(60, 160, 84));
  }
  {
    SpriteSet& set = sprites[size_t(SpriteId::ArmorLarge)];
    set.name = spriteName(SpriteId::ArmorLarge);
    set.frames.resize(1);
    Texture& t = set.frames[0];
    drawPickupBox(t, rgba(52, 96, 168), rgba(38, 70, 130), 30, 26, "", rgba(0, 0, 0));
    rect(t, 22, 42, 20, 12, rgba(120, 190, 255));
    rect(t, 20, 52, 24, 6, rgba(80, 140, 220));
  }
  {
    SpriteSet& set = sprites[size_t(SpriteId::AmmoBullets)];
    set.name = spriteName(SpriteId::AmmoBullets);
    set.frames.resize(1);
    drawAmmoBox(set.frames[0], rng, rgba(112, 92, 56), rgba(196, 170, 90), false, false, false);
  }
  {
    SpriteSet& set = sprites[size_t(SpriteId::AmmoShells)];
    set.name = spriteName(SpriteId::AmmoShells);
    set.frames.resize(1);
    drawAmmoBox(set.frames[0], rng, rgba(120, 60, 44), rgba(196, 60, 44), true, false, false);
  }
  {
    SpriteSet& set = sprites[size_t(SpriteId::AmmoRockets)];
    set.name = spriteName(SpriteId::AmmoRockets);
    set.frames.resize(1);
    drawAmmoBox(set.frames[0], rng, rgba(80, 84, 72), rgba(150, 154, 140), false, true, false);
  }
  {
    SpriteSet& set = sprites[size_t(SpriteId::AmmoCells)];
    set.name = spriteName(SpriteId::AmmoCells);
    set.frames.resize(1);
    drawAmmoBox(set.frames[0], rng, rgba(48, 60, 96), rgba(70, 110, 190), false, false, true);
  }
  {
    const SpriteId ids[4] = {SpriteId::PickupShotgun, SpriteId::PickupChaingun,
                             SpriteId::PickupLauncher, SpriteId::PickupPlasma};
    for (int i = 0; i < 4; ++i) {
      SpriteSet& set = sprites[size_t(ids[i])];
      set.name = spriteName(ids[i]);
      set.frames.resize(1);
      drawWeaponPickup(set.frames[0], rng, i);
    }
  }
  {
    SpriteSet& set = sprites[size_t(SpriteId::Lamp)];
    set.name = spriteName(SpriteId::Lamp);
    set.frames.resize(1);
    drawLamp(set.frames[0], rng);
  }
  {
    SpriteSet& set = sprites[size_t(SpriteId::TechPillar)];
    set.name = spriteName(SpriteId::TechPillar);
    set.frames.resize(1);
    drawTechPillar(set.frames[0], rng);
  }
  {
    SpriteSet& set = sprites[size_t(SpriteId::Gore)];
    set.name = spriteName(SpriteId::Gore);
    set.frames.resize(1);
    drawGore(set.frames[0], rng);
  }

  {
    SpriteSet& set = sprites[size_t(SpriteId::Particle)];
    set.name = spriteName(SpriteId::Particle);
    set.frames.resize(1);
    Texture& t = set.frames[0];
    newTex(t, 16, 16);
    for (int y = 0; y < 16; ++y) {
      for (int x = 0; x < 16; ++x) {
        const float dx = (float(x) - 7.5f) / 7.5f;
        const float dy = (float(y) - 7.5f) / 7.5f;
        const float d = std::sqrt(dx * dx + dy * dy);
        const float a = clampf(1.0f - d, 0.0f, 1.0f);
        setTexel(t, x, y, rgba(255, 255, 255, int(a * a * 255.0f)));
      }
    }
  }

  // ---- first-person weapon view models -----------------------------------
  {
    const SpriteId idle[5] = {SpriteId::WeaponPistol, SpriteId::WeaponShotgun,
                              SpriteId::WeaponChaingun, SpriteId::WeaponLauncher,
                              SpriteId::WeaponPlasma};
    const SpriteId fire[5] = {SpriteId::WeaponPistolFire, SpriteId::WeaponShotgunFire,
                              SpriteId::WeaponChaingunFire, SpriteId::WeaponLauncherFire,
                              SpriteId::WeaponPlasmaFire};
    for (int i = 0; i < 5; ++i) {
      SpriteSet& a = sprites[size_t(idle[i])];
      a.name = spriteName(idle[i]);
      a.frames.resize(1);
      drawFirstPerson(a.frames[0], rng, i, false);

      SpriteSet& b = sprites[size_t(fire[i])];
      b.name = spriteName(fire[i]);
      b.frames.resize(1);
      drawFirstPerson(b.frames[0], rng, i, true);
    }
  }

  // ---- disk overrides ----------------------------------------------------
  for (int i = 0; i < int(Tex::Count); ++i) {
    const std::string base = texName(Tex(i));
    for (const std::string& dir : textureDirs) {
      bool found = false;
      for (const char* ext : {".png", ".bmp", ".ppm", ".PNG"}) {
        const std::string path = dir + "/" + base + ext;
        Texture loaded;
        if (loadTextureFile(path, loaded) && !loaded.empty()) {
          walls[size_t(i)] = std::move(loaded);
          walls[size_t(i)].finalize();
          loadedFromDisk.push_back(base);
          found = true;
          break;
        }
      }
      if (found) break;
    }
  }
  for (int i = 0; i < int(SpriteId::Count); ++i) {
    SpriteSet& set = sprites[size_t(i)];
    for (int f = 0; f < set.count(); ++f) {
      char suffix[16] = {0};
      if (set.count() > 1) std::snprintf(suffix, sizeof(suffix), "_%d", f);
      const std::string base = std::string(spriteName(SpriteId(i))) + suffix;
      for (const std::string& dir : textureDirs) {
        bool found = false;
        for (const char* ext : {".png", ".bmp", ".ppm", ".PNG"}) {
          const std::string path = dir + "/" + base + ext;
          Texture loaded;
          if (loadTextureFile(path, loaded) && !loaded.empty()) {
            set.frames[size_t(f)] = std::move(loaded);
            set.frames[size_t(f)].finalize();
            loadedFromDisk.push_back(base);
            found = true;
            break;
          }
        }
        if (found) break;
      }
    }
  }

  if (verbose) {
    std::printf("[assets] %d textures, %d sprite slots\n", int(Tex::Count),
                int(SpriteId::Count));
    if (loadedFromDisk.empty()) {
      std::printf("[assets] using built-in procedural art (no overrides found)\n");
    } else {
      std::printf("[assets] %d slot(s) overridden from disk:\n",
                  int(loadedFromDisk.size()));
      for (const std::string& n : loadedFromDisk) std::printf("           %s\n", n.c_str());
    }
  }
}

void Assets::dumpDefaults(const std::string& dir) const {
  int written = 0;
  auto emit = [&](const std::string& base, const Texture& tex) {
    if (tex.empty()) return;
    // Sprites keep their alpha channel, so prefer PNG when we can write it.
    const std::string pngPath = dir + "/" + base + ".png";
    if (writePng(pngPath, tex)) {
      ++written;
      return;
    }
    const std::string ppmPath = dir + "/" + base + ".ppm";
    if (writePpm(ppmPath, tex)) ++written;
  };

  for (int i = 0; i < int(Tex::Count); ++i) emit(texName(Tex(i)), walls[size_t(i)]);
  for (int i = 0; i < int(SpriteId::Count); ++i) {
    const SpriteSet& set = sprites[size_t(i)];
    for (int f = 0; f < set.count(); ++f) {
      char suffix[16] = {0};
      if (set.count() > 1) std::snprintf(suffix, sizeof(suffix), "_%d", f);
      emit(std::string(spriteName(SpriteId(i))) + suffix, set.frames[size_t(f)]);
    }
  }
  std::printf("[assets] wrote %d template file(s) to %s\n", written, dir.c_str());
}

}  // namespace fps
