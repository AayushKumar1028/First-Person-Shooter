// game.cpp - window, fixed-step main loop, state machine, campaign and arena.
#include "game.h"

#include <algorithm>
#include <fstream>

#include "font.h"

namespace fps {

namespace {

constexpr double kFixedStep = 1.0 / 60.0;

// Internal render resolutions - the "quality" setting, and F2 / F3.
const int kRenderPresets[6][2] = {{320, 200}, {384, 240}, {480, 300},
                                  {640, 400}, {800, 500}, {960, 600}};
const char* const kQualityNames[6] = {"LOW", "FAST", "MEDIUM", "HIGH", "ULTRA", "EXTREME"};

// Window sizes offered by the settings menu (GameConfig::resolution indexes it).
const int kWindowResolutions[4][2] = {{1024, 640}, {1280, 800}, {1600, 900}, {1920, 1080}};
const char* const kResolutionNames[4] = {"1024 X 640", "1280 X 800", "1600 X 900",
                                         "1920 X 1080"};

// Frame-rate caps; the last entry (0) means "as fast as the machine can go".
const int kFpsLimits[6] = {30, 60, 120, 144, 240, 0};
const char* const kFpsNames[6] = {"30", "60", "120", "144", "240", "UNLIMITED"};

constexpr int kSettingsItems = 5;

// Shrinks a window size so it fits the display's usable area. Never enlarges it,
// so an explicitly requested size is preserved when the screen is big enough.
void fitToDisplay(int& w, int& h) {
  SDL_Rect usable{0, 0, 0, 0};
  if (SDL_GetDisplayUsableBounds(0, &usable) != 0 || usable.w <= 0 || usable.h <= 0) return;
  const float scale = std::min(1.0f, std::min(float(usable.w) * 0.92f / float(w),
                                              float(usable.h) * 0.92f / float(h)));
  if (scale < 1.0f) {
    w = std::max(320, int(float(w) * scale));
    h = std::max(200, int(float(h) * scale));
  }
}

// Level 1's main hall - used for the slowly panning title screen camera.
constexpr float kTitleCentreX = 15.0f;
constexpr float kTitleCentreY = 9.0f;
constexpr float kTitleRadius = 4.0f;

Rng g_shakeRng(0x51A3E1u);

std::string upper(const std::string& text) {
  std::string out = text;
  for (char& c : out) {
    if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
  }
  return out;
}

}  // namespace

std::vector<std::string> defaultTextureDirs() {
  std::vector<std::string> dirs;
  auto add = [&dirs](const std::string& dir) {
    if (dir.empty()) return;
    // Skip anything we already cover: several of these resolve to the same
    // folder depending on where the game is launched from.
    for (const std::string& existing : dirs) {
      if (existing == dir) return;
    }
    dirs.push_back(dir);
  };

  add("assets/textures");
#ifdef FPS_ASSET_DIR
  add(std::string(FPS_ASSET_DIR) + "/textures");
#endif
  if (char* base = SDL_GetBasePath()) {
    add(std::string(base) + "assets/textures");
    SDL_free(base);
  }
  add("../assets/textures");
  return dirs;
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
bool Game::init(const GameConfig& config) {
  config_ = config;

  // Headless runs (tests/CI) must never need a display server.
  if (config_.headless) SDL_setenv("SDL_VIDEODRIVER", "dummy", 0);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::printf("[game] SDL_Init failed: %s\n", SDL_GetError());
    return false;
  }

  if (!config_.muteAudio && !config_.headless) {
    audio_.init(config_.verbose);
  }

  assets_.build(defaultTextureDirs(), config_.verbose);

  campaign_ = buildCampaign();
  bestScore_ = loadBestScore();

  framebuffer_.resize(config_.renderW, config_.renderH);
  renderer_.setAssets(&assets_);
  renderer_.resize(config_.renderW, config_.renderH);
  renderSettings_.fogDistance = 18.0f;
  renderSettings_.ambient = 0.22f;

  // Keep the settings indices in sync with whatever the config (or the CLI)
  // asked for, so the settings menu opens showing the live values.
  config_.quality = 2;
  for (int i = 0; i < 6; ++i) {
    if (kRenderPresets[i][0] == config_.renderW && kRenderPresets[i][1] == config_.renderH) {
      config_.quality = i;
      break;
    }
  }
  config_.resolution = 1;
  for (int i = 0; i < 4; ++i) {
    if (kWindowResolutions[i][0] == config_.windowW && kWindowResolutions[i][1] == config_.windowH) {
      config_.resolution = i;
      break;
    }
  }

  if (!config_.headless) createWindow();

  loadTitleScene();
  state_ = GameState::Title;
  menuIndex_ = 0;
  stateTimer_ = 0.0f;
  totalTime_ = 0.0f;
  return true;
}

void Game::createWindow() {
  // Fit the requested window to whatever display this machine actually has, so
  // it never opens larger than the screen. Starts windowed; F11 toggles.
  int winW = config_.windowW;
  int winH = config_.windowH;
  fitToDisplay(winW, winH);
  config_.windowW = winW;
  config_.windowH = winH;

  Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
  if (config_.fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
  window_ = SDL_CreateWindow("FPS SHOOTER - DOOMLIKE", SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, winW, winH, flags);
  if (!window_) {
    std::printf("[game] window creation failed: %s\n", SDL_GetError());
    return;
  }
  renderer = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) renderer = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
  if (renderer) {
    texture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                 framebuffer_.w, framebuffer_.h);
    SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_NONE);
  }
}

