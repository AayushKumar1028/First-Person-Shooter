// render.h - software raycasting renderer (Wolfenstein/Doom style).
#pragma once

#include <vector>

#include "assets.h"
#include "framebuffer.h"
#include "level.h"

namespace fps {

struct Camera {
  Vec2 pos{1.5f, 1.5f};
  float angle = 0.0f;   // yaw in radians
  float pitch = 0.0f;   // radians, positive looks up
  float height = 0.5f;  // eye height in world units (0 = floor, 1 = ceiling)
  float fov = 74.0f;    // horizontal field of view in degrees
};

// A billboard, ready to be drawn. The texture's canvas sits on the floor at
// world z = "z" and is "height" world units tall.
struct SpriteDraw {
  const Texture* tex = nullptr;
  Vec2 pos;
  float z = 0.0f;
  float height = 0.85f;
  float alpha = 1.0f;
  float emissive = 0.0f;  // 0 = distance-shaded, 1 = full bright
  RGBA tint = 0xFFFFFFFFu;
  bool flat = false;  // decorative floor decals get no distance darkening
};

struct RenderSettings {
  float fogDistance = 18.0f;  // distance at which shading bottoms out
  float ambient = 0.2f;       // minimum brightness
  bool drawFloors = true;
  bool drawCeiling = true;
  bool drawSprites = true;
  bool drawWalls = true;  // off for a quick wireframe-ish "no walls" debug view
  RGBA voidColor = 0xFF08080Au;
};

class Renderer {
 public:
  void setAssets(const Assets* assets) { assets_ = assets; }
  void resize(int w, int h);

  // Renders the world into "fb". Sprites may be in any order; they are sorted
  // internally (back to front).
  void renderWorld(Framebuffer& fb, const Level& level, const Camera& camera,
                   const std::vector<SpriteDraw>& sprites, const RenderSettings& settings);

  // Per-column wall depth from the last frame - handy for effects and tests.
  const std::vector<float>& depth() const { return depth_; }

  // Projection plane distance in pixels for the given FOV and width.
  static float projectionDistance(int width, float fovDegrees);

 private:
  const Assets* assets_ = nullptr;
  std::vector<float> depth_;
};

}  // namespace fps
