// main.cpp - entry point, CLI options and the headless self test.
#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "assets.h"
#include "audio.h"
#include "entities.h"
#include "game.h"
#include "level.h"

using namespace fps;

namespace {

void printUsage(const char* exe) {
  std::printf(
      "FPS SHOOTER - a Doom/Wolfenstein style raycasting FPS in C++ with SDL2\n"
      "\n"
      "usage: %s [options]\n"
      "\n"
      "display\n"
      "  --size WxH        window size (default 1280x800)\n"
      "  --render WxH      internal render resolution (default 480x300)\n"
      "  -f, --fullscreen  start fullscreen\n"
      "  --fov DEGREES     horizontal field of view (default 74)\n"
      "\n"
      "gameplay\n"
      "  --level N         jump straight into campaign level N (1-based)\n"
      "  --arena           jump straight into endless arena mode\n"
      "  --sens VALUE      mouse sensitivity (default 0.0024)\n"
      "  --invert-y        invert vertical mouse look\n"
      "  --mute            no audio device\n"
      "\n"
      "assets\n"
      "  --dump-textures   write every built-in texture to assets/textures/\n"
      "                    (drop-in replacements can then be painted over)\n"
      "\n"
      "testing\n"
      "  --selftest        run a headless scripted demo and print stats\n"
      "  --frames N        how many ticks the self test runs (default 600)\n"
      "  --shot FILE.png   with --selftest, save the last frame as an image\n"
      "  --quiet           less console output\n"
      "  -h, --help        this text\n",
      exe);
}

// Renders a framebuffer as terminal art so a headless run can be eyeballed.
void printAsciiFrame(const Framebuffer& fb, int columns, const char* title) {
  if (fb.w <= 0 || fb.h <= 0 || columns <= 8) return;
  const int rows = std::max(4, int(float(columns) * float(fb.h) / float(fb.w) * 0.5f));
  static const char* ramp = " .:-=+*#%@";
  std::printf("\n--- %s (%dx%d -> %dx%d) ---\n", title, fb.w, fb.h, columns, rows);
  for (int r = 0; r < rows; ++r) {
    std::string line;
    line.reserve(size_t(columns));
    for (int c = 0; c < columns; ++c) {
      // Box-average each cell so thin features stay visible.
      const int x0 = c * fb.w / columns, x1 = std::max(x0 + 1, (c + 1) * fb.w / columns);
      const int y0 = r * fb.h / rows, y1 = std::max(y0 + 1, (r + 1) * fb.h / rows);
      int sum = 0, count = 0;
      for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
          const RGBA p = fb.get(x, y);
          sum += (redOf(p) * 30 + greenOf(p) * 59 + blueOf(p) * 11) / 100;
          ++count;
        }
      }
      const int lum = count > 0 ? sum / count : 0;
      line.push_back(ramp[clampi(lum * 10 / 256, 0, 9)]);
    }
    std::printf("%s\n", line.c_str());
  }
}

// Wraps a framebuffer as a Texture so it can be written to disk as a PNG.
Texture toTexture(const Framebuffer& fb) {
  Texture out;
  out.w = fb.w;
  out.h = fb.h;
  out.px = fb.px;
  out.finalize();
  return out;
}

