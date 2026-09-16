// entities.h - the simulated world: player, weapons, enemies, props, effects.
#pragma once

#include <string>
#include <vector>

#include "assets.h"
#include "audio.h"
#include "level.h"
#include "render.h"

namespace fps {

// ---------------------------------------------------------------------------
// Weapons
// ---------------------------------------------------------------------------
enum class WeaponId : int { Pistol, Shotgun, Chaingun, Launcher, Plasma, Count };
enum class AmmoType : int { Bullets, Shells, Rockets, Cells, Count };

struct WeaponDef {
  const char* name;
  AmmoType ammo;
  int damageMin;
  int damageMax;
  int pellets = 1;
  float spreadDeg = 0.0f;
  float cooldown = 0.3f;   // seconds between shots
  bool autoFire = true;
  bool hitscan = true;
  float projectileSpeed = 0.0f;
  float explosionRadius = 0.0f;
  int ammoPerShot = 1;
  float kick = 0.0f;       // upward recoil in radians
  float range = 32.0f;
  SpriteId view;
  SpriteId viewFire;
  SpriteId pickupSprite;
  Sfx sound;
};

const WeaponDef& weaponDef(WeaponId id);
const WeaponDef& weaponDef(int index);
int maxAmmo(AmmoType type);
const char* ammoName(AmmoType type);

// ---------------------------------------------------------------------------
// Enemies
// ---------------------------------------------------------------------------
enum class EnemyType : int { Zombie, Imp, Brute, Count };
enum class AttackKind : int { Melee, Hitscan, Fireball };
enum class AIState : int { Idle, Chase, Attack, Pain, Dead };

struct EnemyDef {
  const char* name;
  int health;
  float speed;
  float radius;
  float worldHeight;
  int damageMin, damageMax;      // melee / projectile damage
  int shotDamageMin, shotDamageMax;  // hitscan damage (zombie only)
  AttackKind attack;
  float attackRange;   // distance at which the ranged attack is used
  float meleeRange;    // distance at which it switches to melee
  float windup;        // delay between starting and landing the attack
  float cooldown;      // delay after an attack
  float sightRange;
  float projectileSpeed;
  int score;
  SpriteId idle, walk, attackAnim, pain, death;
  Sfx alertSound, painSound, deathSound;
};

const EnemyDef& enemyDef(EnemyType type);
const EnemyDef& enemyDef(int index);

struct Enemy {
  EnemyType type = EnemyType::Zombie;
  const EnemyDef* def = nullptr;
  Vec2 pos;
  float angle = 0.0f;
  int health = 0;
  AIState state = AIState::Idle;
  float stateTime = 0.0f;
  float animTime = 0.0f;
  float cooldown = 0.0f;
  float sightTimer = 0.0f;
  bool awake = false;
  bool attackLanded = false;
  bool seesPlayer = false;
  float hitFlash = 0.0f;
  Vec2 moveDir;
  float moveAmount = 0.0f;  // 0..1 blend used to pick walk vs idle frames

  bool dead() const { return state == AIState::Dead; }
  // Corpses linger for a moment, then stay as scenery.
  bool finishedDying() const { return dead() && stateTime > 1.6f; }
};

// ---------------------------------------------------------------------------
// World objects
// ---------------------------------------------------------------------------
struct Pickup {
  SpawnKind kind = SpawnKind::HealthSmall;
  Vec2 pos;
  SpriteId sprite = SpriteId::HealthSmall;
  float height = 0.85f;
  float bobPhase = 0.0f;
  bool taken = false;
  bool respawns = false;
  float respawnTimer = 0.0f;
};

struct Prop {
  SpawnKind kind = SpawnKind::Barrel;
  Vec2 pos;
  SpriteId sprite = SpriteId::Barrel;
  float height = 0.85f;
  int health = 0;
  bool explosive = false;
  bool destroyed = false;
  float flash = 0.0f;
};

struct Projectile {
  Vec2 pos;
  Vec2 vel;
  float z = 0.5f;
  float radius = 0.0f;   // collision radius (explosion radius when it lands)
  int damage = 0;
  bool fromPlayer = false;
  bool explosive = false;
  float explosionRadius = 0.0f;
  float life = 4.0f;
  float animTime = 0.0f;
  bool alive = true;
  SpriteId sprite = SpriteId::Fireball;
  float height = 0.28f;
  int ownerIndex = -1;
};

struct Effect {
  Vec2 pos;
  float z = 0.0f;
  SpriteId sprite = SpriteId::Explosion;
  float time = 0.0f;
  float duration = 0.4f;
  float height = 0.85f;
  float emissive = 1.0f;
};

struct Particle {
  Vec2 pos;
  float z = 0.5f;
  Vec2 vel;
  float vz = 0.0f;
  float life = 0.0f;
  float maxLife = 1.0f;
  float size = 0.1f;
  float gravity = 0.0f;
  RGBA color = 0xFFFFFFFFu;
};

// ---------------------------------------------------------------------------
// Player
// ---------------------------------------------------------------------------
struct Player {
  Vec2 pos;
  Vec2 vel;
  float angle = 0.0f;
  float height = 0.5f;  // eye height including head bob
  float bobPhase = 0.0f;
  float bobAmount = 0.0f;
  int health = 100;
  int armor = 0;
  int ammo[4] = {50, 0, 0, 0};
  bool hasWeapon[int(WeaponId::Count)] = {true, false, false, false, false};
  int weapon = int(WeaponId::Pistol);
  float fireCooldown = 0.0f;
  float switchTimer = 0.0f;
  bool hasRedKey = false;
  float muzzleFlash = 0.0f;
  float damageFlash = 0.0f;
  float pickupFlash = 0.0f;
  float hitDirection = 0.0f;
  float deathTime = 0.0f;
  float shake = 0.0f;
  int kills = 0;

