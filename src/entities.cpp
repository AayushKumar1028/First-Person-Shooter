// entities.cpp - weapons, enemy AI, combat, pickups, projectiles and effects.
#include "entities.h"

#include <algorithm>

namespace fps {

// ---------------------------------------------------------------------------
// Weapon definitions
// ---------------------------------------------------------------------------
namespace {

const WeaponDef kWeapons[int(WeaponId::Count)] = {
    // name            ammo             dmgMin dmgMax pellets spread cooldown auto  hitscan spd  splash ammo kick   range
    {"PISTOL", AmmoType::Bullets, 9, 15, 1, 1.2f, 0.30f, true, true, 0.0f, 0.0f, 1, 0.012f, 34.0f,
     SpriteId::WeaponPistol, SpriteId::WeaponPistolFire, SpriteId::AmmoBullets, Sfx::Pistol},

    {"SHOTGUN", AmmoType::Shells, 5, 10, 7, 7.5f, 0.76f, false, true, 0.0f, 0.0f, 1, 0.05f, 22.0f,
     SpriteId::WeaponShotgun, SpriteId::WeaponShotgunFire, SpriteId::PickupShotgun, Sfx::Shotgun},

    {"CHAINGUN", AmmoType::Bullets, 8, 14, 1, 3.0f, 0.104f, true, true, 0.0f, 0.0f, 1, 0.017f, 34.0f,
     SpriteId::WeaponChaingun, SpriteId::WeaponChaingunFire, SpriteId::PickupChaingun,
     Sfx::Chaingun},

    {"ROCKET LAUNCHER", AmmoType::Rockets, 70, 100, 1, 0.0f, 0.90f, false, false, 11.0f, 2.7f, 1,
     0.062f, 42.0f, SpriteId::WeaponLauncher, SpriteId::WeaponLauncherFire, SpriteId::PickupLauncher,
     Sfx::Launcher},

    {"PLASMA RIFLE", AmmoType::Cells, 14, 22, 1, 2.2f, 0.115f, true, false, 16.0f, 0.0f, 1, 0.015f,
     42.0f, SpriteId::WeaponPlasma, SpriteId::WeaponPlasmaFire, SpriteId::PickupPlasma, Sfx::Plasma},
};

const EnemyDef kEnemies[int(EnemyType::Count)] = {
    // name     hp  speed radius height melee  shot  attack    range melee windup cool sight  projSpeed score
    {"GUARD", 24, 1.55f, 0.30f, 0.82f, 3, 8, 4, 9, AttackKind::Hitscan, 13.0f, 1.1f, 0.36f, 1.35f,
     15.0f, 0.0f, 100, SpriteId::ZombieIdle, SpriteId::ZombieWalk, SpriteId::ZombieAttack,
     SpriteId::ZombiePain, SpriteId::ZombieDeath, Sfx::EnemyAlert, Sfx::EnemyPain,
     Sfx::EnemyDeath},

    {"IMP", 60, 1.95f, 0.32f, 0.88f, 6, 12, 0, 0, AttackKind::Fireball, 14.0f, 1.3f, 0.50f, 1.70f,
     17.0f, 7.5f, 250, SpriteId::ImpIdle, SpriteId::ImpWalk, SpriteId::ImpAttack, SpriteId::ImpPain,
     SpriteId::ImpDeath, Sfx::EnemyAlert, Sfx::EnemyPain, Sfx::EnemyDeath},

    {"BRUTE", 150, 1.25f, 0.42f, 1.00f, 14, 24, 0, 0, AttackKind::Fireball, 16.0f, 1.5f, 0.62f,
     2.10f, 19.0f, 9.5f, 500, SpriteId::BruteIdle, SpriteId::BruteWalk, SpriteId::BruteAttack,
     SpriteId::BrutePain, SpriteId::BruteDeath, Sfx::EnemyAlert, Sfx::EnemyPain,
     Sfx::EnemyDeath},
};

const int kMaxAmmo[int(AmmoType::Count)] = {200, 60, 40, 200};

// Frames per animation set (kept in sync with assets.cpp).
constexpr int kIdleFrames = 2;
constexpr int kWalkFrames = 4;
constexpr int kAttackFrames = 2;
constexpr int kDeathFrames = 5;

}  // namespace

const WeaponDef& weaponDef(WeaponId id) { return kWeapons[size_t(id)]; }
const WeaponDef& weaponDef(int index) {
  return kWeapons[size_t(clampi(index, 0, int(WeaponId::Count) - 1))];
}
int maxAmmo(AmmoType type) { return kMaxAmmo[size_t(type)]; }

const char* ammoName(AmmoType type) {
  switch (type) {
    case AmmoType::Bullets: return "BULLETS";
    case AmmoType::Shells: return "SHELLS";
    case AmmoType::Rockets: return "ROCKETS";
    case AmmoType::Cells: return "CELLS";
    default: return "AMMO";
  }
}

const EnemyDef& enemyDef(EnemyType type) { return kEnemies[size_t(type)]; }
const EnemyDef& enemyDef(int index) {
  return kEnemies[size_t(clampi(index, 0, int(EnemyType::Count) - 1))];
}

// ---------------------------------------------------------------------------
// World setup
// ---------------------------------------------------------------------------
void World::reset(Level* level, Assets* assets, Audio* audio) {
  level_ = level;
  assets_ = assets;
  audio_ = audio;

  enemies_.clear();
  pickups_.clear();
  props_.clear();
  projectiles_.clear();
  effects_.clear();
  particles_.clear();

  player_ = Player();
  player_.pos = level_->playerStart;
  player_.angle = level_->playerAngle;
  player_.health = 100;
  player_.armor = 0;
  player_.ammo[int(AmmoType::Bullets)] = 50;
  player_.hasWeapon[int(WeaponId::Pistol)] = true;
  player_.weapon = int(WeaponId::Pistol);

  message.clear();
  messageTimer = 0.0f;
  elapsed = 0.0f;
  score = 0;
  exitReached = false;
  prevFire_ = false;
  aliveSpawned_ = false;
  rng_ = Rng(0xBADC0DEu);
}

void World::addEnemy(EnemyType type, const Vec2& pos, bool awake) {
  Enemy e;
  e.type = type;
  e.def = &enemyDef(type);
  e.pos = pos;
  e.health = e.def->health;
  e.state = awake ? AIState::Chase : AIState::Idle;
  e.awake = awake;
  e.angle = 0.0f;
  e.animTime = rng_.range(0.0f, 1.0f);
  enemies_.push_back(e);
  aliveSpawned_ = true;
}

void World::addPickup(SpawnKind kind, const Vec2& pos, bool respawns) {
  Pickup p;
  p.kind = kind;
  p.pos = pos;
  p.respawns = respawns;
  p.bobPhase = rng_.range(0.0f, kTau);
  switch (kind) {
    case SpawnKind::HealthSmall: p.sprite = SpriteId::HealthSmall; break;
    case SpawnKind::HealthLarge: p.sprite = SpriteId::HealthLarge; break;
    case SpawnKind::ArmorSmall: p.sprite = SpriteId::ArmorSmall; break;
    case SpawnKind::ArmorLarge: p.sprite = SpriteId::ArmorLarge; break;
    case SpawnKind::AmmoBullets: p.sprite = SpriteId::AmmoBullets; break;
    case SpawnKind::AmmoShells: p.sprite = SpriteId::AmmoShells; break;
    case SpawnKind::AmmoRockets: p.sprite = SpriteId::AmmoRockets; break;
    case SpawnKind::AmmoCells: p.sprite = SpriteId::AmmoCells; break;
    case SpawnKind::Shotgun: p.sprite = SpriteId::PickupShotgun; break;
    case SpawnKind::Chaingun: p.sprite = SpriteId::PickupChaingun; break;
    case SpawnKind::Launcher: p.sprite = SpriteId::PickupLauncher; break;
    case SpawnKind::Plasma: p.sprite = SpriteId::PickupPlasma; break;
    case SpawnKind::KeyRed: p.sprite = SpriteId::ArmorSmall; break;  // key drawn as a small pickup
    default: p.sprite = SpriteId::HealthSmall; break;
  }
  // Small items sit lower so they read as floor clutter.
  p.height = 0.85f;
  pickups_.push_back(p);
}

void World::addProp(SpawnKind kind, const Vec2& pos) {
  Prop p;
  p.kind = kind;
  p.pos = pos;
  switch (kind) {
    case SpawnKind::Barrel:
      p.sprite = SpriteId::Barrel;
      p.explosive = true;
      p.health = 24;
      p.height = 0.85f;
      break;
    case SpawnKind::Lamp:
      p.sprite = SpriteId::Lamp;
      p.height = 0.95f;
      break;
    case SpawnKind::TechPillar:
      p.sprite = SpriteId::TechPillar;
      p.height = 1.0f;
      break;
    case SpawnKind::Gore:
      p.sprite = SpriteId::Gore;
      p.height = 0.45f;
      break;
    default: return;
  }
  props_.push_back(p);
}

void World::clearPickups() { pickups_.clear(); }
void World::clearProjectiles() { projectiles_.clear(); }

void World::spawnFromLevel() {
  for (const EntitySpawn& s : level_->spawns) {
    switch (s.kind) {
      case SpawnKind::Zombie: addEnemy(EnemyType::Zombie, s.pos); break;
      case SpawnKind::Imp: addEnemy(EnemyType::Imp, s.pos); break;
      case SpawnKind::Brute: addEnemy(EnemyType::Brute, s.pos); break;
      case SpawnKind::Barrel:
      case SpawnKind::Lamp:
      case SpawnKind::TechPillar:
      case SpawnKind::Gore: addProp(s.kind, s.pos); break;
      default: addPickup(s.kind, s.pos); break;
    }
  }
}

void World::addMessage(const std::string& text) {
  message = text;
  messageTimer = 3.0f;
}

void World::addEffect(const Vec2& pos, float z, SpriteId sprite, float duration, float height) {
  Effect e;
  e.pos = pos;
  e.z = z;
  e.sprite = sprite;
  e.duration = duration;
  e.height = height;
  effects_.push_back(e);
}

void World::addParticles(const Vec2& pos, float z, int count, RGBA color, float speed, float life) {
  constexpr size_t kMaxParticles = 160;
  for (int i = 0; i < count; ++i) {
    if (particles_.size() >= kMaxParticles) particles_.erase(particles_.begin());
    Particle p;
    p.pos = pos;
    p.z = clampf(z + rng_.range(-0.08f, 0.08f), 0.02f, 0.98f);
    const float a = rng_.range(0.0f, kTau);
    const float s = speed * rng_.range(0.35f, 1.0f);
    p.vel = Vec2(std::cos(a) * s, std::sin(a) * s);
    p.vz = rng_.range(0.1f, 1.4f);
    p.maxLife = life * rng_.range(0.6f, 1.2f);
    p.life = p.maxLife;
    p.size = rng_.range(0.05f, 0.13f);
    p.gravity = 3.2f;
    p.color = color;
    particles_.push_back(p);
  }
}

// ---------------------------------------------------------------------------
// Damage helpers
// ---------------------------------------------------------------------------
void World::damagePlayer(int amount, const char* source) {
  Player& p = player_;
  if (p.dead() || amount <= 0) return;

  int remaining = amount;
  if (p.armor > 0) {
    // Armour soaks a third of every hit until it runs out.
    const int absorbed = std::min(p.armor, int(std::ceil(float(amount) / 3.0f)));
    p.armor -= absorbed;
    remaining -= absorbed;
  }
  p.health -= remaining;
  p.damageFlash = 1.0f;
  p.shake = std::min(0.06f, p.shake + 0.02f + float(amount) * 0.0009f);

  if (source) {
    addMessage(std::string("HIT BY ") + source);
  }
  if (p.health <= 0) {
    p.health = 0;
    p.deathTime = 0.0f;
    if (audio_) audio_->play(Sfx::PlayerDeath, 0.9f);
    message.clear();
    messageTimer = 0.0f;
  } else if (audio_) {
    audio_->play(Sfx::PlayerPain, 0.55f, rng_.range(0.92f, 1.1f));
  }
}

void World::damageEnemy(int index, int amount, bool fromPlayer) {
  if (index < 0 || index >= int(enemies_.size())) return;
  Enemy& e = enemies_[index];
  if (e.dead() || amount <= 0) return;

  e.health -= amount;
  e.hitFlash = 1.0f;
  e.awake = true;
  if (e.state == AIState::Idle) e.state = AIState::Chase;

  if (e.health <= 0) {
    killEnemy(index, fromPlayer);
    return;
  }
  // Pain interrupts the wind-up often enough to reward sustained fire.
  if (rng_.chance(0.42f) && e.state != AIState::Pain) {
    e.state = AIState::Pain;
    e.stateTime = 0.0f;
    e.attackLanded = false;
  }
  if (audio_) audio_->play(Sfx::EnemyPain, 0.4f, rng_.range(0.9f, 1.15f));
}

void World::killEnemy(int index, bool fromPlayer) {
  Enemy& e = enemies_[index];
  e.state = AIState::Dead;
  e.stateTime = 0.0f;
  e.animTime = 0.0f;
  e.seesPlayer = false;
  if (fromPlayer) {
    player_.kills += 1;
    score += e.def->score;
  }

  if (audio_) audio_->play(Sfx::EnemyDeath, 0.6f, rng_.range(0.9f, 1.1f));
  addParticles(e.pos, 0.5f, 8, rgba(150, 20, 20), 1.1f, 0.9f);

  // Leave a permanent floor stain.
  Prop gore;
  gore.kind = SpawnKind::Gore;
  gore.sprite = SpriteId::Gore;
  gore.pos = e.pos;
  gore.height = 0.45f;
  props_.push_back(gore);
}

void World::explode(const Vec2& pos, float radius, int damage, bool fromPlayer) {
  if (radius <= 0.0f) return;
  addEffect(pos, 0.0f, SpriteId::Explosion, 0.45f, radius * 1.2f);

  if (audio_) {
    const float dist = distance(pos, player_.pos);
    const float vol = clampf(1.0f - dist / 30.0f, 0.1f, 1.0f);
    audio_->play(Sfx::Explosion, vol, rng_.range(0.9f, 1.08f));
  }
  addParticles(pos, 0.45f, 10, rgba(240, 150, 50), 2.3f, 0.5f);

  player_.shake = std::min(0.12f, player_.shake + 0.05f);

  // Enemies: linear falloff, with a floor so a near miss still hurts.
  for (int i = 0; i < int(enemies_.size()); ++i) {
    Enemy& e = enemies_[i];
    if (e.dead()) continue;
    const float d = distance(e.pos, pos);
    if (d > radius) continue;
    const int scaled = int(float(damage) * (1.0f - 0.65f * (d / radius)));
    damageEnemy(i, scaled, fromPlayer);
  }

  // Barrels chain-detonate.
  for (int i = 0; i < int(props_.size()); ++i) {
    Prop& p = props_[i];
    if (!p.explosive || p.destroyed) continue;
    if (distance(p.pos, pos) > radius) continue;
    damageProp(i, 400, fromPlayer);
  }

  const float dPlayer = distance(player_.pos, pos);
  if (dPlayer <= radius) {
    const int scaled = int(float(damage) * (1.0f - 0.65f * (dPlayer / radius)) * 0.55f);
    damagePlayer(std::max(1, scaled), "AN EXPLOSION");
  }
}

void World::damageProp(int index, int amount, bool fromPlayer) {
  if (index < 0 || index >= int(props_.size())) return;
  Prop& p = props_[index];
  if (p.destroyed) return;
  if (!p.explosive) {
    // Non-explosive props (lamps, gore) simply absorb hits.
    p.flash = 0.4f;
    return;
  }
  p.health -= amount;
  p.flash = 0.5f;
  if (p.health <= 0) {
    p.destroyed = true;
    p.sprite = SpriteId::BarrelBroken;
    p.height = 0.42f;
    // Chain reaction: whoever set this barrel off gets credit for the kills.
    explode(p.pos, 3.0f, 85, fromPlayer);
  } else if (audio_) {
    audio_->play(Sfx::EnemyPain, 0.25f, 0.7f);
  }
}

// ---------------------------------------------------------------------------
// Movement helpers
// ---------------------------------------------------------------------------
void World::moveWithCollision(Vec2& pos, const Vec2& delta, float radius) const {
  // Axis-by-axis so the actor slides along walls instead of sticking.
  Vec2 attempt(pos.x + delta.x, pos.y);
  if (!level_->collides(attempt, radius)) {
    pos.x = attempt.x;
  }
  attempt = Vec2(pos.x, pos.y + delta.y);
  if (!level_->collides(attempt, radius)) {
    pos.y = attempt.y;
  }
}

// ---------------------------------------------------------------------------
// Weapon fire
// ---------------------------------------------------------------------------
int World::findEnemyAlongRay(const Vec2& origin, const Vec2& dir, float maxDist) const {
  int best = -1;
  float bestT = maxDist;
  for (int i = 0; i < int(enemies_.size()); ++i) {
    const Enemy& e = enemies_[i];
    if (e.dead()) continue;
    const Vec2 to = e.pos - origin;
    const float t = dot(to, dir);
    if (t <= 0.0f || t > bestT) continue;
    const Vec2 closest = origin + dir * t;
    if (distance(closest, e.pos) > e.def->radius + 0.06f) continue;
    bestT = t;
    best = i;
  }
  return best;
}

int World::findPropAlongRay(const Vec2& origin, const Vec2& dir, float maxDist) const {
  int best = -1;
  float bestT = maxDist;
  for (int i = 0; i < int(props_.size()); ++i) {
    const Prop& p = props_[i];
    if (p.destroyed) continue;
    const Vec2 to = p.pos - origin;
    const float t = dot(to, dir);
    if (t <= 0.0f || t > bestT) continue;
    const Vec2 closest = origin + dir * t;
    if (distance(closest, p.pos) > 0.34f) continue;
    bestT = t;
    best = i;
  }
  return best;
}

void World::fireHitscan(const WeaponDef& weapon) {
  const Vec2 origin = player_.pos;
  for (int pellet = 0; pellet < weapon.pellets; ++pellet) {
    const float spread = weapon.spreadDeg * kDeg2Rad * rng_.range(-0.5f, 0.5f);
    const Vec2 dir = fromAngle(player_.angle + spread);
    const RayHit hit = level_->rayCast(origin, dir, weapon.range);
    const float maxDist = hit.hit ? hit.dist : weapon.range;

    const int enemyIndex = findEnemyAlongRay(origin, dir, maxDist);
    const int propIndex = enemyIndex < 0 ? findPropAlongRay(origin, dir, maxDist) : -1;

    if (enemyIndex >= 0) {
      damageEnemy(enemyIndex, rng_.irange(weapon.damageMin, weapon.damageMax), true);
      addParticles(enemies_[size_t(enemyIndex)].pos, 0.5f, 2, rgba(150, 20, 20), 0.6f, 0.35f);
    } else if (propIndex >= 0) {
      damageProp(propIndex, rng_.irange(weapon.damageMin, weapon.damageMax), true);
      addParticles(props_[size_t(propIndex)].pos, 0.4f, 2, rgba(200, 200, 200), 0.5f, 0.3f);
    } else if (hit.hit) {
      // Spark on the wall, matching the vertical aim so it looks like a hit.
      const Vec2 impact = origin + dir * std::max(0.05f, hit.dist - 0.06f);
      const float z = clampf(player_.height + std::tan(player_.pitch) * hit.dist, 0.05f, 0.95f);
      addParticles(impact, z, 3, rgba(220, 220, 210), 0.5f, 0.25f);
    }
  }
}

void World::fireProjectile(const WeaponDef& weapon) {
  Projectile p;
  p.pos = player_.pos + fromAngle(player_.angle) * 0.25f;
  const float spread = weapon.spreadDeg * kDeg2Rad * rng_.range(-0.5f, 0.5f);
  p.vel = fromAngle(player_.angle + spread) * weapon.projectileSpeed;
  p.z = player_.height - 0.06f;
  p.damage = rng_.irange(weapon.damageMin, weapon.damageMax);
  p.fromPlayer = true;
  p.explosive = weapon.explosionRadius > 0.0f;
  p.explosionRadius = weapon.explosionRadius;
  p.sprite = weapon.explosionRadius > 0.0f ? SpriteId::Rocket : SpriteId::Fireball;
  p.height = weapon.explosionRadius > 0.0f ? 0.3f : 0.24f;
  projectiles_.push_back(p);
}

void World::tryFire() {
  Player& p = player_;
  const WeaponDef& weapon = weaponDef(p.weapon);
  if (p.switchTimer > 0.0f || p.fireCooldown > 0.0f) return;

  if (p.ammo[int(weapon.ammo)] < weapon.ammoPerShot) {
    addMessage(std::string("OUT OF ") + ammoName(weapon.ammo));
    if (audio_) audio_->play(Sfx::NoWay, 0.5f);
    p.fireCooldown = 0.45f;
    return;
  }

  p.ammo[int(weapon.ammo)] -= weapon.ammoPerShot;
  p.fireCooldown = weapon.cooldown;
  p.muzzleFlash = 0.12f;

  if (weapon.hitscan) {
    fireHitscan(weapon);
  } else {
    fireProjectile(weapon);
  }
  if (audio_) audio_->play(weapon.sound, 0.55f, rng_.range(0.96f, 1.05f));
}

void World::playerUse() {
  const Vec2 dir = fromAngle(player_.angle);
  for (const float probe : {0.55f, 0.95f, 1.35f}) {
    const Vec2 point = player_.pos + dir * probe;
    int cx = 0, cy = 0;
    level_->cellOf(point, cx, cy);
    const int di = level_->doorIndexAt(cx, cy);
    if (di < 0) continue;

    const Door& door = level_->doors[size_t(di)];
    if (door.kind == DoorKind::Exit && door.locked) {
      addMessage(fmt("EXIT SEALED - %d HOSTILES REMAIN", aliveEnemies()));
      if (audio_) audio_->play(Sfx::NoWay, 0.6f);
      return;
    }
    std::string msg;
    const bool opened = level_->tryOpenDoor(cx, cy, true, player_.hasRedKey, &msg);
    if (!msg.empty()) addMessage(msg);
    if (opened) {
      if (audio_) audio_->play(Sfx::DoorOpen, 0.6f);
    } else if (audio_) {
      audio_->play(Sfx::NoWay, 0.5f);
    }
    return;
  }
}

// ---------------------------------------------------------------------------
// Pickups
// ---------------------------------------------------------------------------
void World::pickupAt(int index) {
  Pickup& item = pickups_[size_t(index)];
  Player& p = player_;
  bool taken = false;

  switch (item.kind) {
    case SpawnKind::HealthSmall:
      if (p.health < 100) {
        p.health = std::min(100, p.health + 10);
        addMessage("PICKED UP A STIMPACK");
        taken = true;
      }
      break;
    case SpawnKind::HealthLarge:
      if (p.health < 100) {
        p.health = std::min(100, p.health + 25);
        addMessage("PICKED UP A MEDKIT");
        taken = true;
      }
      break;
    case SpawnKind::ArmorSmall:
      if (p.armor < 100) {
        p.armor = std::min(100, p.armor + 35);
        addMessage("PICKED UP ARMOR");
        taken = true;
      }
      break;
    case SpawnKind::ArmorLarge:
      if (p.armor < 100) {
        p.armor = std::min(100, p.armor + 70);
        addMessage("PICKED UP COMBAT ARMOR");
        taken = true;
      }
      break;
    case SpawnKind::KeyRed:
      if (!p.hasRedKey) {
        p.hasRedKey = true;
        addMessage("PICKED UP THE RED KEYCARD");
        taken = true;
      }
      break;
    case SpawnKind::AmmoBullets:
      if (p.ammo[int(AmmoType::Bullets)] < maxAmmo(AmmoType::Bullets)) {
        p.ammo[int(AmmoType::Bullets)] =
            std::min(maxAmmo(AmmoType::Bullets), p.ammo[int(AmmoType::Bullets)] + 20);
        addMessage("PICKED UP A CLIP");
        taken = true;
      }
      break;
    case SpawnKind::AmmoShells:
      if (p.ammo[int(AmmoType::Shells)] < maxAmmo(AmmoType::Shells)) {
        p.ammo[int(AmmoType::Shells)] =
            std::min(maxAmmo(AmmoType::Shells), p.ammo[int(AmmoType::Shells)] + 8);
        addMessage("PICKED UP A SHELL BOX");
        taken = true;
      }
      break;
    case SpawnKind::AmmoRockets:
      if (p.ammo[int(AmmoType::Rockets)] < maxAmmo(AmmoType::Rockets)) {
        p.ammo[int(AmmoType::Rockets)] =
            std::min(maxAmmo(AmmoType::Rockets), p.ammo[int(AmmoType::Rockets)] + 5);
        addMessage("PICKED UP ROCKETS");
        taken = true;
      }
      break;
    case SpawnKind::AmmoCells:
      if (p.ammo[int(AmmoType::Cells)] < maxAmmo(AmmoType::Cells)) {
        p.ammo[int(AmmoType::Cells)] =
            std::min(maxAmmo(AmmoType::Cells), p.ammo[int(AmmoType::Cells)] + 40);
        addMessage("PICKED UP AN ENERGY CELL");
        taken = true;
      }
      break;
    case SpawnKind::Shotgun:
    case SpawnKind::Chaingun:
    case SpawnKind::Launcher:
    case SpawnKind::Plasma: {
      WeaponId id = WeaponId::Shotgun;
      if (item.kind == SpawnKind::Chaingun) id = WeaponId::Chaingun;
      else if (item.kind == SpawnKind::Launcher) id = WeaponId::Launcher;
      else if (item.kind == SpawnKind::Plasma) id = WeaponId::Plasma;
      const WeaponDef& def = weaponDef(id);
      if (!p.hasWeapon[int(id)]) {
        p.hasWeapon[int(id)] = true;
        p.weapon = int(id);
        p.switchTimer = 0.35f;
        addMessage(std::string("YOU GOT THE ") + def.name + "!");
        if (audio_) audio_->play(Sfx::PickupWeapon, 0.7f);
      } else {
        // Already owned: hand over ammo instead.
        const int amount = def.ammo == AmmoType::Shells ? 8 : (def.ammo == AmmoType::Rockets ? 5 : 20);
        p.ammo[int(def.ammo)] = std::min(maxAmmo(def.ammo), p.ammo[int(def.ammo)] + amount);
        addMessage(std::string("PICKED UP ") + ammoName(def.ammo));
      }
      taken = true;
      break;
    }
    default: break;
  }

  if (taken) {
    item.taken = true;
    player_.pickupFlash = 0.35f;
    if (audio_) audio_->play(Sfx::PickupItem, 0.5f, rng_.range(0.95f, 1.08f));
  }
}

// ---------------------------------------------------------------------------
// Per-frame update
// ---------------------------------------------------------------------------
void World::updatePlayer(float dt, const InputState& input) {
  Player& p = player_;

  if (p.dead()) {
    p.deathTime += dt;
    p.vel = Vec2(0.0f, 0.0f);
    // Slump to the floor.
    p.height = lerpf(p.height, 0.18f, 1.0f - std::exp(-dt * 4.0f));
    p.pitch = lerpf(p.pitch, -0.45f, 1.0f - std::exp(-dt * 3.0f));
    p.damageFlash = std::max(0.0f, p.damageFlash - dt * 0.4f);
    p.shake *= std::exp(-dt * 4.0f);
    return;
  }

  // Mouse look arrives as a delta; arrow keys turn at a fixed rate.
  const float keyTurn = (input.turnRight ? 1.0f : 0.0f) - (input.turnLeft ? 1.0f : 0.0f);
  const float keyLook = (input.lookUp ? 1.0f : 0.0f) - (input.lookDown ? 1.0f : 0.0f);
  p.angle += input.turn + keyTurn * 2.6f * dt;
  p.pitch = clampf(p.pitch + input.look + keyLook * 1.7f * dt, -0.62f, 0.62f);

  const Vec2 forward = fromAngle(p.angle);
  const Vec2 right(-forward.y, forward.x);
  Vec2 wish(0.0f, 0.0f);
  if (input.forward) wish += forward;
  if (input.back) wish -= forward;
  if (input.strafeRight) wish += right;
  if (input.strafeLeft) wish -= right;

  const float runMultiplier = input.run ? 1.55f : 1.0f;
  const float speed = 2.9f * runMultiplier;
  wish = normalize(wish) * speed;
  const float blend = 1.0f - std::exp(-dt * 15.0f);
  p.vel.x = lerpf(p.vel.x, wish.x, blend);
  p.vel.y = lerpf(p.vel.y, wish.y, blend);
  moveWithCollision(p.pos, Vec2(p.vel.x * dt, p.vel.y * dt), 0.24f);

  // Head bob drives both the camera and the weapon sway.
  const float moving = length(p.vel);
  p.bobPhase += dt * (1.2f + moving * 2.1f);
  p.bobAmount = lerpf(p.bobAmount, clampf(moving / 3.0f, 0.0f, 1.0f),
                      1.0f - std::exp(-dt * 5.0f));
  p.height = 0.5f + std::sin(p.bobPhase * 2.0f) * 0.021f * p.bobAmount;

  p.fireCooldown = std::max(0.0f, p.fireCooldown - dt);
  p.switchTimer = std::max(0.0f, p.switchTimer - dt);
  p.muzzleFlash = std::max(0.0f, p.muzzleFlash - dt * 4.0f);
  p.damageFlash = std::max(0.0f, p.damageFlash - dt * 1.6f);
  p.pickupFlash = std::max(0.0f, p.pickupFlash - dt * 2.4f);
  p.shake *= std::exp(-dt * 6.0f);

  // Weapon selection.
  if (input.selectWeapon >= 0 && input.selectWeapon < int(WeaponId::Count) &&
      p.hasWeapon[input.selectWeapon] && input.selectWeapon != p.weapon) {
    p.weapon = input.selectWeapon;
    p.switchTimer = 0.22f;
    if (audio_) audio_->play(Sfx::SwitchWeapon, 0.45f);
  } else if (input.cycleWeapon != 0) {
    const int dir = input.cycleWeapon > 0 ? 1 : -1;
    int next = p.weapon;
    for (int i = 0; i < int(WeaponId::Count); ++i) {
      next = (next + dir + int(WeaponId::Count)) % int(WeaponId::Count);
      if (p.hasWeapon[next]) break;
    }
    if (next != p.weapon) {
      p.weapon = next;
      p.switchTimer = 0.22f;
      if (audio_) audio_->play(Sfx::SwitchWeapon, 0.45f);
    }
  }

  // Firing.
  const WeaponDef& weapon = weaponDef(p.weapon);
  const bool wantsFire = input.fire && (weapon.autoFire || !prevFire_);
  if (wantsFire) tryFire();
  prevFire_ = input.fire;

  if (input.use) playerUse();
}

void World::updateEnemies(float dt) {
  Player& p = player_;

  for (int i = 0; i < int(enemies_.size()); ++i) {
    Enemy& e = enemies_[i];
    const EnemyDef& def = *e.def;
    e.animTime += dt;
    e.hitFlash = std::max(0.0f, e.hitFlash - dt * 3.5f);

    if (e.dead()) {
      e.stateTime += dt;
      continue;
    }
    if (e.cooldown > 0.0f) e.cooldown -= dt;

    const float dist = distance(e.pos, p.pos);
    e.sightTimer -= dt;
    if (e.sightTimer <= 0.0f) {
      e.sightTimer = 0.11f + rng_.unit() * 0.07f;
      e.seesPlayer = !p.dead() && level_->lineOfSight(e.pos, p.pos);
    }

    // Wake up on sight (or when shot).
    if (!e.awake) {
      if (e.seesPlayer && dist < def.sightRange) {
        e.awake = true;
        e.state = AIState::Chase;
        if (audio_) {
          const float vol = clampf(1.0f - dist / 26.0f, 0.15f, 0.65f);
          audio_->play(def.alertSound, vol, rng_.range(0.92f, 1.1f));
        }
      } else {
        e.state = AIState::Idle;
        e.moveAmount = lerpf(e.moveAmount, 0.0f, 1.0f - std::exp(-dt * 8.0f));
        continue;
      }
    }

    switch (e.state) {
      case AIState::Idle: {
        e.moveAmount = lerpf(e.moveAmount, 0.0f, 1.0f - std::exp(-dt * 8.0f));
        if (e.seesPlayer && dist < def.sightRange) e.state = AIState::Chase;
        break;
      }

      case AIState::Chase: {
        const bool wantsRanged = def.attack != AttackKind::Melee && dist <= def.attackRange;
        const bool wantsMelee = dist <= def.meleeRange;
        if (e.cooldown <= 0.0f && (wantsMelee || (wantsRanged && e.seesPlayer))) {
          e.state = AIState::Attack;
          e.stateTime = 0.0f;
          e.attackLanded = false;
          e.moveAmount = 0.0f;
          break;
        }

        // Pick a direction: straight at the player when visible, otherwise
        // follow the BFS flow field (which also routes through doors).
        Vec2 target = p.pos;
        const bool direct = e.seesPlayer && dist > 0.9f;
        if (!direct) {
          int ex = 0, ey = 0;
          level_->cellOf(e.pos, ex, ey);
          int nx = 0, ny = 0;
          if (level_->flowStep(ex, ey, nx, ny)) target = Level::cellCenter(nx, ny);
        }
        const Vec2 delta = target - e.pos;
        if (length(delta) > 1e-4f) {
          const Vec2 dir = normalize(delta);
          moveWithCollision(e.pos, dir * (def.speed * dt), def.radius);
          e.angle = direct ? std::atan2(p.pos.y - e.pos.y, p.pos.x - e.pos.x)
                           : std::atan2(dir.y, dir.x);
          e.moveAmount = lerpf(e.moveAmount, 1.0f, 1.0f - std::exp(-dt * 8.0f));

          // Nudge doors open while walking into them.
          const Vec2 probe = e.pos + dir * (def.radius + 0.35f);
          int cx = 0, cy = 0;
          level_->cellOf(probe, cx, cy);
          if (level_->doorIndexAt(cx, cy) >= 0) {
            if (level_->tryOpenDoor(cx, cy, false, false, nullptr) && audio_) {
              audio_->play(Sfx::DoorOpen, clampf(1.0f - dist / 30.0f, 0.1f, 0.5f));
            }
          }
        } else {
          e.moveAmount = lerpf(e.moveAmount, 0.0f, 1.0f - std::exp(-dt * 8.0f));
        }
        break;
      }

      case AIState::Attack: {
        e.stateTime += dt;
        e.moveAmount = 0.0f;
        if (!p.dead()) e.angle = std::atan2(p.pos.y - e.pos.y, p.pos.x - e.pos.x);

        if (!e.attackLanded && e.stateTime >= def.windup) {
          e.attackLanded = true;
          if (dist <= def.meleeRange + 0.3f) {
            damagePlayer(rng_.irange(def.damageMin, def.damageMax), def.name);
          } else if (def.attack == AttackKind::Hitscan) {
            if (e.seesPlayer) {
              // Moving targets are harder to hit, so a charging player is safe-ish.
              const float playerSpeed = length(p.vel);
              const float missChance = clampf(0.30f + playerSpeed * 0.09f, 0.3f, 0.75f);
              if (rng_.unit() > missChance) {
                damagePlayer(rng_.irange(def.shotDamageMin, def.shotDamageMax), def.name);
              } else {
                const Vec2 near = p.pos + fromAngle(p.angle + rng_.range(-1.2f, 1.2f)) * 0.4f;
                addParticles(near, 0.55f, 2, rgba(220, 200, 120), 0.7f, 0.25f);
              }
            }
            if (audio_) audio_->play(Sfx::Pistol, 0.3f, 0.85f);
          } else if (def.attack == AttackKind::Fireball && e.seesPlayer) {
            Projectile proj;
            proj.pos = e.pos + fromAngle(e.angle) * (def.radius + 0.15f);
            proj.vel = fromAngle(e.angle) * def.projectileSpeed;
            proj.z = 0.5f;
            proj.damage = rng_.irange(def.damageMin, def.damageMax);
            proj.fromPlayer = false;
            proj.sprite = SpriteId::Fireball;
            proj.height = 0.3f;
            projectiles_.push_back(proj);
            if (audio_) {
              const float vol = clampf(1.0f - dist / 28.0f, 0.1f, 0.5f);
              audio_->play(Sfx::Launcher, vol, 1.4f);
            }
          }
        }
        if (e.stateTime >= def.windup + 0.3f) {
          e.state = AIState::Chase;
          e.cooldown = def.cooldown * rng_.range(0.8f, 1.3f);
        }
        break;
      }

      case AIState::Pain: {
        e.stateTime += dt;
        e.moveAmount = 0.0f;
        if (e.stateTime >= 0.3f) {
          e.state = AIState::Chase;
          e.attackLanded = false;
        }
        break;
      }

      case AIState::Dead: break;
    }
  }

  // Keep enemies from stacking into a single blob.
  for (int i = 0; i < int(enemies_.size()); ++i) {
    Enemy& a = enemies_[i];
    if (a.dead()) continue;
    for (int j = i + 1; j < int(enemies_.size()); ++j) {
      Enemy& b = enemies_[j];
      if (b.dead()) continue;
      const Vec2 delta = b.pos - a.pos;
      const float d = length(delta);
      const float minDist = a.def->radius + b.def->radius;
      if (d >= minDist || d < 1e-5f) continue;
      const Vec2 push = delta * ((minDist - d) * 0.5f / d);
      a.pos -= push;
      b.pos += push;
    }
  }
}

void World::updateProjectiles(float dt) {
  Player& p = player_;

  for (Projectile& proj : projectiles_) {
    if (!proj.alive) continue;
    proj.life -= dt;
    proj.animTime += dt;
    if (proj.life <= 0.0f) {
      proj.alive = false;
      continue;
    }

    const Vec2 step = proj.vel * dt;
    const int substeps = clampi(int(length(step) / 0.2f) + 1, 1, 8);
    for (int s = 0; s < substeps && proj.alive; ++s) {
      proj.pos += step / float(substeps);

      int cx = 0, cy = 0;
      level_->cellOf(proj.pos, cx, cy);
      if (level_->blocksShots(cx, cy)) {
        if (proj.explosive) {
          explode(proj.pos, proj.explosionRadius, proj.damage, proj.fromPlayer);
        } else {
          addParticles(proj.pos, proj.z, 4, rgba(240, 140, 40), 1.2f, 0.3f);
        }
        proj.alive = false;
        break;
      }

      if (proj.fromPlayer) {
        for (int i = 0; i < int(enemies_.size()); ++i) {
          Enemy& e = enemies_[i];
          if (e.dead()) continue;
          if (distance(e.pos, proj.pos) > e.def->radius + 0.18f) continue;
          if (proj.explosive) {
            explode(proj.pos, proj.explosionRadius, proj.damage, true);
          } else {
            damageEnemy(i, proj.damage, true);
            addParticles(proj.pos, proj.z, 3, rgba(120, 200, 255), 0.8f, 0.3f);
          }
          proj.alive = false;
          break;
        }
      } else if (!p.dead()) {
        if (distance(p.pos, proj.pos) < 0.42f) {
          damagePlayer(proj.damage, "A FIREBALL");
          addParticles(proj.pos, proj.z, 5, rgba(240, 140, 40), 1.2f, 0.3f);
          proj.alive = false;
        }
      }
    }
  }

  projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(),
                                    [](const Projectile& p) { return !p.alive; }),
                     projectiles_.end());
}

