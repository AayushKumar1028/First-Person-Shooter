// render_check.cpp - headless renderer smoke test.
//
// Renders a few known scenes and prints them as ASCII luminance art so the
// geometry can be eyeballed from a terminal (and asserted in CI). Build with the
// "render_check" target, then run: ./render_check
#include <cstdio>
#include <string>
#include <vector>

#include "assets.h"
#include "level.h"
#include "render.h"

using namespace fps;

namespace {

const char* kRamp = " .:-=+*#%@";

void printFrame(const Framebuffer& fb, const char* title) {
  std::printf("\n=== %s (%dx%d) ===\n", title, fb.w, fb.h);
  for (int y = 0; y < fb.h; ++y) {
    std::string line;
    line.reserve(size_t(fb.w));
    for (int x = 0; x < fb.w; ++x) {
      const RGBA c = fb.get(x, y);
      const int lum = (redOf(c) * 30 + greenOf(c) * 59 + blueOf(c) * 11) / 100;
      line.push_back(kRamp[clampi(lum * 10 / 256, 0, 9)]);
    }
    std::printf("%s\n", line.c_str());
  }
}

std::vector<std::string> testMap() {
  // Exercises: textured walls on both axes, a pillar, a door in the middle of a
  // corridor, and different floor/ceiling tiles.
  return {
      "##############################",
      "#,,,,,,,,,,,,,,,,,,,,,,,,,,,,#",
      "#,..........MM..........,,,,,#",
      "#,..........MM..........,,,,,#",
      "#,......................^^^^^#",
      "#,....%%%%........%%%...,,,,,#",
      "#,....%%%%........%%%...,,,,,#",
      "#,......................,,,,,#",
      "#,.......DD.............,,,,,#",
      "#,.......DD.............,,,,,#",
      "#,......................,,,,,#",
      "#,___...................:::::#",
      "#,___...................:::::#",
      "#,,,,,,,,,,,,,,,,,,,,,,,,,,,,#",
      "##############################",
  };
}

}  // namespace

int main() {
  Assets assets;
  assets.build({}, true);

  Level level;
  level.defaultFloor = Tex::FloorCobble;
  level.defaultCeil = Tex::CeilStone;
  if (!level.loadAscii("TEST", testMap())) {
    std::printf("failed to load test map\n");
    return 1;
  }
  std::printf("map %dx%d, doors=%d\n", level.w, level.h, int(level.doors.size()));

  Framebuffer fb;
  fb.resize(104, 30);
  Renderer renderer;
  renderer.setAssets(&assets);

  RenderSettings rs;
  rs.fogDistance = 20.0f;

  // --- 1. Facing down the corridor from the west end ----------------------
  {
    Camera cam;
    cam.pos = Vec2(2.5f, 2.5f);
    cam.angle = 0.0f;
    cam.height = 0.5f;
    renderer.renderWorld(fb, level, cam, {}, rs);
    printFrame(fb, "east down the corridor, level pitch");
    // Row 2 has a metal block at x=12, so the centre ray must stop 9.5 units out.
    std::printf("depth at screen centre = %.2f (expected 9.50: the 'M' block at x=12)\n",
                renderer.depth()[fb.w / 2]);
  }

  // --- 2. Pitch up and down (y-shearing) ----------------------------------
  for (float pitchDeg : {-20.0f, 20.0f}) {
    Camera cam;
    cam.pos = Vec2(2.5f, 2.5f);
    cam.angle = 0.0f;
    cam.pitch = pitchDeg * kDeg2Rad;
    renderer.renderWorld(fb, level, cam, {}, rs);
    printFrame(fb, pitchDeg < 0 ? "pitch down 20deg" : "pitch up 20deg");
  }

  // --- 3. Door closed vs rammed open --------------------------------------
  {
    Camera cam;
    cam.pos = Vec2(3.5f, 8.5f);
    cam.angle = 0.0f;
    renderer.renderWorld(fb, level, cam, {}, rs);
    printFrame(fb, "door shut, standing in front of it");

    level.doors[0].open = 0.65f;
    renderer.renderWorld(fb, level, cam, {}, rs);
    printFrame(fb, "door 65% open (should reveal the room behind)");
  }

  // --- 4. Sprite billboards against a wall --------------------------------
  {
    Camera cam;
    cam.pos = Vec2(3.5f, 3.5f);
    cam.angle = 0.0f;
    std::vector<SpriteDraw> sprites;
    sprites.push_back({&assets.anim(SpriteId::ZombieIdle).frame(0), Vec2(7.5f, 3.5f), 0.0f, 0.85f});
    sprites.push_back({&assets.anim(SpriteId::Barrel).frame(0), Vec2(5.0f, 4.6f), 0.0f, 0.85f});
    sprites.push_back({&assets.anim(SpriteId::HealthLarge).frame(0), Vec2(4.0f, 2.4f), 0.0f, 0.85f});
    renderer.renderWorld(fb, level, cam, sprites, rs);
    printFrame(fb, "three sprites at different distances");
  }

  // --- 5. Arena generator sanity ------------------------------------------
  {
    Level arena;
    arena.buildArena(1234, 3);
    int open = 0;
    for (int y = 0; y < arena.h; ++y)
      for (int x = 0; x < arena.w; ++x)
        if (!arena.isWall(x, y)) ++open;
    std::printf("\narena %dx%d, open cells=%d, player start=(%.1f,%.1f)\n", arena.w, arena.h, open,
                arena.playerStart.x, arena.playerStart.y);
    arena.computeFlow(arena.playerStart);
    std::printf("flow field reachable cells=%d (should be close to open cells)\n", [&] {
      int n = 0;
      for (int y = 0; y < arena.h; ++y)
        for (int x = 0; x < arena.w; ++x)
          if (arena.flowAt(x, y) >= 0) ++n;
      return n;
    }());
  }

  std::printf("\nrender_check finished\n");
  return 0;
}
