// audio.cpp - tiny synthesiser + software mixer feeding an SDL audio device.
#include "audio.h"

#include <SDL.h>

#include <cstdio>
#include <cstring>

namespace fps {

namespace {

constexpr int kMaxVoices = 24;

// One layer of a synthesised sound: either filtered noise or a swept sine.
struct Component {
  bool noise = false;
  float freq0 = 440.0f;  // start frequency / noise cutoff
  float freq1 = 440.0f;  // end frequency
  float gain = 0.5f;
  float decay = 12.0f;  // exponential decay rate (1/sec)
  float attack = 0.004f;
};

std::vector<int16_t> synth(float duration, const std::vector<Component>& components, uint32_t seed,
                           int sampleRate) {
  const int count = int(duration * float(sampleRate));
  std::vector<float> buffer(size_t(count), 0.0f);
  Rng rng(seed);

  for (const Component& c : components) {
    float phase = 0.0f;
    float noiseState = 0.0f;
    float freq = c.freq0;
    const float freqStep = (c.freq1 - c.freq0) / float(std::max(1, count));
    for (int i = 0; i < count; ++i) {
      const float t = float(i) / float(sampleRate);
      // Attack/release envelope.
      float env = std::exp(-c.decay * t);
      if (t < c.attack) env *= t / c.attack;
      float value = 0.0f;
      if (c.noise) {
        // One-pole low pass over white noise gives us thumps and hisses.
        const float white = rng.range(-1.0f, 1.0f);
        const float alpha = clampf(std::abs(freq) / float(sampleRate) * 6.2832f, 0.002f, 0.95f);
        noiseState += alpha * (white - noiseState);
        value = noiseState * 3.0f;
      } else {
        phase += freq * 6.2831853f / float(sampleRate);
        if (phase > 6.2831853f * 1024.0f) phase -= 6.2831853f * 1024.0f;
        value = std::sin(phase);
      }
      buffer[size_t(i)] += value * env * c.gain;
      freq += freqStep;
    }
  }

  std::vector<int16_t> out(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    const float v = clampf(buffer[size_t(i)], -1.0f, 1.0f);
    out[size_t(i)] = int16_t(v * 32000.0f);
  }
  return out;
}

std::vector<std::vector<Component>> buildLibrary() {
  std::vector<std::vector<Component>> lib(int(Sfx::Count));
  auto set = [&](Sfx id, std::vector<Component> comps) { lib[size_t(id)] = std::move(comps); };

  set(Sfx::Pistol, {{true, 4200.0f, 700.0f, 0.55f, 26.0f}, {false, 240.0f, 70.0f, 0.30f, 30.0f}});
  set(Sfx::Shotgun,
      {{true, 2600.0f, 260.0f, 0.80f, 12.0f}, {false, 150.0f, 45.0f, 0.45f, 13.0f}});
  set(Sfx::Chaingun,
      {{true, 3800.0f, 900.0f, 0.45f, 34.0f}, {false, 280.0f, 120.0f, 0.22f, 40.0f}});
  set(Sfx::Launcher,
      {{true, 1000.0f, 180.0f, 0.70f, 9.0f}, {false, 190.0f, 40.0f, 0.50f, 10.0f}});
  set(Sfx::Plasma,
      {{false, 900.0f, 1700.0f, 0.32f, 9.0f}, {false, 1500.0f, 320.0f, 0.22f, 20.0f}});
  set(Sfx::Explosion,
      {{true, 1800.0f, 110.0f, 0.90f, 5.0f}, {false, 120.0f, 28.0f, 0.60f, 4.5f}});
  set(Sfx::PlayerPain,
      {{false, 340.0f, 170.0f, 0.45f, 11.0f}, {true, 1200.0f, 380.0f, 0.22f, 18.0f}});
  set(Sfx::PlayerDeath,
      {{false, 270.0f, 65.0f, 0.55f, 3.0f}, {true, 800.0f, 130.0f, 0.30f, 5.5f}});
  set(Sfx::EnemyPain,
      {{false, 430.0f, 230.0f, 0.38f, 13.0f}, {true, 1500.0f, 500.0f, 0.18f, 20.0f}});
  set(Sfx::EnemyDeath,
      {{false, 300.0f, 85.0f, 0.50f, 6.0f}, {true, 900.0f, 190.0f, 0.28f, 9.0f}});
  set(Sfx::EnemyAlert,
      {{false, 170.0f, 430.0f, 0.32f, 4.0f}, {true, 700.0f, 500.0f, 0.18f, 6.0f}});
  set(Sfx::DoorOpen,
      {{true, 420.0f, 950.0f, 0.26f, 2.6f}, {false, 90.0f, 150.0f, 0.18f, 2.0f}});
  set(Sfx::DoorClose,
      {{true, 750.0f, 240.0f, 0.30f, 4.0f}, {false, 150.0f, 65.0f, 0.22f, 3.0f}});
  set(Sfx::PickupItem, {{false, 700.0f, 1250.0f, 0.28f, 13.0f}});
  set(Sfx::PickupWeapon,
      {{false, 500.0f, 1000.0f, 0.32f, 7.0f}, {false, 760.0f, 1520.0f, 0.22f, 8.0f}});
  set(Sfx::NoWay, {{false, 200.0f, 150.0f, 0.28f, 15.0f}, {false, 100.0f, 75.0f, 0.20f, 15.0f}});
  set(Sfx::SwitchWeapon, {{true, 2200.0f, 900.0f, 0.22f, 32.0f}});
  // Knife: a short filtered-noise swish as the blade sweeps through the air.
  set(Sfx::Knife, {{true, 1600.0f, 300.0f, 0.30f, 16.0f}, {false, 420.0f, 180.0f, 0.16f, 12.0f}});
  return lib;
}

float durationFor(Sfx id) {
  switch (id) {
    case Sfx::Explosion: return 0.95f;
    case Sfx::PlayerDeath: return 1.2f;
    case Sfx::Launcher: return 0.55f;
    case Sfx::DoorOpen: return 0.75f;
    case Sfx::DoorClose: return 0.55f;
    case Sfx::EnemyDeath: return 0.75f;
    case Sfx::Shotgun: return 0.45f;
    case Sfx::EnemyAlert: return 0.5f;
    case Sfx::Knife: return 0.24f;
    default: return 0.35f;
  }
}

}  // namespace

const char* Audio::sfxName(Sfx id) {
  switch (id) {
    case Sfx::Pistol: return "pistol";
    case Sfx::Shotgun: return "shotgun";
    case Sfx::Chaingun: return "chaingun";
    case Sfx::Launcher: return "launcher";
    case Sfx::Plasma: return "plasma";
    case Sfx::Explosion: return "explosion";
    case Sfx::PlayerPain: return "player_pain";
    case Sfx::PlayerDeath: return "player_death";
    case Sfx::EnemyPain: return "enemy_pain";
    case Sfx::EnemyDeath: return "enemy_death";
    case Sfx::EnemyAlert: return "enemy_alert";
    case Sfx::DoorOpen: return "door_open";
    case Sfx::DoorClose: return "door_close";
    case Sfx::PickupItem: return "pickup_item";
    case Sfx::PickupWeapon: return "pickup_weapon";
    case Sfx::NoWay: return "no_way";
    case Sfx::SwitchWeapon: return "switch_weapon";
    case Sfx::Knife: return "knife";
    default: return "?";
  }
}

bool Audio::init(bool verbose) {
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    if (verbose) std::printf("[audio] disabled: %s\n", SDL_GetError());
    return false;
  }