void World::updateEffects(float dt) {
  for (Effect& e : effects_) e.time += dt;
  effects_.erase(std::remove_if(effects_.begin(), effects_.end(),
                                [](const Effect& e) { return e.time >= e.duration; }),
                 effects_.end());

  for (Prop& p : props_) p.flash = std::max(0.0f, p.flash - dt * 2.5f);

  for (Particle& p : particles_) {
    p.life -= dt;
    p.pos += p.vel * dt;
    p.z += p.vz * dt;
    p.vz -= p.gravity * dt;
    if (p.z < 0.03f) {
      p.z = 0.03f;
      p.vz = 0.0f;
      p.vel *= 0.6f;
    }
  }
  particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                                  [](const Particle& p) { return p.life <= 0.0f; }),
                   particles_.end());
}

void World::updateDoorsAndExits(float dt) {
  // Everyone standing in a doorway keeps it open.
  std::vector<Vec2> actors;
  actors.reserve(enemies_.size() + 1);
  actors.push_back(player_.pos);
  for (const Enemy& e : enemies_) {
    if (!e.dead()) actors.push_back(e.pos);
  }
  level_->updateDoors(dt, actors);

  // Exit doors stay sealed until the sector is clear.
  const bool cleared = aliveEnemies() == 0;
  for (Door& d : level_->doors) {
    if (d.kind == DoorKind::Exit) d.locked = !cleared;
  }

  // Reaching the exit needs the door actually slid open, so the player walks up
  // to it rather than standing inside the wall.
  if (level_->exitCell.x >= 0.0f && cleared && !player_.dead() && exitDoorOpen()) {
    if (distance(player_.pos, level_->exitCell) < 1.15f) exitReached = true;
  }
}

