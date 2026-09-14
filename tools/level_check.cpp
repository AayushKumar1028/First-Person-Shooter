// level_check.cpp - static validation of the campaign maps.
//
// Checks row alignment, seals, and that every spawn / pickup / exit is reachable
// from the player start. Run: ./level_check
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

#include "level.h"
#include "levels.h"

using namespace fps;

namespace {

const char* spawnLabel(SpawnKind kind) {
  switch (kind) {
    case SpawnKind::PlayerStart: return "player";
    case SpawnKind::Zombie: return "guard";
    case SpawnKind::Imp: return "imp";
    case SpawnKind::Brute: return "brute";
    case SpawnKind::Barrel: return "barrel";
    case SpawnKind::Lamp: return "lamp";
    case SpawnKind::TechPillar: return "pillar";
    case SpawnKind::Gore: return "gore";
    case SpawnKind::KeyRed: return "RED KEY";
    case SpawnKind::HealthSmall: return "stim";
    case SpawnKind::HealthLarge: return "medkit";
    case SpawnKind::ArmorSmall: return "armor";
    case SpawnKind::ArmorLarge: return "big armor";
    case SpawnKind::AmmoBullets: return "bullets";
    case SpawnKind::AmmoShells: return "shells";
    case SpawnKind::AmmoRockets: return "rockets";
    case SpawnKind::AmmoCells: return "cells";
    case SpawnKind::Shotgun: return "SHOTGUN";
    case SpawnKind::Chaingun: return "CHAINGUN";
    case SpawnKind::Launcher: return "LAUNCHER";
    case SpawnKind::Plasma: return "PLASMA";
    default: return "?";
  }
}

// Flood fill from the start; "lockedDoorsBlock" decides whether the red-key
// doors count as passable.
std::vector<char> flood(const Level& level, bool lockedDoorsBlock) {
  std::vector<char> seen(size_t(level.w) * size_t(level.h), 0);
  auto walkable = [&](int x, int y) {
    if (!level.inside(x, y)) return false;
    const Tile& t = level.at(x, y);
    if (t.kind == TileKind::Wall) return false;
    if (lockedDoorsBlock && t.doorIndex >= 0 && level.doors[size_t(t.doorIndex)].kind ==
                                                     DoorKind::Locked) {
      return false;
    }
    return true;
  };
  int sx = 0, sy = 0;
  level.cellOf(level.playerStart, sx, sy);
  if (!walkable(sx, sy)) return seen;
  std::deque<std::pair<int, int>> queue;
  queue.push_back({sx, sy});
  seen[size_t(sy) * size_t(level.w) + size_t(sx)] = 1;
  const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
  while (!queue.empty()) {
    const auto [cx, cy] = queue.front();
    queue.pop_front();
    for (int k = 0; k < 4; ++k) {
      const int nx = cx + dx[k], ny = cy + dy[k];
      if (!walkable(nx, ny)) continue;
      char& cell = seen[size_t(ny) * size_t(level.w) + size_t(nx)];
      if (cell) continue;
      cell = 1;
      queue.push_back({nx, ny});
    }
  }
  return seen;
}

int countReachable(const std::vector<char>& seen) {
  int n = 0;
  for (char c : seen) n += c ? 1 : 0;
  return n;
}

bool reachableAt(const Level& level, const std::vector<char>& seen, const Vec2& pos) {
  int x = 0, y = 0;
  level.cellOf(pos, x, y);
  if (!level.inside(x, y)) return false;
  return seen[size_t(y) * size_t(level.w) + size_t(x)] != 0;
}

void dumpWithReach(const Level& level, const std::vector<char>& seen) {
  for (int y = 0; y < level.h; ++y) {
    std::string line;
    for (int x = 0; x < level.w; ++x) {
      const Tile& t = level.at(x, y);
      if (t.kind == TileKind::Wall) {
        line.push_back('#');
      } else {
        const bool ok = seen[size_t(y) * size_t(level.w) + size_t(x)] != 0;
        if (t.kind == TileKind::ExitDoor) line.push_back('Z');
        else if (t.kind == TileKind::Door) line.push_back(ok ? '+' : 'x');
        else line.push_back(ok ? ' ' : '.');
      }
    }
    std::printf("  |%s|\n", line.c_str());
  }
}

int checkLevel(const CampaignLevel& data, int index) {
  Level level;
  level.defaultFloor = data.floor;
  level.defaultCeil = data.ceil;
  if (!level.loadAscii(data.name, data.rows)) {
    std::printf("LEVEL %d (%s): failed to load\n", index + 1, data.name.c_str());
    return 1;
  }

  int problems = 0;
  int widest = 0;
  for (const std::string& r : data.rows) widest = std::max(widest, int(r.size()));
  for (size_t i = 0; i < data.rows.size(); ++i) {
    if (int(data.rows[i].size()) != widest) {
      std::printf("  ! row %d is %d chars, expected %d\n", int(i), int(data.rows[i].size()), widest);
      ++problems;
    }
  }

  std::printf("\n=== LEVEL %d: %s  (%dx%d, %zu rows) ===\n", index + 1, level.name.c_str(), level.w,
              level.h, data.rows.size());

  const std::vector<char> openDoors = flood(level, false);
  const std::vector<char> closedLocked = flood(level, true);
  int walkable = 0;
  for (int y = 0; y < level.h; ++y)
    for (int x = 0; x < level.w; ++x)
      if (level.at(x, y).kind != TileKind::Wall) ++walkable;

  std::printf("  walkable=%d reachable(doors open)=%d reachable(locked shut)=%d\n", walkable,
              countReachable(openDoors), countReachable(closedLocked));

  const bool startOk = reachableAt(level, openDoors, level.playerStart);
  std::printf("  player start (%.1f,%.1f) reachable=%s\n", level.playerStart.x, level.playerStart.y,
              startOk ? "yes" : "NO");
  if (!startOk) ++problems;

  if (level.exitCell.x >= 0.0f) {
    const bool ok = reachableAt(level, closedLocked, level.exitCell) ||
                    reachableAt(level, openDoors, level.exitCell);
    const bool gated = !reachableAt(level, closedLocked, level.exitCell);
    std::printf("  exit at (%.1f,%.1f) reachable=%s%s\n", level.exitCell.x, level.exitCell.y,
                ok ? "yes" : "NO", gated ? " (behind a locked door)" : "");
    if (!ok) ++problems;
  } else {
    std::printf("  ! no exit door 'Z' in this level\n");
    ++problems;
  }

  int items = 0, enemies = 0, unreachableItems = 0;
  bool keyReachable = false;
  bool hasLockedDoor = false;
  for (const Door& d : level.doors) {
    if (d.kind == DoorKind::Locked) hasLockedDoor = true;
  }
  for (const EntitySpawn& s : level.spawns) {
    if (s.kind == SpawnKind::Zombie || s.kind == SpawnKind::Imp || s.kind == SpawnKind::Brute) {
      ++enemies;
    } else {
      ++items;
    }
    // Items may sit behind the keycard door; the key itself may not.
    const bool isKey = s.kind == SpawnKind::KeyRed;
    const bool ok = reachableAt(level, isKey ? closedLocked : openDoors, s.pos);
    if (isKey && ok) keyReachable = true;
    if (!ok) {
      ++unreachableItems;
      std::printf("  ! %s at (%.1f,%.1f) is not reachable\n", spawnLabel(s.kind), s.pos.x, s.pos.y);
      ++problems;
    }
  }
  std::printf("  spawns: %d enemies, %d items/props\n", enemies, items);
  if (hasLockedDoor) {
    std::printf("  locked door present, red key reachable without it: %s\n",
                keyReachable ? "yes" : "NO");
    if (!keyReachable) ++problems;
  }

  dumpWithReach(level, openDoors);
  std::printf("  --> %s\n", problems == 0 ? "OK" : "PROBLEMS FOUND");
  return problems;
}

}  // namespace

int main() {
  const std::vector<CampaignLevel> campaign = buildCampaign();
  int problems = 0;
  for (size_t i = 0; i < campaign.size(); ++i) problems += checkLevel(campaign[i], int(i));
  std::printf("\n%d level(s) checked, %d problem(s)\n", int(campaign.size()), problems);
  return problems == 0 ? 0 : 1;
}