  prepareClips();

  SDL_AudioSpec want{};
  want.freq = sampleRate_;
  want.format = AUDIO_S16SYS;
  want.channels = 1;
  want.samples = 512;
  want.callback = &Audio::callback;
  want.userdata = this;

  SDL_AudioSpec have{};
  device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (device_ == 0) {
    if (verbose) std::printf("[audio] no output device: %s\n", SDL_GetError());
    return false;
  }
  if (have.freq != sampleRate_) sampleRate_ = have.freq;

  voices_.assign(kMaxVoices, Voice());
  SDL_PauseAudioDevice(device_, 0);
  if (verbose) std::printf("[audio] %d effects synthesised at %d Hz\n", int(Sfx::Count), sampleRate_);
  return true;
}

void Audio::prepareClips() {
  if (clips_.size() == size_t(Sfx::Count)) return;  // already synthesised
  clips_.clear();
  clips_.resize(size_t(Sfx::Count));
  const std::vector<std::vector<Component>> lib = buildLibrary();
  for (int i = 0; i < int(Sfx::Count); ++i) {
    clips_[size_t(i)] = synth(durationFor(Sfx(i)), lib[size_t(i)], 0x5EED0000u + uint32_t(i) * 7919u,
                              sampleRate_);
  }
}

size_t Audio::clipBytes() const {
  size_t total = 0;
  for (const std::vector<int16_t>& clip : clips_) total += clip.size() * sizeof(int16_t);
  return total;
}