bool World::exitDoorOpen() const {
  for (const Door& d : level_->doors) {
    if (d.kind == DoorKind::Exit && d.open < 0.6f) return false;
  }
  return true;
}

void World::update(float dt, const InputState& input) {
  elapsed += dt;
  if (messageTimer > 0.0f) messageTimer = std::max(0.0f, messageTimer - dt);

  if (level_ && level_->w > 0) level_->computeFlow(player_.pos);

  updatePlayer(dt, input);
  updateEnemies(dt);
  updateProjectiles(dt);
  updateEffects(dt);
  updateDoorsAndExits(dt);

  // Pickups: walk over them to collect.
  if (!player_.dead()) {
    for (int i = 0; i < int(pickups_.size()); ++i) {
      Pickup& item = pickups_[size_t(i)];
      if (item.taken) continue;
      item.bobPhase += dt * 3.0f;
      if (distance(item.pos, player_.pos) < 0.55f) pickupAt(i);
    }
    pickups_.erase(std::remove_if(pickups_.begin(), pickups_.end(),
                                  [](const Pickup& p) { return p.taken; }),
                   pickups_.end());
  }
}

int World::aliveEnemies() const {
  int n = 0;
  for (const Enemy& e : enemies_) {
    if (!e.dead()) ++n;
  }
  return n;
}