  bool dead() const { return health <= 0; }
};

// ---------------------------------------------------------------------------
// Input snapshot for one simulation tick
// ---------------------------------------------------------------------------
struct InputState {
  bool forward = false;
  bool back = false;
  bool strafeLeft = false;
  bool strafeRight = false;
  bool run = false;
  bool fire = false;
  bool use = false;
  bool turnLeft = false;
  bool turnRight = false;
  float turn = 0.0f;   // yaw delta in radians for this tick
  int selectWeapon = -1;
  int cycleWeapon = 0;
  // One-shot menu navigation, consumed by the game state machine.
  bool menuUp = false;
  bool menuDown = false;
  bool menuConfirm = false;
  bool menuBack = false;
};

// ---------------------------------------------------------------------------
// World
// ---------------------------------------------------------------------------
class World {
 public:
  void reset(Level* level, Assets* assets, Audio* audio);
  // Instantiates enemies/props/pickups from the level's spawn list.
  void spawnFromLevel();

  void update(float dt, const InputState& input);

  void addEnemy(EnemyType type, const Vec2& pos, bool awake = false);
  void addPickup(SpawnKind kind, const Vec2& pos, bool respawns = false);
  void addProp(SpawnKind kind, const Vec2& pos);
  void addEffect(const Vec2& pos, float z, SpriteId sprite, float duration, float height);
  void addParticles(const Vec2& pos, float z, int count, RGBA color, float speed, float life);
  // Removes pickups (arena mode re-populates between waves).
  void clearPickups();
  void clearProjectiles();

  void damagePlayer(int amount, const char* source = nullptr);
  void damageEnemy(int index, int amount, bool fromPlayer);
  void damageProp(int index, int amount, bool fromPlayer);
  void explode(const Vec2& pos, float radius, int damage, bool fromPlayer);

  void addMessage(const std::string& text);
  bool levelComplete() const;

  int aliveEnemies() const;
  int deadEnemies() const { return player_.kills; }
  int enemyTotal() const { return int(enemies_.size()); }
  int pickupCount() const { return int(pickups_.size()); }
  int propCount() const { return int(props_.size()); }
  const std::vector<Enemy>& enemies() const { return enemies_; }
  const std::vector<Projectile>& projectiles() const { return projectiles_; }
  const std::vector<Prop>& props() const { return props_; }
  const std::vector<Pickup>& pickups() const { return pickups_; }

  Player& player() { return player_; }
  const Player& player() const { return player_; }
  Level* level() { return level_; }

  // Builds the billboard list for the renderer.
  void gatherSprites(std::vector<SpriteDraw>& out) const;
  // First-person view model texture for this frame (drawn by the HUD).
  const Texture* viewModelTexture() const;
  // World-space sprite that marks the exit door, so it is easy to spot.
  bool exitUnlocked() const { return aliveEnemies() == 0 && aliveSpawned_; }

  std::string message;
  float messageTimer = 0.0f;
  float elapsed = 0.0f;
  int score = 0;
  bool exitReached = false;

 private:
  Level* level_ = nullptr;
  Assets* assets_ = nullptr;
  Audio* audio_ = nullptr;
  Rng rng_{0xBADC0DEu};

  Player player_;
  bool prevFire_ = false;
  bool aliveSpawned_ = false;
  std::vector<Enemy> enemies_;
  std::vector<Pickup> pickups_;
  std::vector<Prop> props_;
  std::vector<Projectile> projectiles_;
  std::vector<Effect> effects_;
  std::vector<Particle> particles_;

  void updatePlayer(float dt, const InputState& input);
  void updateEnemies(float dt);
  void updateProjectiles(float dt);
  void updateEffects(float dt);
  void updateDoorsAndExits(float dt);

  void moveWithCollision(Vec2& pos, const Vec2& delta, float radius) const;
  void tryFire();
  void fireHitscan(const WeaponDef& weapon);
  void fireProjectile(const WeaponDef& weapon);
  void playerUse();
  void pickupAt(int index);
  void killEnemy(int index, bool fromPlayer);
  int findEnemyAlongRay(const Vec2& origin, const Vec2& dir, float maxDist) const;
  int findPropAlongRay(const Vec2& origin, const Vec2& dir, float maxDist) const;
  bool exitDoorOpen() const;
  void hitSpriteFor(const Enemy& e, SpriteDraw& out) const;
};

}  // namespace fps