// Deterministic mechanics tests: no window, no timing, fixed maps.
int runMechanicsTest() {
  std::printf("\n=== MECHANICS ===\n");
  Assets assets;
  assets.build({}, false);
  Audio audio;  // never opened: play() is a no-op, which is what we want here

  auto check = [](bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? "ok" : "FAIL", what);
    return ok ? 0 : 1;
  };

  int failures = 0;

  Level level;
  level.defaultFloor = Tex::FloorStone;
  level.defaultCeil = Tex::CeilStone;
  const std::vector<std::string> rows = {
      "#########",
      "#.......#",
      "#.o...o.#",
      "#.......#",
      "#...z...#",
      "#...h...#",
      "#..L....#",
      "#########",
  };
  if (!level.loadAscii("MECH", rows)) {
    std::printf("  [FAIL] test map did not load\n");
    return 1;
  }

  World world;
  world.reset(&level, &assets, &audio);
  world.spawnFromLevel();

  failures += check(world.enemyTotal() == 1, "level spawn created 1 hostile");
  failures += check(world.propCount() == 2, "level spawn created 2 barrels");
  failures += check(world.pickupCount() == 1, "level spawn created 1 pickup");
  failures += check(world.aliveEnemies() == 1, "hostile starts alive");

  // --- combat ----------------------------------------------------------
  const int healthBeforeKill = world.player().health;
  world.damageEnemy(0, 500, true);
  failures += check(world.enemies()[0].dead(), "hostile dies when damaged");
  failures += check(world.player().kills == 1, "kill is counted for the player");
  failures += check(world.score > 0, "score awarded for a kill");
  failures += check(world.player().health == healthBeforeKill, "killing does not heal the player");
  failures += check(world.aliveEnemies() == 0, "no hostiles remain");

  // --- explosions and barrel chains ------------------------------------
  world.explode(Vec2(2.5f, 2.5f), 3.4f, 120, true);
  int destroyed = 0;
  for (const Prop& p : world.props()) {
    if (p.explosive && p.destroyed) ++destroyed;
  }
  failures += check(destroyed >= 1, "explosion destroys a barrel next to it");
  failures += check(world.player().shake > 0.0f, "explosion shakes the camera");

  // --- pickups ---------------------------------------------------------
  world.player().health = 80;
  world.player().pos = Vec2(4.5f, 5.5f);  // the 'h' stimpack cell
  world.update(1.0f / 60.0f, InputState());
  failures += check(world.player().health == 90, "stimpack heals 10");
  failures += check(world.pickupCount() == 0, "collected pickup is removed");

  world.player().health = 100;
  world.addPickup(SpawnKind::HealthSmall, Vec2(4.5f, 5.5f));
  world.update(1.0f / 60.0f, InputState());
  failures += check(world.pickupCount() == 1, "health is not wasted at full health");

  world.addPickup(SpawnKind::AmmoBullets, Vec2(4.5f, 5.5f));
  const int bulletsBefore = world.player().ammo[0];
  world.update(1.0f / 60.0f, InputState());
  failures += check(world.player().ammo[0] == bulletsBefore + 20, "ammo pickup grants 20 bullets");

  // --- armour absorbs a third of incoming damage -----------------------
  world.player().health = 100;
  world.player().armor = 0;
  world.damagePlayer(30);
  failures += check(world.player().health == 70, "30 damage past no armour leaves 70 hp");
  world.player().health = 100;
  world.player().armor = 100;
  world.damagePlayer(30);
  failures += check(world.player().health == 80, "armour soaks a third (only 20 hp lost)");
  failures += check(world.player().armor == 90, "armour is spent by the amount absorbed");

  // --- weapons ---------------------------------------------------------
  failures += check(!world.player().hasWeapon[int(WeaponId::Shotgun)], "shotgun not owned at start");
  world.addPickup(SpawnKind::Shotgun, Vec2(4.5f, 5.5f));
  world.update(1.0f / 60.0f, InputState());
  failures += check(world.player().hasWeapon[int(WeaponId::Shotgun)], "shotgun pickup grants it");
  failures += check(world.player().weapon == int(WeaponId::Shotgun), "picking up a weapon equips it");

  // --- doors slide, block, then close again -----------------------------
  failures += check(level.doorIndexAt(3, 6) >= 0, "door tile is registered");
  failures += check(level.isSolid(3, 6), "closed door blocks movement");
  std::string message;
  failures += check(!level.tryOpenDoor(3, 6, true, false, &message),
                    "locked door refuses without the key");
  failures += check(level.tryOpenDoor(3, 6, true, true, &message), "key opens the locked door");
  const std::vector<Vec2> nobody;
  for (int i = 0; i < 90; ++i) level.updateDoors(1.0f / 60.0f, nobody);
  failures += check(level.doorOpen(3, 6) > 0.95f, "door slides fully open");
  failures += check(!level.isSolid(3, 6), "open door is walkable");
  for (int i = 0; i < 600; ++i) level.updateDoors(1.0f / 60.0f, nobody);
  failures += check(level.isSolid(3, 6), "door closes again when nobody is in it");
  const std::vector<Vec2> inDoorway = {Level::cellCenter(3, 6)};
  for (int i = 0; i < 120; ++i) level.updateDoors(1.0f / 60.0f, inDoorway);
  failures += check(level.doorOpen(3, 6) > 0.5f, "standing in a door reopens it");

  // --- line of sight and raycasts --------------------------------------
  Level sight;
  sight.loadAscii("SIGHT", {"#####", "#...#", "###.#", "#...#", "#####"});
  failures += check(sight.lineOfSight(Vec2(1.5f, 1.5f), Vec2(3.5f, 1.5f)),
                    "open corridor has line of sight");
  failures += check(!sight.lineOfSight(Vec2(1.5f, 2.5f), Vec2(3.5f, 2.5f)),
                    "a wall blocks line of sight");
  const RayHit hit = sight.rayCast(Vec2(1.5f, 2.5f), Vec2(1.0f, 0.0f), 10.0f);
  failures += check(hit.hit && hit.cx == 2 && std::abs(hit.dist - 0.5f) < 0.01f,
                    "raycast reports the exact wall distance");

  std::printf("  mechanics: %d failure(s)\n", failures);
  return failures;
}