bool World::levelComplete() const {
  if (level_->exitCell.x < 0.0f) return false;
  return exitReached;
}

// ---------------------------------------------------------------------------
// Sprite gathering
// ---------------------------------------------------------------------------
void World::hitSpriteFor(const Enemy& e, SpriteDraw& out) const {
  const EnemyDef& def = *e.def;
  const SpriteSet* set = nullptr;
  int frame = 0;

  if (e.dead()) {
    set = &assets_->anim(def.death);
    // The corpse holds its final frame once the fall is over.
    const float t = clampf(e.stateTime / 0.85f, 0.0f, 1.0f);
    frame = clampi(int(t * float(set->count())), 0, set->count() - 1);
  } else if (e.state == AIState::Pain) {
    set = &assets_->anim(def.pain);
    frame = 0;
  } else if (e.state == AIState::Attack) {
    set = &assets_->anim(def.attackAnim);
    const float t = clampf(e.stateTime / std::max(0.05f, def.windup), 0.0f, 1.0f);
    frame = clampi(int(t * float(set->count())), 0, set->count() - 1);
  } else if (e.moveAmount > 0.25f) {
    set = &assets_->anim(def.walk);
    frame = clampi(int(e.animTime * 7.0f), 0, kWalkFrames - 1);
    if (set->count() != kWalkFrames) frame = clampi(frame, 0, set->count() - 1);
  } else {
    set = &assets_->anim(def.idle);
    frame = clampi(int(e.animTime * 2.0f), 0, kIdleFrames - 1);
    if (set->count() != kIdleFrames) frame = clampi(frame, 0, set->count() - 1);
  }

  out.tex = &set->frame(frame);
  out.pos = e.pos;
  out.z = 0.0f;
  out.height = def.worldHeight;
  out.emissive = 0.0f;
  // Flash white-ish on hit, dark red while dying.
  if (e.hitFlash > 0.01f) {
    out.tint = mixColor(0xFFFFFFFFu, 0xFFFF8080u, e.hitFlash);
  } else if (e.dead()) {
    out.tint = mixColor(0xFFFFFFFFu, 0xFF6A5050u, 0.55f);
  }
}

