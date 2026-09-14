// level.h - grid map, doors, spawns, collision queries and AI pathfinding.
#pragma once

#include <string>
#include <vector>

#include "assets.h"
#include "core.h"

namespace fps {

enum class TileKind : uint8_t { Empty, Wall, Door, ExitDoor };

// Doors 'L' need the red keycard; 'D'/'d' are normal sliding doors.
enum class DoorKind : uint8_t { Metal, Wood, Locked, Exit };

struct Tile {
  TileKind kind = TileKind::Empty;
  Tex wall = Tex::WallBrick;   // used when kind != Empty
  Tex floor = Tex::FloorStone;
  Tex ceil = Tex::CeilStone;
  int doorIndex = -1;

  bool isSolidTile() const { return kind == TileKind::Wall; }
};

struct Door {
  int cx = 0, cy = 0;
  DoorKind kind = DoorKind::Metal;
  Tex tex = Tex::DoorMetal;
  float open = 0.0f;      // 0 fully shut .. 1 fully open
  bool moving = false;    // true while sliding
  bool fullyOpen = false;
  bool locked = false;    // still needs the key
  bool used = false;      // exit doors: triggered at least once
  float holdTimer = 0.0f; // countdown before auto-closing
};

enum class SpawnKind : int {
  PlayerStart,
  Zombie,
  Imp,
  Brute,
  Barrel,
  Lamp,
  TechPillar,
  Gore,
  KeyRed,
  HealthSmall,
  HealthLarge,
  ArmorSmall,
  ArmorLarge,
  AmmoBullets,
  AmmoShells,
  AmmoRockets,
  AmmoCells,
  Shotgun,
  Chaingun,
  Launcher,
  Plasma,
  Count
};

struct EntitySpawn {
  SpawnKind kind = SpawnKind::Zombie;
  Vec2 pos;
  float angle = 0.0f;
};

struct RayHit {
  bool hit = false;
  float dist = 0.0f;   // distance along the (normalised) ray
  int cx = 0, cy = 0;  // cell that was hit
  int doorIndex = -1;
  bool door = false;
  float u = 0.0f;  // 0..1 across the face that was hit
};

class Level {
 public:
  std::string name;
  std::string intro;
  int w = 0, h = 0;
  std::vector<Tile> tiles;
  std::vector<Door> doors;
  std::vector<EntitySpawn> spawns;
  Vec2 playerStart{1.5f, 1.5f};
  float playerAngle = 0.0f;
  Tex defaultFloor = Tex::FloorStone;
  Tex defaultCeil = Tex::CeilStone;
  Vec2 exitCell{-1.0f, -1.0f};  // centre of the first exit door

  void clear();
  // Builds the map from ASCII rows (see level.cpp for the legend).
  bool loadAscii(const std::string& levelName, const std::vector<std::string>& rows);
  // Procedural deathmatch-style arena used by the endless mode.
  void buildArena(int seed, int wave);

  inline Tile& at(int x, int y) { return tiles[size_t(y) * size_t(w) + size_t(x)]; }
  inline const Tile& at(int x, int y) const { return tiles[size_t(y) * size_t(w) + size_t(x)]; }
  inline bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }

  static Vec2 cellCenter(int x, int y) { return Vec2(float(x) + 0.5f, float(y) + 0.5f); }
  void cellOf(const Vec2& p, int& x, int& y) const {
    x = int(std::floor(p.x));
    y = int(std::floor(p.y));
  }

  bool isWall(int x, int y) const;
  // Solid for movement: walls, plus doors that are not open enough to walk through.
  bool isSolid(int x, int y) const;
  // Blocks bullets: walls and mostly-shut doors.
  bool blocksShots(int x, int y) const;
  bool blocksSight(int x, int y) const;
  float doorOpen(int x, int y) const;

  bool collides(const Vec2& center, float radius) const;
  RayHit rayCast(const Vec2& origin, const Vec2& dir, float maxDist) const;
  bool lineOfSight(const Vec2& a, const Vec2& b) const;

  // Door interaction. Returns true when something actually happened.
  bool tryOpenDoor(int x, int y, bool byPlayer, bool hasKey, std::string* message);
  void updateDoors(float dt, const std::vector<Vec2>& actors);
  int doorIndexAt(int x, int y) const;

  // Random walkable cell, used to place pickups and arena spawns.
  Vec2 randomOpenCell(Rng& rng, float minDistFrom = 0.0f, const Vec2& avoid = Vec2()) const;

  // BFS distance field from "target" over walkable cells (-1 = unreachable).
  void computeFlow(const Vec2& target);
  int flowAt(int x, int y) const;
  // Next cell to step toward, following the flow field. Returns false if stuck.
  bool flowStep(int fromX, int fromY, int& outX, int& outY) const;

  int countSpawns(SpawnKind kind) const;

 private:
  std::vector<int> flow_;
  void parseCell(char ch, int x, int y);
};

}  // namespace fps