void Game::setRenderResolution(int w, int h) {
  if (w < 160 || h < 100) return;
  framebuffer_.resize(w, h);
  renderer_.resize(w, h);
  if (renderer) {
    if (texture_) SDL_DestroyTexture(texture_);
    texture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                 framebuffer_.w, framebuffer_.h);
    SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_NONE);
  }
}

// ---------------------------------------------------------------------------
// Display settings (driven by the in-game settings menu and the F-keys)
// ---------------------------------------------------------------------------
void Game::applyQuality(int index) {
  config_.quality = clampi(index, 0, 5);
  setRenderResolution(kRenderPresets[config_.quality][0], kRenderPresets[config_.quality][1]);
}

void Game::applyResolution(int index) {
  config_.resolution = clampi(index, 0, 3);
  applyWindowSize(kWindowResolutions[config_.resolution][0],
                  kWindowResolutions[config_.resolution][1]);
}

void Game::applyFullscreen(bool fullscreen) {
  config_.fullscreen = fullscreen;
  if (window_) SDL_SetWindowFullscreen(window_, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

void Game::applyFpsLimit(int index) { config_.fpsLimit = kFpsLimits[clampi(index, 0, 5)]; }

void Game::applyWindowSize(int w, int h) {
  fitToDisplay(w, h);
  config_.windowW = w;
  config_.windowH = h;
  if (!window_ || config_.fullscreen) return;
  SDL_SetWindowSize(window_, w, h);
  SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

void Game::shutdown() {
  SDL_SetRelativeMouseMode(SDL_FALSE);
  audio_.shutdown();
  if (texture_) SDL_DestroyTexture(texture_);
  if (renderer) SDL_DestroyRenderer(renderer);
  if (window_) SDL_DestroyWindow(window_);
  texture_ = nullptr;
  renderer = nullptr;
  window_ = nullptr;
  SDL_Quit();
}

void Game::loadTitleScene() {
  arenaMode_ = false;
  const CampaignLevel& first = campaign_[0];
  level_.defaultFloor = first.floor;
  level_.defaultCeil = first.ceil;
  level_.loadAscii(first.name, first.rows);
  level_.playerAngle = first.playerAngle;
  world_.reset(&level_, &assets_, &audio_);
  world_.spawnFromLevel();
  score_ = 0;
  currentLevel_ = 0;
}

void Game::loadCampaignLevel(int index) {
  currentLevel_ = clampi(index, 0, int(campaign_.size()) - 1);
  const CampaignLevel& data = campaign_[size_t(currentLevel_)];
  level_.defaultFloor = data.floor;
  level_.defaultCeil = data.ceil;
  level_.loadAscii(data.name, data.rows);
  level_.playerAngle = data.playerAngle;
  world_.reset(&level_, &assets_, &audio_);
  world_.spawnFromLevel();
  world_.addMessage(upper(data.intro));
  arenaMode_ = false;
  score_ = 0;
  announcedClear_ = false;
  state_ = GameState::LevelIntro;
  stateTimer_ = 0.0f;
  setStatus(upper(data.intro));
  SDL_SetRelativeMouseMode(SDL_TRUE);
}

void Game::startCampaign(int levelIndex) {
  loadCampaignLevel(levelIndex);
}

void Game::startArena() {
  arenaMode_ = true;
  score_ = 0;
  arenaWave_ = 0;
  announcedClear_ = false;
  level_.defaultFloor = Tex::FloorTech;
  level_.defaultCeil = Tex::CeilTech;
  level_.buildArena(0x5EED, 3);
  world_.reset(&level_, &assets_, &audio_);
  startArenaWave(1);
}

void Game::restartArena() {
  arenaMode_ = false;
  startArena();
}

void Game::startArenaWave(int wave) {
  arenaWave_ = wave;
  world_.clearProjectiles();
  Rng rng(uint32_t(0x5EED + wave * 7919));

  const int count = 2 + wave;
  for (int i = 0; i < count; ++i) {
    EnemyType type = EnemyType::Zombie;
    const float roll = rng.unit();
    if (wave >= 4 && roll < clampf(0.12f + float(wave) * 0.02f, 0.0f, 0.4f)) {
      type = EnemyType::Brute;
    } else if (wave >= 2 && roll < 0.55f) {
      type = EnemyType::Imp;
    }
    // Not awake: arena hostiles hunt the player only once they spot them.
    const Vec2 pos = level_.randomOpenCell(rng, 7.0f, world_.player().pos);
    world_.addEnemy(type, pos, false);
  }

  // Between-wave supplies, weighted toward whatever the player is short of.
  const SpawnKind pool[6] = {SpawnKind::AmmoShells, SpawnKind::AmmoBullets,
                             SpawnKind::AmmoRockets, SpawnKind::AmmoCells,
                             SpawnKind::AmmoShells, SpawnKind::ArmorSmall};
  for (int i = 0; i < 3 + wave / 2; ++i) {
    const Vec2 pos = level_.randomOpenCell(rng, 3.0f, world_.player().pos);
    world_.addPickup(pool[rng.irange(0, 5)], pos, true);
  }
  const Vec2 health = level_.randomOpenCell(rng, 3.0f, world_.player().pos);
  world_.addPickup(wave % 3 == 0 ? SpawnKind::HealthLarge : SpawnKind::HealthSmall, health, true);

  // Weapon unlocks keep the arena ramping without a shop.
  if (wave == 1) {
    const Vec2 p = level_.randomOpenCell(rng, 3.0f, world_.player().pos);
    world_.addPickup(SpawnKind::Shotgun, p, true);
  } else if (wave == 3) {
    const Vec2 p = level_.randomOpenCell(rng, 3.0f, world_.player().pos);
    world_.addPickup(SpawnKind::Chaingun, p, true);
  } else if (wave == 5) {
    const Vec2 p = level_.randomOpenCell(rng, 3.0f, world_.player().pos);
    world_.addPickup(SpawnKind::Launcher, p, true);
  } else if (wave == 7) {
    const Vec2 p = level_.randomOpenCell(rng, 3.0f, world_.player().pos);
    world_.addPickup(SpawnKind::Plasma, p, true);
  }

  world_.addMessage(fmt("WAVE %d - %d HOSTILES", wave, count));
  state_ = GameState::Playing;
  stateTimer_ = 0.0f;
  SDL_SetRelativeMouseMode(SDL_TRUE);
}

void Game::openMenu(GameState menu, int itemCount) {
  menuReturnState_ = state_;
  state_ = menu;
  menuIndex_ = 0;
  menuTimer_ = 0.0f;
  SDL_SetRelativeMouseMode(SDL_FALSE);
  (void)itemCount;
}

// ---------------------------------------------------------------------------
// Score persistence
// ---------------------------------------------------------------------------
int Game::loadBestScore() const {
  std::ifstream in("arena_best.txt");
  int value = 0;
  if (in) in >> value;
  return value;
}

void Game::saveBestScore(int value) const {
  std::ofstream out("arena_best.txt");
  if (out) out << value << "\n";
}

void Game::setStatus(const std::string& text) { status_ = text; }

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------
void Game::run() {
  running_ = true;
  const Uint64 freq = SDL_GetPerformanceFrequency();
  Uint64 last = SDL_GetPerformanceCounter();
  double accumulator = 0.0;

  while (running_ && state_ != GameState::Quit) {
    const Uint64 now = SDL_GetPerformanceCounter();
    double frameTime = double(now - last) / double(freq);
    last = now;
    if (frameTime > 0.25) frameTime = 0.25;  // don't spiral after a stall

    handleEvents();
    accumulator += frameTime;

    int steps = 0;
    while (accumulator >= kFixedStep && steps < 6) {
      InputState input = pendingInput_;
      // Mouse deltas and menu edges apply to a single tick only.
      pendingInput_.turn = 0.0f;
      pendingInput_.menuUp = pendingInput_.menuDown = false;
      pendingInput_.menuLeft = pendingInput_.menuRight = false;
      pendingInput_.menuConfirm = pendingInput_.menuBack = false;
      pendingInput_.cycleWeapon = 0;
      pendingInput_.selectWeapon = -1;
      pendingInput_.reload = false;
      tick(float(kFixedStep), input);
      accumulator -= kFixedStep;
      ++steps;
      if (!running_ || state_ == GameState::Quit) break;
    }
    if (steps >= 6) accumulator = 0.0;

    renderFrame();
    present();

    // Optional frame-rate cap. vsync (when available) still caps to the display.
    if (config_.fpsLimit > 0) {
      const double target = 1.0 / double(config_.fpsLimit);
      const double elapsed = double(SDL_GetPerformanceCounter() - now) / double(freq);
      if (elapsed < target) SDL_Delay(Uint32((target - elapsed) * 1000.0));
    }
  }
}

void Game::tick(float dt, const InputState& input) {
  stateTimer_ += dt;
  totalTime_ += dt;

  switch (state_) {
    case GameState::Title:
      titleTime_ += dt;
      updateTitle(dt, input);
      break;
    case GameState::Playing:
      updatePlaying(dt, input);
      break;
    case GameState::LevelIntro:
      if (stateTimer_ > 2.6f) {
        state_ = GameState::Playing;
        stateTimer_ = 0.0f;
      }
      break;
    case GameState::ArenaIntermission:
      if (stateTimer_ > 3.0f) startArenaWave(arenaWave_ + 1);
      break;
    case GameState::Paused:
    case GameState::LevelComplete:
    case GameState::PlayerDead:
    case GameState::Victory:
    case GameState::Help:
      updateMenus(dt, input);
      break;
    case GameState::Settings:
      updateSettings(dt, input);
      break;
    case GameState::Quit:
      break;
  }

  if (world_.messageTimer > 0.0f && !world_.message.empty()) status_ = world_.message;
}

void Game::updatePlaying(float dt, const InputState& input) {
  if (input.menuBack) {
    openMenu(GameState::Paused, 4);
    return;
  }

  const bool wasClear = world_.aliveEnemies() == 0;
  world_.update(dt, input);
  if (world_.score > score_) score_ = world_.score;

  if (!wasClear && world_.aliveEnemies() == 0 && !announcedClear_) {
    announcedClear_ = true;
    if (arenaMode_) {
      world_.addMessage(fmt("WAVE %d CLEARED", arenaWave_));
    } else {
      world_.addMessage("SECTOR CLEAR - REACH THE EXIT");
    }
  }

  if (world_.player().dead()) {
    if (stateTimer_ > 1.4f) onPlayerDeath();
    return;
  }

  if (arenaMode_) {
    if (world_.aliveEnemies() == 0 && stateTimer_ > 1.2f) {
      score_ += 25 * arenaWave_;
      state_ = GameState::ArenaIntermission;
      stateTimer_ = 0.0f;
      announcedClear_ = false;
      SDL_SetRelativeMouseMode(SDL_FALSE);
    }
    return;
  }

  if (world_.levelComplete()) {
    state_ = GameState::LevelComplete;
    stateTimer_ = 0.0f;
    announcedClear_ = false;
    SDL_SetRelativeMouseMode(SDL_FALSE);
  }
}

void Game::onPlayerDeath() {
  state_ = GameState::PlayerDead;
  stateTimer_ = 0.0f;
  menuIndex_ = 0;
  SDL_SetRelativeMouseMode(SDL_FALSE);
  if (arenaMode_ && score_ > bestScore_) {
    bestScore_ = score_;
    saveBestScore(bestScore_);
  }
}

void Game::advanceCampaign() {
  if (currentLevel_ + 1 >= int(campaign_.size())) {
    state_ = GameState::Victory;
    menuIndex_ = 0;
    return;
  }
  const int carryScore = score_;
  loadCampaignLevel(currentLevel_ + 1);
  score_ = carryScore;
}

void Game::updateTitle(float dt, const InputState& input) {
  (void)dt;
  const int count = 5;
  if (input.menuUp) menuIndex_ = (menuIndex_ + count - 1) % count;
  if (input.menuDown) menuIndex_ = (menuIndex_ + 1) % count;

  if (!input.menuConfirm) return;
  switch (menuIndex_) {
    case 0:
      score_ = 0;
      loadCampaignLevel(0);
      setStatus("FIND THE EXIT");
      break;
    case 1:
      startArena();
      break;
    case 2:
      menuReturnState_ = GameState::Title;
      state_ = GameState::Help;
      menuIndex_ = 0;
      break;
    case 3:
      menuReturnState_ = GameState::Title;
      state_ = GameState::Settings;
      menuIndex_ = 0;
      break;
    default:
      state_ = GameState::Quit;
      running_ = false;
      break;
  }
}

void Game::updateMenus(float dt, const InputState& input) {
  (void)dt;
  const bool up = input.menuUp;
  const bool down = input.menuDown;
  const bool confirm = input.menuConfirm;

  switch (state_) {
    case GameState::Help: {
      if (confirm || input.menuBack) {
        state_ = menuReturnState_;
        menuIndex_ = 0;
        if (state_ == GameState::Title) {
          // stay on the title screen
        } else if (state_ == GameState::Paused) {
          SDL_SetRelativeMouseMode(SDL_FALSE);
        }
      }
      break;
    }

    case GameState::Paused: {
      const int count = 5;
      if (up) menuIndex_ = (menuIndex_ + count - 1) % count;
      if (down) menuIndex_ = (menuIndex_ + 1) % count;
      if (input.menuBack) {
        state_ = GameState::Playing;
        SDL_SetRelativeMouseMode(SDL_TRUE);
        break;
      }
      if (!confirm) break;
      if (menuIndex_ == 0) {
        state_ = GameState::Playing;
        SDL_SetRelativeMouseMode(SDL_TRUE);
      } else if (menuIndex_ == 1) {
        if (arenaMode_) {
          restartArena();
        } else {
          loadCampaignLevel(currentLevel_);
        }
      } else if (menuIndex_ == 2) {
        menuReturnState_ = GameState::Paused;
        state_ = GameState::Help;
        menuIndex_ = 0;
      } else if (menuIndex_ == 3) {
        menuReturnState_ = GameState::Paused;
        state_ = GameState::Settings;
        menuIndex_ = 0;
      } else {
        loadTitleScene();
        state_ = GameState::Title;
        menuIndex_ = 0;
      }
      break;
    }

    case GameState::LevelComplete: {
      if (confirm) advanceCampaign();
      break;
    }

    case GameState::PlayerDead: {
      const int count = arenaMode_ ? 2 : 2;
      if (up) menuIndex_ = (menuIndex_ + count - 1) % count;
      if (down) menuIndex_ = (menuIndex_ + 1) % count;
      if (!confirm) break;
      if (menuIndex_ == 0) {
        if (arenaMode_) {
          restartArena();
        } else {
          loadCampaignLevel(currentLevel_);
        }
      } else {
        loadTitleScene();
        state_ = GameState::Title;
        menuIndex_ = 0;
      }
      break;
    }

    case GameState::Victory: {
      const int count = 2;
      if (up) menuIndex_ = (menuIndex_ + count - 1) % count;
      if (down) menuIndex_ = (menuIndex_ + 1) % count;
      if (!confirm) break;
      if (menuIndex_ == 0) {
        score_ = 0;
        loadCampaignLevel(0);
      } else {
        loadTitleScene();
        state_ = GameState::Title;
        menuIndex_ = 0;
      }
      break;
    }

    case GameState::ArenaIntermission: {
      if (confirm) startArenaWave(arenaWave_ + 1);
      break;
    }

    default: break;
  }
}

// The settings menu: up/down picks a row, left/right changes the value.
void Game::updateSettings(float dt, const InputState& input) {
  (void)dt;
  if (input.menuUp) menuIndex_ = (menuIndex_ + kSettingsItems - 1) % kSettingsItems;
  if (input.menuDown) menuIndex_ = (menuIndex_ + 1) % kSettingsItems;

  auto fpsIndex = [&]() {
    for (int i = 0; i < 6; ++i) {
      if (kFpsLimits[i] == config_.fpsLimit) return i;
    }
    return 5;
  };

  const int step = (input.menuRight ? 1 : 0) - (input.menuLeft ? 1 : 0);
  if (step != 0) {
    switch (menuIndex_) {
      case 0: applyQuality((config_.quality + step + 6) % 6); break;
      case 1: applyResolution((config_.resolution + step + 4) % 4); break;
      case 2: applyFullscreen(!config_.fullscreen); break;
      case 3: applyFpsLimit((fpsIndex() + step + 6) % 6); break;
      default: break;
    }
  }

  if (input.menuBack || (input.menuConfirm && menuIndex_ == 4)) {
    state_ = menuReturnState_;
    menuIndex_ = 0;
  } else if (input.menuConfirm && menuIndex_ == 2) {
    applyFullscreen(!config_.fullscreen);
  }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
void Game::handleEvents() {
  InputState& in = pendingInput_;
  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    switch (ev.type) {
      case SDL_QUIT:
        running_ = false;
        state_ = GameState::Quit;
        break;

      case SDL_KEYDOWN: {
        keys_[ev.key.keysym.scancode] = true;
        if (ev.key.repeat) break;
        const SDL_Keycode sym = ev.key.keysym.sym;
        const SDL_Scancode code = ev.key.keysym.scancode;
        if (sym == SDLK_ESCAPE) in.menuBack = true;
        else if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == SDLK_SPACE) in.menuConfirm = true;
        // Arrows and W/S both step through menus (edge triggered, no repeats).
        else if (sym == SDLK_UP || code == SDL_SCANCODE_W) in.menuUp = true;
        else if (sym == SDLK_DOWN || code == SDL_SCANCODE_S) in.menuDown = true;
        else if (sym == SDLK_LEFT || code == SDL_SCANCODE_A) in.menuLeft = true;
        else if (sym == SDLK_RIGHT || code == SDL_SCANCODE_D) in.menuRight = true;
        else if (code == SDL_SCANCODE_Z) in.cycleWeapon += 1;
        else if (code == SDL_SCANCODE_R) in.reload = true;
        else if (sym >= SDLK_1 && sym <= SDLK_6) in.selectWeapon = int(sym - SDLK_1);
        else if (sym == SDLK_F11 && window_) {
          applyFullscreen(!config_.fullscreen);
        } else if (sym == SDLK_F2) {
          applyQuality(config_.quality - 1);
          setStatus(fmt("RENDER %dx%d", framebuffer_.w, framebuffer_.h));
        } else if (sym == SDLK_F3) {
          applyQuality(config_.quality + 1);
          setStatus(fmt("RENDER %dx%d", framebuffer_.w, framebuffer_.h));
        }
        break;
      }

      case SDL_KEYUP:
        keys_[ev.key.keysym.scancode] = false;
        break;

      case SDL_MOUSEMOTION:
        // Horizontal look only: aiming is level, so the mouse Y axis is ignored.
        if (SDL_GetRelativeMouseMode()) {
          in.turn += float(ev.motion.xrel) * config_.mouseSensitivity;
        }
        break;

      case SDL_MOUSEBUTTONDOWN:
        if (ev.button.button == SDL_BUTTON_LEFT) {
          mouseFire_ = true;
          in.menuConfirm = true;  // clicking also confirms menus
        }
        break;

      case SDL_MOUSEBUTTONUP:
        if (ev.button.button == SDL_BUTTON_LEFT) mouseFire_ = false;
        break;

      case SDL_MOUSEWHEEL:
        in.cycleWeapon += ev.wheel.y;
        break;

      default:
        break;
    }
  }

  // Held keys drive continuous actions. Assign rather than OR, so releasing a
  // key actually clears the flag (ORing latched strafe/fire/turn on forever).
  in.forward = keys_[SDL_SCANCODE_W] || keys_[SDL_SCANCODE_UP];
  in.back = keys_[SDL_SCANCODE_S] || keys_[SDL_SCANCODE_DOWN];
  in.turnLeft = keys_[SDL_SCANCODE_A] || keys_[SDL_SCANCODE_LEFT];
  in.turnRight = keys_[SDL_SCANCODE_D] || keys_[SDL_SCANCODE_RIGHT];
  in.strafeLeft = keys_[SDL_SCANCODE_Q];
  in.strafeRight = keys_[SDL_SCANCODE_E];
  in.run = keys_[SDL_SCANCODE_LSHIFT] || keys_[SDL_SCANCODE_RSHIFT];
  // Space shoots and interacts; the mouse and Ctrl also fire.
  in.use = keys_[SDL_SCANCODE_SPACE];
  in.fire = mouseFire_ || keys_[SDL_SCANCODE_SPACE] || keys_[SDL_SCANCODE_LCTRL] ||
            keys_[SDL_SCANCODE_RCTRL];
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
void Game::drawTitleCamera(Framebuffer& fb) {
  Camera cam;
  const float a = titleTime_ * 0.16f;
  cam.pos = Vec2(kTitleCentreX + std::cos(a) * kTitleRadius,
                 kTitleCentreY + std::sin(a) * kTitleRadius);
  cam.angle = a + kPi * 0.5f + 0.35f;
  cam.pitch = -0.04f;
  cam.height = 0.5f;
  cam.fov = config_.fov;

  std::vector<SpriteDraw> sprites;
  world_.gatherSprites(sprites);
  renderer_.renderWorld(fb, level_, cam, sprites, renderSettings_);
}

void Game::renderFrame() {
  Framebuffer& fb = framebuffer_;

  switch (state_) {
    case GameState::Title: {
      drawTitleCamera(fb);
      const std::vector<std::string> items = {"NEW GAME", "ARENA MODE", "HOW TO PLAY",
                                              "SETTINGS", "QUIT"};
      drawMenu(fb, "FPS SHOOTER", "A DOOM-STYLE RAYCASTER IN C++", items, menuIndex_,
               {"ARROWS / WASD MOVE + TURN - MOUSE LOOK",
                "SPACE FIRE + USE - Z WEAPONS - ESC MENU - F11 FULLSCREEN",
                bestScore_ > 0 ? ("ARENA BEST " + std::to_string(bestScore_)) : ""});
      break;
    }

    case GameState::Help:
    case GameState::Settings: {
      if (menuReturnState_ == GameState::Title) drawTitleCamera(fb);
      break;
    }

    case GameState::Playing:
    case GameState::LevelIntro:
    case GameState::Paused:
    case GameState::LevelComplete:
    case GameState::PlayerDead:
    case GameState::ArenaIntermission:
    case GameState::Victory: {
      Camera cam;
      const Player& p = world_.player();
      cam.pos = p.pos;
      cam.angle = p.angle;
      cam.height = p.height;
      cam.fov = config_.fov;
      if (p.shake > 0.001f) {
        cam.pos.x += g_shakeRng.range(-p.shake, p.shake);
        cam.pos.y += g_shakeRng.range(-p.shake, p.shake);
        cam.pitch += g_shakeRng.range(-p.shake, p.shake) * 0.6f;
      }

      std::vector<SpriteDraw> sprites;
      world_.gatherSprites(sprites);
      renderer_.renderWorld(fb, level_, cam, sprites, renderSettings_);

      const bool showWeapon = (state_ == GameState::Playing || state_ == GameState::Paused ||
                               state_ == GameState::LevelIntro);
      if (showWeapon && !p.dead()) {
        drawViewModel(fb, *world_.viewModelTexture(), p);
        const float spread = p.muzzleFlash * 6.0f + (p.bobAmount > 0.4f ? 2.0f : 0.0f);
        drawCrosshair(fb, spread);
      }
      break;
    }

    default:
      break;
  }

  // HUD and overlays for the in-game states.
  const bool inGame = state_ == GameState::Playing || state_ == GameState::LevelIntro ||
                      state_ == GameState::Paused || state_ == GameState::LevelComplete ||
                      state_ == GameState::PlayerDead ||
                      state_ == GameState::ArenaIntermission || state_ == GameState::Victory;

  if (inGame) {
    HudInfo info;
    info.player = &world_.player();
    info.level = &level_;
    info.levelIndex = currentLevel_;
    info.levelCount = int(campaign_.size());
    info.wave = arenaWave_;
    info.score = score_;
    info.bestScore = bestScore_;
    info.arena = arenaMode_;
    info.hostilesLeft = world_.aliveEnemies();
    info.elapsed = totalTime_;
    drawHud(fb, assets_, info);

    drawScreenEffects(fb, world_.player());
    if (world_.messageTimer > 0.0f) {
      drawMessage(fb, world_.message, clampf(world_.messageTimer, 0.0f, 1.0f));
    }
  }

  // Overlays: in-game banners and menus, plus the Help and Settings screens
  // (which darken whatever is behind them and draw their own menu panel).
  if (inGame || state_ == GameState::Help || state_ == GameState::Settings) {
    drawOverlays();
  }
}

void Game::drawOverlays() {
  Framebuffer& fb = framebuffer_;
  const Player& p = world_.player();

  switch (state_) {
    case GameState::LevelIntro:
      drawBigCenterText(fb, level_.name, "FIND THE EXIT - CLEAR THE SECTOR", ui::kAccent);
      break;

    case GameState::LevelComplete:
      drawBigCenterText(fb, "SECTOR CLEAR", fmt("KILLS %d   SCORE %d", p.kills, score_),
                        ui::kHealth);
      fontDrawCentered(fb, 0, fb.h / 2 + 34, fb.w, "PRESS FIRE OR ENTER TO CONTINUE", ui::kText, 1, 1);
      break;

    case GameState::PlayerDead: {
      drawBigCenterText(fb, "YOU DIED", arenaMode_ ? fmt("WAVE %d   SCORE %d", arenaWave_, score_)
                                                   : fmt("KILLS %d", p.kills),
                        ui::kDanger);
      const std::vector<std::string> items = {arenaMode_ ? "NEW RUN" : "RETRY LEVEL",
                                              "QUIT TO MENU"};
      drawMenu(fb, "", "", items, menuIndex_, {});
      break;
    }

    case GameState::Victory: {
      drawBigCenterText(fb, "YOU ESCAPED", fmt("TOTAL SCORE %d   TIME %d:%02d", score_,
                                               int(totalTime_) / 60, int(totalTime_) % 60),
                        ui::kAccent);
      const std::vector<std::string> items = {"PLAY AGAIN", "QUIT TO MENU"};
      drawMenu(fb, "", "", items, menuIndex_, {});
      break;
    }

    case GameState::ArenaIntermission: {
      drawBigCenterText(fb, fmt("WAVE %d CLEARED", arenaWave_),
                        fmt("SCORE %d - NEXT WAVE IN %d", score_,
                            int(3.0f - stateTimer_) + 1),
                        ui::kAccent);
      break;
    }

    case GameState::Paused: {
      const std::vector<std::string> items = {"RESUME", arenaMode_ ? "RESTART WAVE" : "RESTART LEVEL",
                                              "HOW TO PLAY", "SETTINGS", "QUIT TO MENU"};
      drawMenu(fb, "PAUSED", arenaMode_ ? fmt("ARENA WAVE %d", arenaWave_)
                                        : level_.name,
               items, menuIndex_, {});
      break;
    }

    case GameState::Settings: {
      int fpsIndex = 5;
      for (int i = 0; i < 6; ++i) {
        if (kFpsLimits[i] == config_.fpsLimit) fpsIndex = i;
      }
      const std::vector<std::string> items = {
          fmt("QUALITY       %s", kQualityNames[config_.quality]),
          fmt("RESOLUTION    %s", kResolutionNames[config_.resolution]),
          fmt("DISPLAY       %s", config_.fullscreen ? "FULLSCREEN" : "WINDOWED"),
          fmt("FPS LIMIT     %s", kFpsNames[fpsIndex]),
          "BACK",
      };
      const std::vector<std::string> footers = {
          fmt("WINDOW %d X %d    RENDER %d X %d", config_.windowW, config_.windowH,
              framebuffer_.w, framebuffer_.h),
          "UP / DOWN SELECT    LEFT / RIGHT CHANGE    ESC BACK",
      };
      drawMenu(fb, "SETTINGS", "", items, menuIndex_, footers);
      break;
    }

    case GameState::Help: {
      const std::vector<std::string> lines = {
          "MOVE          W S OR UP DOWN - FORWARD / BACK",
          "TURN          A D OR LEFT RIGHT",
          "STRAFE        Q / E",
          "LOOK          MOUSE (HORIZONTAL ONLY)",
          "RUN           LEFT SHIFT",
          "SHOOT / USE   SPACE  (MOUSE 1 OR CTRL ALSO FIRE)",
          "RELOAD        R     (AN EMPTY MAGAZINE RELOADS AUTOMATICALLY)",
          "WEAPONS       Z CYCLES   1 2 3 4 5 6   OR MOUSE WHEEL",
          "KNIFE         ALWAYS READY - PRESS 6 OR Z",
          "PAUSE / MENU  ESC TOGGLES",
          "DISPLAY       F11 FULLSCREEN   F2 / F3 QUALITY   ESC SETTINGS",
          "",
          "GOAL          CLEAR EVERY HOSTILE, THEN REACH THE EXIT",
          "              RED KEYCARDS OPEN THE LOCKED DOORS",
          "              BARRACKS DRUMS EXPLODE - MIND THE SPLASH",
          "",
          "CUSTOM ART    DROP PNG/BMP/PPM FILES NAMED LIKE THE SLOTS",
          "              INTO assets/textures/ AND RESTART",
          "",
          "PRESS FIRE, ENTER OR ESC TO GO BACK"};
      drawMenu(fb, "HOW TO PLAY", "", {}, -1, lines);
      break;
    }

    default:
      break;
  }
}

void Game::present() {
  if (!renderer || !texture_) return;
  SDL_UpdateTexture(texture_, nullptr, framebuffer_.px.data(), framebuffer_.w * 4);
  SDL_RenderClear(renderer);

  int outW = 0, outH = 0;
  SDL_GetRendererOutputSize(renderer, &outW, &outH);
  if (outW <= 0 || outH <= 0) return;

  // Letterbox the low-res framebuffer into the window, nearest-neighbour.
  const float scale = std::min(float(outW) / float(framebuffer_.w),
                               float(outH) / float(framebuffer_.h));
  const int dstW = std::max(1, int(float(framebuffer_.w) * scale));
  const int dstH = std::max(1, int(float(framebuffer_.h) * scale));
  const SDL_Rect dst{(outW - dstW) / 2, (outH - dstH) / 2, dstW, dstH};
  SDL_RenderCopy(renderer, texture_, nullptr, &dst);
  SDL_RenderPresent(renderer);
}

}  // namespace fps
