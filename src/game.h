// game.h - window, main loop, state machine, campaign and arena modes.
#pragma once

#include <SDL.h>

#include <string>
#include <vector>

#include "assets.h"
#include "audio.h"
#include "entities.h"
#include "framebuffer.h"
#include "hud.h"
#include "level.h"
#include "levels.h"
#include "render.h"

namespace fps {

struct GameConfig {
  int windowW = 1280;
  int windowH = 800;
  int renderW = 480;  // internal render resolution (scaled up to the window)
  int renderH = 300;
  bool fullscreen = false;
  bool headless = false;  // no window/audio: used by --selftest
  bool muteAudio = false;
  float mouseSensitivity = 0.0024f;
  bool invertY = false;
  float fov = 74.0f;
  bool verbose = true;
};

enum class GameState {
  Title,
  Playing,
  Paused,
  LevelIntro,
  LevelComplete,
  PlayerDead,
  ArenaIntermission,
  Victory,
  Help,
  Quit
};

class Game {
 public:
  bool init(const GameConfig& config);
  void shutdown();
  void run();

  // --- driving the game without a window (used by --selftest) -------------
  void startCampaign(int levelIndex);
  void startArena();
  void feedInput(const InputState& input) { pendingInput_ = input; }
  void tick(float dt, const InputState& input);
  void renderFrame();
  void present();
  bool shouldQuit() const { return state_ == GameState::Quit; }

  // --- inspection --------------------------------------------------------
  GameState state() const { return state_; }
  const Framebuffer& framebuffer() const { return framebuffer_; }
  const Assets& assets() const { return assets_; }
  const World& world() const { return world_; }
  const Level& level() const { return level_; }
  int score() const { return score_; }
  int arenaWave() const { return arenaWave_; }
  bool arenaMode() const { return arenaMode_; }
  const std::string& status() const { return status_; }
  SDL_Window* window() const { return window_; }
  void setState(GameState state) { state_ = state; }
  void setRenderResolution(int w, int h);

 private:
  GameConfig config_;
  Assets assets_;
  Audio audio_;
  Renderer renderer_;
  Framebuffer framebuffer_;
  World world_;
  Level level_;
  std::vector<CampaignLevel> campaign_;

  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer = nullptr;  // SDL's 2D renderer (blits the framebuffer)
  SDL_Texture* texture_ = nullptr;

  GameState state_ = GameState::Title;
  GameState menuReturnState_ = GameState::Title;
  int menuIndex_ = 0;
  int renderPreset_ = 2;
  bool announcedClear_ = false;
  float menuTimer_ = 0.0f;
  float stateTimer_ = 0.0f;
  float titleTime_ = 0.0f;
  int currentLevel_ = 0;
  int arenaWave_ = 0;
  int score_ = 0;
  int bestScore_ = 0;
  bool arenaMode_ = false;
  float totalTime_ = 0.0f;
  std::string status_;
  RenderSettings renderSettings_;

  InputState pendingInput_;
  bool keys_[SDL_NUM_SCANCODES] = {false};
  bool mouseFire_ = false;
  bool running_ = false;

  // --- setup -------------------------------------------------------------
  void createWindow();
  void loadTitleScene();
  void loadCampaignLevel(int index);
  void startArenaWave(int wave);
  void restartArena();
  void openMenu(GameState menu, int itemCount);

  // --- per-frame ---------------------------------------------------------
  void handleEvents();
  void updateTitle(float dt, const InputState& input);
  void updatePlaying(float dt, const InputState& input);
  void updateMenus(float dt, const InputState& input);
  void advanceCampaign();
  void onPlayerDeath();
  void drawOverlays();
  void drawTitleCamera(Framebuffer& fb);

  // --- misc --------------------------------------------------------------
  void setStatus(const std::string& text);
  int loadBestScore() const;
  void saveBestScore(int value) const;
};

// Folders searched for custom textures, in priority order.
std::vector<std::string> defaultTextureDirs();

}  // namespace fps