void Audio::shutdown() {
  if (device_ != 0) {
    SDL_CloseAudioDevice(device_);
    device_ = 0;
  }
  clips_.clear();
  voices_.clear();
}

void Audio::stopAll() {
  if (device_ == 0) return;
  SDL_LockAudioDevice(device_);
  for (Voice& v : voices_) v.active = false;
  SDL_UnlockAudioDevice(device_);
}

int Audio::activeVoices() const { return int(voices_.size()); }

void Audio::play(Sfx id, float volume, float pitch) {
  if (device_ == 0 || volume <= 0.0f) return;
  const int clip = int(id);
  if (clip < 0 || clip >= int(clips_.size()) || clips_[size_t(clip)].empty()) return;

  SDL_LockAudioDevice(device_);
  // Prefer a free slot, otherwise steal the quietest one.
  int slot = -1;
  float quietest = 1e9f;
  for (size_t i = 0; i < voices_.size(); ++i) {
    if (!voices_[i].active) {
      slot = int(i);
      break;
    }
    if (voices_[i].gain < quietest) {
      quietest = voices_[i].gain;
      slot = int(i);
    }
  }
  if (slot >= 0) {
    Voice& v = voices_[size_t(slot)];
    v.clip = clip;
    v.pos = 0.0f;
    v.gain = clampf(volume, 0.0f, 1.5f);
    v.step = clampf(pitch, 0.25f, 4.0f);
    v.active = true;
  }
  SDL_UnlockAudioDevice(device_);
}

void Audio::mix(int16_t* out, int frames) {
  std::memset(out, 0, size_t(frames) * sizeof(int16_t));
  for (Voice& v : voices_) {
    if (!v.active || v.clip < 0 || v.clip >= int(clips_.size())) {
      v.active = false;
      continue;
    }
    const std::vector<int16_t>& clip = clips_[size_t(v.clip)];
    for (int i = 0; i < frames; ++i) {
      const size_t index = size_t(v.pos);
      if (index >= clip.size()) {
        v.active = false;
        break;
      }
      const float sample = float(clip[index]) * v.gain;
      const int mixed = int(out[i]) + int(sample);
      out[i] = int16_t(clampi(mixed, -32767, 32767));
      v.pos += v.step;
    }
  }
}

void Audio::callback(void* userdata, uint8_t* stream, int len) {
  Audio* self = static_cast<Audio*>(userdata);
  if (!self) return;
  self->mix(reinterpret_cast<int16_t*>(stream), len / int(sizeof(int16_t)));
}

}  // namespace fps
