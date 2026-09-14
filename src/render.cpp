// render.cpp - the software renderer.
//
// Pipeline per frame:
//   1. floors + ceilings by scanning rows and casting them back to the grid
//   2. walls by casting one ray per screen column (DDA)
//   3. billboard sprites, sorted back to front, clipped by a per-column depth buffer
//
// The projection plane distance is derived from the field of view, which keeps
// pixels square no matter what internal resolution is used.
#include "render.h"

#include <algorithm>

namespace fps {

float Renderer::projectionDistance(int width, float fovDegrees) {
  const float halfFov = clampf(fovDegrees, 30.0f, 120.0f) * 0.5f * kDeg2Rad;
  return (float(width) * 0.5f) / std::tan(halfFov);
}

void Renderer::resize(int w, int h) { depth_.assign(size_t(std::max(0, w)), 1e30f); }

namespace {

inline float lightAt(float dist, const RenderSettings& rs) {
  const float f = 1.0f - dist / std::max(1.0f, rs.fogDistance);
  return clampf(f, rs.ambient, 1.0f);
}

inline RGBA applyTint(RGBA c, RGBA tint) {
  if (tint == 0xFFFFFFFFu) return c;
  const int r = redOf(c) * redOf(tint) / 255;
  const int g = greenOf(c) * greenOf(tint) / 255;
  const int b = blueOf(c) * blueOf(tint) / 255;
  return rgba(r, g, b, alphaOf(c));
}

}  // namespace

void Renderer::renderWorld(Framebuffer& fb, const Level& level, const Camera& camera,
                           const std::vector<SpriteDraw>& sprites,
                           const RenderSettings& settings) {
  if (!assets_ || fb.w <= 0 || fb.h <= 0) return;
  if (int(depth_.size()) != fb.w) resize(fb.w, fb.h);

  const Vec2 dir = fromAngle(camera.angle);
  const Vec2 right(-dir.y, dir.x);
  const float projDist = projectionDistance(fb.w, camera.fov);
  // Half the projection plane in world units per screen half-width.
  const float planeScale = (float(fb.w) * 0.5f) / projDist;
  const Vec2 plane = right * planeScale;

  const float posX = camera.pos.x;
  const float posY = camera.pos.y;
  const float camZ = clampf(camera.height, 0.1f, 0.9f);
  // Looking up shifts the horizon down the screen by this many pixels.
  const int horizon = int(std::round(float(fb.h) * 0.5f + projDist * std::tan(camera.pitch)));

  const float rayDirX0 = dir.x - plane.x;
  const float rayDirY0 = dir.y - plane.y;
  const float rayDirX1 = dir.x + plane.x;
  const float rayDirY1 = dir.y + plane.y;

  // -----------------------------------------------------------------------
  // 1. Floors and ceilings
  // -----------------------------------------------------------------------
  if (settings.drawFloors || settings.drawCeiling) {
    for (int y = 0; y < fb.h; ++y) {
      const bool isFloor = y > horizon;
      if (isFloor && !settings.drawFloors) continue;
      if (!isFloor && !settings.drawCeiling) continue;
      if (y == horizon) continue;

      const int p = isFloor ? (y - horizon) : (horizon - y);
      if (p <= 0) continue;

      // Distance to the floor/ceiling point on this row.
      const float rowDist = projDist * camZ / float(p);
      if (rowDist > 400.0f) continue;

      const float stepX = rowDist * (rayDirX1 - rayDirX0) / float(fb.w);
      const float stepY = rowDist * (rayDirY1 - rayDirY0) / float(fb.w);
      float fx = posX + rowDist * rayDirX0;
      float fy = posY + rowDist * rayDirY0;

      const float light = lightAt(rowDist, settings);
      RGBA* dst = fb.row(y);
      const Texture* cachedTex = nullptr;
      int cachedCell = -1;

      for (int x = 0; x < fb.w; ++x) {
        const int cx = int(std::floor(fx));
        const int cy = int(std::floor(fy));
        if (cx < 0 || cy < 0 || cx >= level.w || cy >= level.h) {
          dst[x] = settings.voidColor;
        } else {
          const int cell = cy * level.w + cx;
          if (cell != cachedCell) {
            cachedCell = cell;
            const Tile& tile = level.at(cx, cy);
            const Tex which = isFloor ? tile.floor : tile.ceil;
            cachedTex = &assets_->tex(which);
          }
          const Texture& tex = *cachedTex;
          const int tx = int((fx - float(cx)) * float(tex.w));
          const int ty = int((fy - float(cy)) * float(tex.h));
          dst[x] = shade(tex.texel(tx, ty), light);
        }
        fx += stepX;
        fy += stepY;
      }
    }
  }

  // -----------------------------------------------------------------------
  // 2. Walls
  // -----------------------------------------------------------------------
  std::fill(depth_.begin(), depth_.end(), 1e30f);

  if (settings.drawWalls) {
    for (int x = 0; x < fb.w; ++x) {
      const float cameraX = 2.0f * float(x) / float(fb.w) - 1.0f;
      const float rayDirX = dir.x + plane.x * cameraX;
      const float rayDirY = dir.y + plane.y * cameraX;

      int mapX = int(std::floor(posX));
      int mapY = int(std::floor(posY));

      const float deltaX = (std::abs(rayDirX) < 1e-8f) ? 1e30f : std::abs(1.0f / rayDirX);
      const float deltaY = (std::abs(rayDirY) < 1e-8f) ? 1e30f : std::abs(1.0f / rayDirY);
      const int stepX = rayDirX < 0.0f ? -1 : 1;
      const int stepY = rayDirY < 0.0f ? -1 : 1;

      float sideDistX = (rayDirX < 0.0f) ? (posX - float(mapX)) * deltaX
                                         : (float(mapX + 1) - posX) * deltaX;
      float sideDistY = (rayDirY < 0.0f) ? (posY - float(mapY)) * deltaY
                                         : (float(mapY + 1) - posY) * deltaY;

      bool hit = false;
      bool side = false;
      bool isDoor = false;
      int guard = 0;
      while (guard++ < 512) {
        if (sideDistX < sideDistY) {
          sideDistX += deltaX;
          mapX += stepX;
          side = false;
        } else {
          sideDistY += deltaY;
          mapY += stepY;
          side = true;
        }
        if (!level.inside(mapX, mapY)) break;
        const Tile& tile = level.at(mapX, mapY);
        const bool doorTile = (tile.kind == TileKind::Door || tile.kind == TileKind::ExitDoor);
        if (tile.kind == TileKind::Wall) {
          hit = true;
          isDoor = false;
          break;
        }
        if (doorTile) {
          // A nearly open door lets the ray slip past it.
          if (level.doorOpen(mapX, mapY) >= 0.99f) continue;
          hit = true;
          isDoor = true;
          break;
        }
      }

      if (!hit) {
        // Outside the map: draw the void so the player never sees through it.
        for (int y = 0; y < fb.h; ++y) fb.set(x, y, settings.voidColor);
        depth_[size_t(x)] = 1e30f;
        continue;
      }

      const float perpDist = side ? (sideDistY - deltaY) : (sideDistX - deltaX);
      const float dist = std::max(0.02f, perpDist);
      depth_[size_t(x)] = dist;

      const Tile& tile = level.at(mapX, mapY);
      const Texture& tex = assets_->tex(tile.wall);
      const float doorOpenAmount = isDoor ? level.doorOpen(mapX, mapY) : 0.0f;

      // Vertical extent of the visible wall slab in world units. Regular walls
      // span floor (0) to ceiling (1); a sliding door shrinks from the bottom.
      const float wyTop = 1.0f;
      const float wyBottom = isDoor ? doorOpenAmount : 0.0f;

      const float pxPerWorld = projDist / dist;
      const float yTop = float(horizon) - pxPerWorld * (wyTop - camZ);
      const float yBottom = float(horizon) - pxPerWorld * (wyBottom - camZ);
      if (yBottom <= yTop) continue;

      // Texture V: the top of the wall is v=0.
      const float vTop = 1.0f - wyTop;
      const float vBottom = 1.0f - wyBottom;

      const int y0 = std::max(0, int(std::ceil(yTop)));
      const int y1 = std::min(fb.h - 1, int(std::floor(yBottom)));
      if (y1 < y0) continue;

      const float invSpan = 1.0f / (yBottom - yTop);
      const float dvPerPixel = (vBottom - vTop) * invSpan;

      // Horizontal texture coordinate across the hit face.
      float wallX = side ? (posX + dist * rayDirX) : (posY + dist * rayDirY);
      wallX -= std::floor(wallX);
      if ((!side && rayDirX > 0.0f) || (side && rayDirY < 0.0f)) wallX = 1.0f - wallX;
      const int texX = clampi(int(wallX * float(tex.w)), 0, tex.w - 1);

      // Darken one axis so corners stay readable.
      float light = lightAt(dist, settings) * (side ? 0.72f : 1.0f);
      if (isDoor && doorOpenAmount > 0.0f) light *= 1.05f;

      float v = vTop + (float(y0) - yTop) * dvPerPixel;
      for (int y = y0; y <= y1; ++y) {
        const int texY = clampi(int(v * float(tex.h)), 0, tex.h - 1);
        fb.set(x, y, shade(tex.px[size_t(texY) * size_t(tex.w) + size_t(texX)], light));
        v += dvPerPixel;
      }
    }
  }

  // -----------------------------------------------------------------------
  // 3. Sprites
  // -----------------------------------------------------------------------
  if (settings.drawSprites && !sprites.empty()) {
    struct Ordered {
      const SpriteDraw* s;
      float depth;
    };
    std::vector<Ordered> order;
    order.reserve(sprites.size());
    for (const SpriteDraw& s : sprites) {
      if (!s.tex || s.tex->empty() || s.alpha <= 0.0f) continue;
      const Vec2 d = s.pos - camera.pos;
      order.push_back({&s, dot(d, d)});
    }
    std::sort(order.begin(), order.end(),
              [](const Ordered& a, const Ordered& b) { return a.depth > b.depth; });

    const float invDet = 1.0f / (plane.x * dir.y - dir.x * plane.y);

    for (const Ordered& item : order) {
      const SpriteDraw& s = *item.s;
      const Texture& tex = *s.tex;
      const float relX = s.pos.x - posX;
      const float relY = s.pos.y - posY;

      // Transform into camera space.
      const float transformX = invDet * (dir.y * relX - dir.x * relY);
      const float transformY = invDet * (-plane.y * relX + plane.x * relY);
      if (transformY < 0.08f) continue;

      const int screenX = int(std::round(float(fb.w) * 0.5f * (1.0f + transformX / transformY)));
      const float pxPerWorld = projDist / transformY;
      const float aspect = float(tex.w) / float(tex.h);
      const float spriteH = pxPerWorld * s.height;
      const float spriteW = spriteH * aspect;
      if (spriteH < 1.0f) continue;

      const float yTop = float(horizon) - pxPerWorld * (s.z + s.height - camZ);
      const float yBottom = float(horizon) - pxPerWorld * (s.z - camZ);
      const float xLeft = float(screenX) - spriteW * 0.5f;

      const int x0 = std::max(0, int(std::ceil(xLeft)));
      const int x1 = std::min(fb.w - 1, int(std::floor(xLeft + spriteW)));
      const int y0 = std::max(0, int(std::ceil(yTop)));
      const int y1 = std::min(fb.h - 1, int(std::floor(yBottom)));
      if (x1 < x0 || y1 < y0) continue;

      float light = lightAt(transformY, settings);
      if (s.flat) light = 1.0f;
      light = lerpf(light, 1.0f, clampf(s.emissive, 0.0f, 1.0f));
      const float invSpanY = 1.0f / std::max(1e-4f, yBottom - yTop);
      const float invSpanX = 1.0f / std::max(1e-4f, spriteW);

      for (int x = x0; x <= x1; ++x) {
        if (transformY >= depth_[size_t(x)]) continue;  // hidden behind a wall
        const int texX = clampi(int((float(x) - xLeft) * invSpanX * float(tex.w)), 0, tex.w - 1);
        for (int y = y0; y <= y1; ++y) {
          const int texY = clampi(int((float(y) - yTop) * invSpanY * float(tex.h)), 0, tex.h - 1);
          const RGBA c = tex.px[size_t(texY) * size_t(tex.w) + size_t(texX)];
          const int a = alphaOf(c);
          if (a == 0) continue;
          const RGBA lit = shade(applyTint(c, s.tint), light);
          if (a == 255) {
            fb.set(x, y, lit);
          } else {
            fb.blend(x, y, lit, (float(a) / 255.0f) * s.alpha);
          }
        }
      }
    }
  }
}

}  // namespace fps