int runSelfTest(const GameConfig& base, int frames, int levelIndex, bool arena, const char* shot) {
  if (runMechanicsTest() != 0) {
    std::printf("selftest FAILED (mechanics)\n");
    return 1;
  }

  GameConfig cfg = base;
  cfg.headless = true;
  cfg.muteAudio = true;
  cfg.renderW = 160;
  cfg.renderH = 100;

  Game game;
  if (!game.init(cfg)) {
    std::printf("selftest: init failed\n");
    return 1;
  }

  std::printf("\n=== SELFTEST: title screen ===\n");
  game.renderFrame();
  printAsciiFrame(game.framebuffer(), 116, "title screen (menu over live 3D scene)");

  if (arena) {
    game.startArena();
  } else {
    game.startCampaign(levelIndex >= 0 ? levelIndex : 0);
  }

  // Let the level intro banner finish so gameplay input is accepted.
  for (int i = 0; i < 200; ++i) game.tick(1.0f / 60.0f, InputState());
  std::printf("\n=== SELFTEST: scripted play (%s, %d ticks) ===\n",
              arena ? "arena" : "campaign", frames);

  InputState in;
  for (int i = 0; i < frames; ++i) {
    const int phase = i % 420;
    in.forward = phase < 330;
    in.strafeRight = phase >= 330 && phase < 360;
    in.strafeLeft = phase >= 360 && phase < 390;
    in.run = (i / 120) % 2 == 0;
    in.turn = (phase < 4) ? 0.035f : 0.0f;
    in.fire = (i % 22) < 3;
    in.use = (i % 150) < 2;
    in.selectWeapon = -1;
    if (i % 220 == 40) in.selectWeapon = 1;
    if (i % 220 == 120) in.selectWeapon = 0;

    game.tick(1.0f / 60.0f, in);

    // Auto-confirm any menu that pops up so the demo keeps running.
    const GameState state = game.state();
    if (state == GameState::LevelComplete || state == GameState::ArenaIntermission) {
      InputState confirm;
      confirm.menuConfirm = true;
      game.tick(1.0f / 60.0f, confirm);
    } else if (state == GameState::PlayerDead || state == GameState::Victory) {
      break;
    }

    if (i % 150 == 0) {
      const Player& p = game.world().player();
      std::printf(
          "  tick %4d  pos=(%5.2f,%5.2f) hp=%3d armor=%3d weapon=%d ammo=%2d kills=%2d alive=%2d "
          "state=%d\n",
          i, p.pos.x, p.pos.y, p.health, p.armor, p.weapon, p.ammo[0], p.kills,
          game.world().aliveEnemies(), int(game.state()));
      game.renderFrame();
      printAsciiFrame(game.framebuffer(), 116, fmt("tick %d gameplay + HUD", i).c_str());
    }
  }

  // --- combat check: aim at the nearest hostile and keep firing ----------------
  std::printf("\n=== SELFTEST: combat (aim + fire at the nearest hostile) ===\n");
  const int killsBefore = game.world().player().kills;
  for (int i = 0; i < 700; ++i) {
    const Player& p = game.world().player();
    if (p.dead()) break;
    // Re-target every tick: hostiles keep moving.
    bool found = false;
    Vec2 target;
    float best = 1e9f;
    for (const Enemy& e : game.world().enemies()) {
      if (e.dead()) continue;
      const float d = distance(e.pos, p.pos);
      if (d < best) {
        best = d;
        target = e.pos;
        found = true;
      }
    }
    InputState aim;
    if (found) {
      const float desired = std::atan2(target.y - p.pos.y, target.x - p.pos.x);
      aim.turn = angleDelta(p.angle, desired);
      aim.fire = true;
    }
    game.tick(1.0f / 60.0f, aim);
    if (game.world().player().kills > killsBefore + 1) break;
  }
  const int killsAfter = game.world().player().kills;
  std::printf("  kills %d -> %d, hostiles left %d, score %d\n", killsBefore, killsAfter,
              game.world().aliveEnemies(), game.score());
  if (killsAfter > killsBefore) {
    game.renderFrame();
    printAsciiFrame(game.framebuffer(), 116, "after a kill");
  } else {
    std::printf("  ! no hostile died during the combat check\n");
  }

  game.renderFrame();
  const Player& p = game.world().player();
  std::printf("\n=== SELFTEST RESULT ===\n");
  std::printf("  final state   : %d (7 = Victory, 4 = Playing, 5 = Dead)\n", int(game.state()));
  std::printf("  player pos    : (%.2f, %.2f) angle=%.2f pitch=%+.2f\n", p.pos.x, p.pos.y, p.angle,
              p.pitch);
  std::printf("  health/armor  : %d / %d\n", p.health, p.armor);
  std::printf("  kills / alive : %d / %d of %d spawned\n", p.kills, game.world().aliveEnemies(),
              game.world().enemyTotal());
  std::printf("  score         : %d\n", game.score());
  std::printf("  player inside : %s\n",
              game.level().inside(int(p.pos.x), int(p.pos.y)) ? "yes" : "NO");

  bool sane = game.level().inside(int(p.pos.x), int(p.pos.y));
  sane = sane && p.health >= 0 && p.health <= 100;
  sane = sane && p.armor >= 0;
  sane = sane && killsAfter > killsBefore;  // combat must actually damage hostiles

  if (shot && *shot) {
    if (writePng(shot, toTexture(game.framebuffer()))) {
      std::printf("  screenshot    : %s\n", shot);
    } else {
      std::printf("  screenshot    : failed\n");
      sane = false;
    }
  }

  game.shutdown();
  std::printf("selftest %s\n", sane ? "PASSED" : "FAILED");
  return sane ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
  GameConfig config;
  bool selftest = false;
  bool dumpTextures = false;
  bool startArena = false;
  int startLevel = -1;
  int frames = 600;
  const char* shot = nullptr;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto next = [&](const char* what) -> const char* {
      if (i + 1 >= argc) {
        std::printf("error: %s expects a value\n", what);
        return nullptr;
      }
      return argv[++i];
    };

    if (arg == "-h" || arg == "--help") {
      printUsage(argv[0]);
      return 0;
    } else if (arg == "-f" || arg == "--fullscreen") {
      config.fullscreen = true;
    } else if (arg == "--windowed") {
      config.fullscreen = false;
    } else if (arg == "--mute") {
      config.muteAudio = true;
    } else if (arg == "--quiet") {
      config.verbose = false;
    } else if (arg == "--invert-y") {
      config.invertY = true;
    } else if (arg == "--arena") {
      startArena = true;
    } else if (arg == "--selftest") {
      selftest = true;
    } else if (arg == "--dump-textures") {
      dumpTextures = true;
    } else if (arg == "--size") {
      const char* value = next("--size");
      if (!value) return 1;
      std::sscanf(value, "%dx%d", &config.windowW, &config.windowH);
    } else if (arg == "--render") {
      const char* value = next("--render");
      if (!value) return 1;
      std::sscanf(value, "%dx%d", &config.renderW, &config.renderH);
    } else if (arg == "--fov") {
      const char* value = next("--fov");
      if (!value) return 1;
      config.fov = float(std::atof(value));
    } else if (arg == "--sens") {
      const char* value = next("--sens");
      if (!value) return 1;
      config.mouseSensitivity = float(std::atof(value));
    } else if (arg == "--level") {
      const char* value = next("--level");
      if (!value) return 1;
      startLevel = std::atoi(value) - 1;
    } else if (arg == "--frames") {
      const char* value = next("--frames");
      if (!value) return 1;
      frames = std::atoi(value);
    } else if (arg == "--shot") {
      shot = next("--shot");
      if (!shot) return 1;
    } else {
      std::printf("unknown option: %s (try --help)\n", arg.c_str());
      return 1;
    }
  }

  if (dumpTextures) {
    SDL_Init(SDL_INIT_TIMER);
    Assets assets;
    assets.build(defaultTextureDirs(), true);
    assets.dumpDefaults("assets/textures");
    SDL_Quit();
    return 0;
  }

  if (selftest) return runSelfTest(config, frames, startLevel, startArena, shot);

  Game game;
  if (!game.init(config)) return 1;
  if (startArena) {
    game.startArena();
  } else if (startLevel >= 0) {
    game.startCampaign(startLevel);
  }
  game.run();
  game.shutdown();
  return 0;
}