void World::gatherSprites(std::vector<SpriteDraw>& out) const {
  out.clear();
  out.reserve(enemies_.size() + props_.size() + pickups_.size() + projectiles_.size() +
              effects_.size() + particles_.size());

  for (const Enemy& e : enemies_) {
    SpriteDraw d;
    hitSpriteFor(e, d);
    out.push_back(d);
  }

  for (const Prop& p : props_) {
    SpriteDraw d;
    d.tex = &assets_->anim(p.sprite).frame(0);
    d.pos = p.pos;
    d.z = 0.0f;
    d.height = p.height;
    if (p.flash > 0.01f) d.tint = mixColor(0xFFFFFFFFu, 0xFFFF9090u, p.flash);
    out.push_back(d);
  }

  for (const Pickup& item : pickups_) {
    SpriteDraw d;
    d.tex = &assets_->anim(item.sprite).frame(0);
    d.pos = item.pos;
    d.z = std::max(0.0f, std::sin(item.bobPhase) * 0.03f);
    d.height = item.height;
    out.push_back(d);
  }

  for (const Projectile& proj : projectiles_) {
    const SpriteSet& set = assets_->anim(proj.sprite);
    SpriteDraw d;
    d.tex = &set.frame(int(proj.animTime * 14.0f) % std::max(1, set.count()));
    d.pos = proj.pos;
    d.z = proj.z - proj.height * 0.5f;
    d.height = proj.height;
    d.emissive = 1.0f;
    out.push_back(d);
  }

  for (const Effect& e : effects_) {
    const SpriteSet& set = assets_->anim(e.sprite);
    const float t = clampf(e.time / std::max(0.01f, e.duration), 0.0f, 0.999f);
    SpriteDraw d;
    d.tex = &set.frame(int(t * float(set.count())));
    d.pos = e.pos;
    d.z = e.z;
    d.height = e.height;
    d.emissive = 1.0f;
    d.alpha = 1.0f - t * 0.35f;
    out.push_back(d);
  }

  for (const Particle& p : particles_) {
    SpriteDraw d;
    d.tex = &assets_->anim(SpriteId::Particle).frame(0);
    d.pos = p.pos;
    d.z = p.z - p.size * 0.5f;
    d.height = p.size;
    d.emissive = 1.0f;
    d.tint = p.color;
    d.alpha = clampf(p.life / std::max(0.01f, p.maxLife), 0.0f, 1.0f);
    out.push_back(d);
  }
}

const Texture* World::viewModelTexture() const {
  const WeaponDef& weapon = weaponDef(player_.weapon);
  const bool firing = player_.muzzleFlash > 0.005f;
  return &assets_->anim(firing ? weapon.viewFire : weapon.view).frame(0);
}

}  // namespace fps
