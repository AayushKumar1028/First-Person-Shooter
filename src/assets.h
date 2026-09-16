// assets.h - every texture and sprite the game needs.
//
// The game ships with procedurally generated retro art, so it runs with an
// empty assets folder. Any slot can be replaced with your own art by dropping a
// file named after the slot into assets/textures/ (PNG, BMP or PPM, any size).
// Run the game with --dump-textures to write the built-ins out as editable files.
#pragma once

#include <string>
#include <vector>

#include "image.h"

namespace fps {

// ---------------------------------------------------------------------------
// Wall / floor / ceiling slots
// ---------------------------------------------------------------------------
enum class Tex : int {
  WallBrick,
  WallStone,
  WallMetal,
  WallBlue,
  WallWood,
  WallGreen,
  WallConcrete,
  WallBlood,
  WallCrate,
  WallSupport,
  WallFlag,
  WallExit,
  DoorMetal,
  DoorWood,
  DoorLocked,
  DoorExit,
  FloorStone,
  FloorCobble,
  FloorMetal,
  FloorTech,
  FloorDirt,
  FloorBlood,
  CeilStone,
  CeilRust,
  CeilDark,
  CeilTech,
  Count
};

// ---------------------------------------------------------------------------
// Sprite slots. Multi-frame slots are animations (see SpriteSet).
// ---------------------------------------------------------------------------
enum class SpriteId : int {
  ZombieIdle,
  ZombieWalk,
  ZombieAttack,
  ZombiePain,
  ZombieDeath,
  ImpIdle,
  ImpWalk,
  ImpAttack,
  ImpPain,
  ImpDeath,
  BruteIdle,
  BruteWalk,
  BruteAttack,
  BrutePain,
  BruteDeath,
  Fireball,
  Rocket,
  Explosion,
  Barrel,
  BarrelBroken,
  HealthSmall,
  HealthLarge,
  ArmorSmall,
  ArmorLarge,
  AmmoBullets,
  AmmoShells,
  AmmoRockets,
  AmmoCells,
  PickupShotgun,
  PickupChaingun,
  PickupLauncher,
  PickupPlasma,
  Lamp,
  TechPillar,
  Gore,
  // First-person weapon view models (canvas anchored bottom-centre).
  WeaponPistol,
  WeaponPistolFire,
  WeaponShotgun,
  WeaponShotgunFire,
  WeaponChaingun,
  WeaponChaingunFire,
  WeaponLauncher,
  WeaponLauncherFire,
  WeaponPlasma,
  WeaponPlasmaFire,
  WeaponKnife,
  WeaponKnifeFire,
  // Soft dot used for blood, sparks and debris (tinted per particle).
  Particle,
  Count
};

struct SpriteSet {
  std::string name;
  std::vector<Texture> frames;

  int count() const { return int(frames.size()); }
  const Texture& frame(int index) const {
    return frames[size_t(clampi(index, 0, count() - 1))];
  }
};

struct Assets {
  std::vector<Texture> walls;      // indexed by Tex
  std::vector<SpriteSet> sprites;  // indexed by SpriteId

  std::vector<std::string> textureDirs;
  std::vector<std::string> loadedFromDisk;

  // Fills every slot (procedural first, then disk overrides win).
  void build(const std::vector<std::string>& dirs, bool verbose);
  // Writes the procedural art to "dir" as editable template files.
  void dumpDefaults(const std::string& dir) const;

  const Texture& tex(Tex id) const { return walls[size_t(id)]; }
  const SpriteSet& anim(SpriteId id) const { return sprites[size_t(id)]; }
};

// Slot names - these are the filenames you use to override art.
const char* texName(Tex id);
const char* spriteName(SpriteId id);

}  // namespace fps
