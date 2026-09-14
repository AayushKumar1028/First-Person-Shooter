// audio.h - procedurally synthesised sound effects (no audio files needed).
//
// Every effect is generated as a short waveform at startup, so the game has
// sound with zero assets. If no audio device is available the whole system
// quietly disables itself and the game keeps running.
#pragma once

#include <cstdint>
#include <vector>

#include "core.h"

namespace fps {

enum class Sfx : int {
  Pistol,
  Shotgun,
  Chaingun,
  Launcher,
  Plasma,
  Explosion,
  PlayerPain,
  PlayerDeath,
  EnemyPain,
  EnemyDeath,
  EnemyAlert,
  DoorOpen,
  DoorClose,
  PickupItem,
  PickupWeapon,
  NoWay,
  SwitchWeapon,
  Count
};

class Audio {
 public:
  bool init(bool verbose);
  void shutdown();

  void play(Sfx id, float volume = 1.0f, float pitch = 1.0f);
  // Stops everything (used when leaving a level).
  void stopAll();

  bool enabled() const { return device_ != 0; }
  int activeVoices() const;

  static const char* sfxName(Sfx id);

 private:
  struct Voice {
    int clip = -1;
    float pos = 0.0f;
    float gain = 0.0f;
    float step = 1.0f;
    bool active = false;
  };

  std::vector<std::vector<int16_t>> clips_;
  std::vector<Voice> voices_;
  uint32_t device_ = 0;
  int sampleRate_ = 22050;

  void mix(int16_t* out, int frames);
  static void callback(void* userdata, uint8_t* stream, int len);
};

}  // namespace fps
