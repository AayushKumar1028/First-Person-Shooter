// level.cpp - map loading, doors, collision, raycasting and AI pathfinding.
//
// ---------------------------------------------------------------------------
// ASCII legend (used by levels in levels.cpp)
// ---------------------------------------------------------------------------
//  Walls    '#' brick      '%' stone     'M' metal     'N' blue brick
//           'W' wood       'G' tech      'C' concrete  'X' bloodied concrete
//           'R' crate      'U' steel     'F' banner    'S' exit sign
//  Doors    'D' metal      'd' wood      'L' locked    'Z' level exit
//  Floors   ',' stone      '.' cobble    '_' grate     '=' tech
//           ':' dirt       ';' blood
//  Ceils    '~' stone      '^' rust      '"' dark      '`' tech
//  Spawns   'P' player     'z' zombie    'i' imp       'B' brute
//           'o' barrel     'l' lamp      't' pillar    'g' gore
//           'K' red key | 'h' health  'H' medkit | 'v' small armor  'V' armor
//           'm' bullets    'n' shells    'k' rockets   'e' energy cells
//           'w' shotgun    'q' chaingun  'y' launcher  'j' plasma rifle
//  ' ' (space) is empty floor using the level's default floor/ceiling.
// ---------------------------------------------------------------------------
#include "level.h"

#include <deque>

namespace fps {

void Level::clear() {
  name.clear();
  intro.clear();
  w = h = 0;
  tiles.clear();
  doors.clear();
  spawns.clear();
  playerStart = Vec2(1.5f, 1.5f);
  playerAngle = 0.0f;
  exitCell = Vec2(-1.0f, -1.0f);
  flow_.clear();
}

void Level::parseCell(char ch, int x, int y) {
  Tile& t = at(x, y);
  const Vec2 centre = cellCenter(x, y);

  switch (ch) {
    // ---- walls ----
    case '#': t.kind = TileKind::Wall; t.wall = Tex::WallBrick; return;
    case '%': t.kind = TileKind::Wall; t.wall = Tex::WallStone; return;
    case 'M': t.kind = TileKind::Wall; t.wall = Tex::WallMetal; return;
    case 'N': t.kind = TileKind::Wall; t.wall = Tex::WallBlue; return;
    case 'W': t.kind = TileKind::Wall; t.wall = Tex::WallWood; return;
    case 'G': t.kind = TileKind::Wall; t.wall = Tex::WallGreen; return;
    case 'C': t.kind = TileKind::Wall; t.wall = Tex::WallConcrete; return;
    case 'X': t.kind = TileKind::Wall; t.wall = Tex::WallBlood; return;
    case 'R': t.kind = TileKind::Wall; t.wall = Tex::WallCrate; return;
    case 'U': t.kind = TileKind::Wall; t.wall = Tex::WallSupport; return;
    case 'F': t.kind = TileKind::Wall; t.wall = Tex::WallFlag; return;
    case 'S': t.kind = TileKind::Wall; t.wall = Tex::WallExit; return;

    // ---- doors ----
    case 'D':
    case 'd':
    case 'L':
    case 'Z': {
      Door door;
      door.cx = x;
      door.cy = y;
      if (ch == 'd') { door.kind = DoorKind::Wood; door.tex = Tex::DoorWood; }
      else if (ch == 'L') { door.kind = DoorKind::Locked; door.tex = Tex::DoorLocked; door.locked = true; }
      else if (ch == 'Z') { door.kind = DoorKind::Exit; door.tex = Tex::DoorExit; }
      else { door.kind = DoorKind::Metal; door.tex = Tex::DoorMetal; }
      t.kind = (ch == 'Z') ? TileKind::ExitDoor : TileKind::Door;
      t.wall = door.tex;
      t.doorIndex = int(doors.size());
      doors.push_back(door);
      if (ch == 'Z' && exitCell.x < 0.0f) exitCell = centre;
      return;
    }

    // ---- floor overrides ----
    case ',': t.floor = Tex::FloorStone; return;
    case '.': t.floor = Tex::FloorCobble; return;
    case '_': t.floor = Tex::FloorMetal; return;
    case '=': t.floor = Tex::FloorTech; return;
    case ':': t.floor = Tex::FloorDirt; return;
    case ';': t.floor = Tex::FloorBlood; return;

    // ---- ceiling overrides ----
    case '~': t.ceil = Tex::CeilStone; return;
    case '^': t.ceil = Tex::CeilRust; return;
    case '"': t.ceil = Tex::CeilDark; return;
    case '`': t.ceil = Tex::CeilTech; return;

    // ---- spawns ----
    case 'P': spawns.push_back({SpawnKind::PlayerStart, centre, playerAngle}); return;
    case 'z': spawns.push_back({SpawnKind::Zombie, centre, 0.0f}); return;
    case 'i': spawns.push_back({SpawnKind::Imp, centre, 0.0f}); return;
    case 'B': spawns.push_back({SpawnKind::Brute, centre, 0.0f}); return;
    case 'o': spawns.push_back({SpawnKind::Barrel, centre, 0.0f}); return;
    case 'l': spawns.push_back({SpawnKind::Lamp, centre, 0.0f}); return;
    case 't': spawns.push_back({SpawnKind::TechPillar, centre, 0.0f}); return;
    case 'g': spawns.push_back({SpawnKind::Gore, centre, 0.0f}); return;
    case 'K': spawns.push_back({SpawnKind::KeyRed, centre, 0.0f}); return;
    case 'h': spawns.push_back({SpawnKind::HealthSmall, centre, 0.0f}); return;
    case 'H': spawns.push_back({SpawnKind::HealthLarge, centre, 0.0f}); return;
    case 'v': spawns.push_back({SpawnKind::ArmorSmall, centre, 0.0f}); return;
    case 'V': spawns.push_back({SpawnKind::ArmorLarge, centre, 0.0f}); return;
    case 'm': spawns.push_back({SpawnKind::AmmoBullets, centre, 0.0f}); return;
    case 'n': spawns.push_back({SpawnKind::AmmoShells, centre, 0.0f}); return;
    case 'k': spawns.push_back({SpawnKind::AmmoRockets, centre, 0.0f}); return;
    case 'e': spawns.push_back({SpawnKind::AmmoCells, centre, 0.0f}); return;
    case 'w': spawns.push_back({SpawnKind::Shotgun, centre, 0.0f}); return;
    case 'q': spawns.push_back({SpawnKind::Chaingun, centre, 0.0f}); return;
    case 'y': spawns.push_back({SpawnKind::Launcher, centre, 0.0f}); return;
    case 'j': spawns.push_back({SpawnKind::Plasma, centre, 0.0f}); return;

    default: return;  // empty floor
  }
}

bool Level::loadAscii(const std::string& levelName, const std::vector<std::string>& rows) {
  clear();
  name = levelName;
  h = int(rows.size());
  for (const std::string& r : rows) w = std::max(w, int(r.size()));
  if (w <= 0 || h <= 0) return false;

  tiles.assign(size_t(w) * size_t(h), Tile());
  for (Tile& t : tiles) {
    t.floor = defaultFloor;
    t.ceil = defaultCeil;
  }

  for (int y = 0; y < h; ++y) {
    const std::string& row = rows[size_t(y)];
    for (int x = 0; x < w; ++x) {
      // Short rows are padded with wall so a hand-written map can never leak
      // the player into the void.
      const char ch = x < int(row.size()) ? row[size_t(x)] : '#';
      parseCell(ch, x, y);
    }
  }

  // Seal the border, but keep any door the map explicitly placed there (the
  // campaign uses a door in the bottom wall as the level exit).
  for (int x = 0; x < w; ++x) {
    for (const int y : {0, h - 1}) {
      Tile& t = at(x, y);
      if (t.kind == TileKind::Empty) {
        t.kind = TileKind::Wall;
        t.wall = Tex::WallConcrete;
      }
    }
  }
  for (int y = 0; y < h; ++y) {
    for (const int x : {0, w - 1}) {
      Tile& t = at(x, y);
      if (t.kind == TileKind::Empty) {
        t.kind = TileKind::Wall;
        t.wall = Tex::WallConcrete;
      }
    }
  }

  // First player start wins; the rest are ignored.
  for (size_t i = 0; i < spawns.size(); ++i) {
    if (spawns[i].kind == SpawnKind::PlayerStart) {
      playerStart = spawns[i].pos;
      playerAngle = spawns[i].angle;
      spawns.erase(spawns.begin() + std::ptrdiff_t(i));
      break;
    }
  }
  if (exitCell.x < 0.0f) {
    // Fall back to any exit door, else the map centre.
    for (const Door& d : doors) {
      if (d.kind == DoorKind::Exit) {
        exitCell = cellCenter(d.cx, d.cy);
        break;
      }
    }
  }
  return true;
}

void Level::buildArena(int seed, int wave) {
  clear();
  Rng rng(uint32_t(seed * 7919 + wave * 104729 + 13));
  name = "ARENA";
  // Arena grows slowly with the wave count.
  const int base = clampi(18 + wave, 18, 28);
  w = base;
  h = base;
  defaultFloor = Tex::FloorTech;
  defaultCeil = Tex::CeilTech;

  tiles.assign(size_t(w) * size_t(h), Tile());
  for (Tile& t : tiles) {
    t.floor = (rng.chance(0.25f)) ? Tex::FloorMetal : Tex::FloorTech;
    t.ceil = rng.chance(0.3f) ? Tex::CeilDark : Tex::CeilTech;
  }

  const Tex wallChoices[4] = {Tex::WallMetal, Tex::WallGreen, Tex::WallSupport, Tex::WallBlue};
  const Tex chosenWall = wallChoices[rng.irange(0, 3)];
  auto wall = [&](int x, int y) {
    if (!inside(x, y)) return;
    Tile& t = at(x, y);
    t.kind = TileKind::Wall;
    t.wall = chosenWall;
  };

  for (int x = 0; x < w; ++x) {
    wall(x, 0);
    wall(x, h - 1);
  }
  for (int y = 0; y < h; ++y) {
    wall(0, y);
    wall(w - 1, y);
  }

  // Symmetric pillar clusters keep the arena readable and fair.
  const int margin = 3;
  for (int i = 0; i < 5 + wave / 2; ++i) {
    const int cx = rng.irange(margin, w - 1 - margin);
    const int cy = rng.irange(margin, h - 1 - margin);
    const int bw = rng.irange(1, 3);
    const int bh = rng.irange(1, 3);
    for (int y = cy; y < cy + bh; ++y)
      for (int x = cx; x < cx + bw; ++x) wall(x, y);
  }

  // A few crate/blood accents scattered along the floor.
  for (int i = 0; i < 10; ++i) {
    const int x = rng.irange(1, w - 2), y = rng.irange(1, h - 2);
    if (at(x, y).kind != TileKind::Empty) continue;
    at(x, y).floor = rng.chance(0.5f) ? Tex::FloorBlood : Tex::FloorMetal;
  }

  playerStart = cellCenter(w / 2, h / 2);
  if (!inside(w / 2, h / 2) || at(w / 2, h / 2).kind == TileKind::Wall) {
    // Guarantee a clear centre spawn.
    for (int y = h / 2 - 2; y <= h / 2 + 2; ++y)
      for (int x = w / 2 - 2; x <= w / 2 + 2; ++x)
        if (inside(x, y)) at(x, y).kind = TileKind::Empty;
  }
  playerAngle = 0.0f;

  // Decorative lamps in the corners.
  const int lampSpots[4][2] = {{2, 2}, {w - 3, 2}, {2, h - 3}, {w - 3, h - 3}};
  for (const auto& spot : lampSpots) {
    if (inside(spot[0], spot[1]) && at(spot[0], spot[1]).kind == TileKind::Empty) {
      spawns.push_back({SpawnKind::Lamp, cellCenter(spot[0], spot[1]), 0.0f});
    }
  }
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------
bool Level::isWall(int x, int y) const {
  if (!inside(x, y)) return true;
  return at(x, y).kind == TileKind::Wall;
}

int Level::doorIndexAt(int x, int y) const {
  if (!inside(x, y)) return -1;
  return at(x, y).doorIndex;
}

float Level::doorOpen(int x, int y) const {
  const int di = doorIndexAt(x, y);
  if (di < 0) return 1.0f;
  return doors[size_t(di)].open;
}

bool Level::isSolid(int x, int y) const {
  if (!inside(x, y)) return true;
  const Tile& t = at(x, y);
  if (t.kind == TileKind::Wall) return true;
  if (t.kind == TileKind::Door || t.kind == TileKind::ExitDoor) {
    return doorOpen(x, y) < 0.75f;
  }
  return false;
}

bool Level::blocksShots(int x, int y) const {
  if (!inside(x, y)) return true;
  const Tile& t = at(x, y);
  if (t.kind == TileKind::Wall) return true;
  if (t.kind == TileKind::Door || t.kind == TileKind::ExitDoor) {
    return doorOpen(x, y) < 0.6f;
  }
  return false;
}

bool Level::blocksSight(int x, int y) const {
  if (!inside(x, y)) return true;
  const Tile& t = at(x, y);
  if (t.kind == TileKind::Wall) return true;
  if (t.kind == TileKind::Door || t.kind == TileKind::ExitDoor) {
    return doorOpen(x, y) < 0.85f;
  }
  return false;
}

bool Level::collides(const Vec2& center, float radius) const {
  const int x0 = int(std::floor(center.x - radius));
  const int x1 = int(std::floor(center.x + radius));
  const int y0 = int(std::floor(center.y - radius));
  const int y1 = int(std::floor(center.y + radius));
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      if (!isSolid(x, y)) continue;
      const float cx = clampf(center.x, float(x), float(x + 1));
      const float cy = clampf(center.y, float(y), float(y + 1));
      const float dx = center.x - cx, dy = center.y - cy;
      if (dx * dx + dy * dy < radius * radius) return true;
    }
  }
  return false;
}

RayHit Level::rayCast(const Vec2& origin, const Vec2& dir, float maxDist) const {
  RayHit hit;
  const float len = length(dir);
  if (len < 1e-6f) return hit;
  const Vec2 d = dir / len;

  int mapX = int(std::floor(origin.x));
  int mapY = int(std::floor(origin.y));

  const float deltaX = (std::abs(d.x) < 1e-8f) ? 1e30f : std::abs(1.0f / d.x);
  const float deltaY = (std::abs(d.y) < 1e-8f) ? 1e30f : std::abs(1.0f / d.y);
  const int stepX = d.x < 0.0f ? -1 : 1;
  const int stepY = d.y < 0.0f ? -1 : 1;

  float sideDistX = (d.x < 0.0f) ? (origin.x - float(mapX)) * deltaX
                                 : (float(mapX + 1) - origin.x) * deltaX;
  float sideDistY = (d.y < 0.0f) ? (origin.y - float(mapY)) * deltaY
                                 : (float(mapY + 1) - origin.y) * deltaY;

  float dist = 0.0f;
  bool sideX = false;
  for (int guard = 0; guard < 4096; ++guard) {
    sideX = sideDistX < sideDistY;
    if (sideX) {
      dist = sideDistX;
      sideDistX += deltaX;
      mapX += stepX;
    } else {
      dist = sideDistY;
      sideDistY += deltaY;
      mapY += stepY;
    }
    if (dist > maxDist) break;
    if (!inside(mapX, mapY)) {
      hit.hit = true;
      hit.dist = dist;
      hit.cx = mapX;
      hit.cy = mapY;
      return hit;
    }
    const Tile& t = at(mapX, mapY);
    const bool isDoor = (t.kind == TileKind::Door || t.kind == TileKind::ExitDoor);
    if (isDoor && doorOpen(mapX, mapY) >= 0.85f) continue;  // ray slips through an open door
    if (t.kind == TileKind::Wall || isDoor) {
      hit.hit = true;
      hit.dist = dist;
      hit.cx = mapX;
      hit.cy = mapY;
      hit.door = isDoor;
      hit.doorIndex = isDoor ? t.doorIndex : -1;
      const float px = origin.x + d.x * dist;
      const float py = origin.y + d.y * dist;
      hit.u = sideX ? (py - std::floor(py)) : (px - std::floor(px));
      return hit;
    }
  }
  return hit;
}

bool Level::lineOfSight(const Vec2& a, const Vec2& b) const {
  const Vec2 delta = b - a;
  const float dist = length(delta);
  if (dist < 1e-4f) return true;
  const RayHit hit = rayCast(a, delta, dist - 0.02f);
  if (!hit.hit) return true;
  return hit.dist >= dist - 0.05f;
}

// ---------------------------------------------------------------------------
// Doors
// ---------------------------------------------------------------------------
bool Level::tryOpenDoor(int x, int y, bool byPlayer, bool hasKey, std::string* message) {
  const int di = doorIndexAt(x, y);
  if (di < 0) return false;
  Door& d = doors[size_t(di)];

  if (d.kind == DoorKind::Locked && d.locked) {
    if (!hasKey) {
      if (message) *message = byPlayer ? "YOU NEED A RED KEYCARD" : "";
      return false;
    }
    d.locked = false;
    if (message) *message = "KEYCARD ACCEPTED";
  }
  if (d.kind == DoorKind::Exit && d.locked) {
    if (message) *message = "THE EXIT IS SEALED";
    return false;
  }
  if (d.fullyOpen || d.moving == false) {
    d.moving = true;
    d.fullyOpen = false;
    d.holdTimer = 4.0f;
    return true;
  }
  return false;
}

int Level::countSpawns(SpawnKind kind) const {
  int n = 0;
  for (const EntitySpawn& s : spawns) {
    if (s.kind == kind) ++n;
  }
  return n;
}

void Level::updateDoors(float dt, const std::vector<Vec2>& actors) {
  constexpr float kSpeed = 1.8f;
  for (Door& d : doors) {
    const Vec2 centre = cellCenter(d.cx, d.cy);
    bool occupied = false;
    for (const Vec2& a : actors) {
      if (distance(a, centre) < 0.8f) {
        occupied = true;
        break;
      }
    }

    if (d.moving) {
      if (d.fullyOpen) {  // currently closing
        if (occupied) {
          d.fullyOpen = false;  // reverse and reopen
        } else {
          d.open -= kSpeed * dt;
          if (d.open <= 0.0f) {
            d.open = 0.0f;
            d.moving = false;
          }
        }
      } else {  // currently opening
        d.open += kSpeed * dt;
        if (d.open >= 1.0f) {
          d.open = 1.0f;
          d.fullyOpen = true;
          d.moving = false;
          d.holdTimer = 4.0f;
        }
      }
    } else if (d.fullyOpen && !d.locked) {
      if (occupied) {
        d.holdTimer = 1.5f;
      } else {
        d.holdTimer -= dt;
        if (d.holdTimer <= 0.0f) d.moving = true;
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Pathfinding
// ---------------------------------------------------------------------------
void Level::computeFlow(const Vec2& target) {
  flow_.assign(size_t(w) * size_t(h), -1);
  if (w <= 0 || h <= 0) return;

  int sx = 0, sy = 0;
  cellOf(target, sx, sy);
  if (!inside(sx, sy)) return;
  if (at(sx, sy).kind == TileKind::Wall) {
    // Snap to a neighbouring open cell so enemies still converge on the player.
    bool found = false;
    const int offsets[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    for (const auto& o : offsets) {
      if (inside(sx + o[0], sy + o[1]) && at(sx + o[0], sy + o[1]).kind != TileKind::Wall) {
        sx += o[0];
        sy += o[1];
        found = true;
        break;
      }
    }
    if (!found) return;
  }

  auto idx = [&](int x, int y) { return size_t(y) * size_t(w) + size_t(x); };
  auto walkable = [&](int x, int y) {
    if (!inside(x, y)) return false;
    // Doors count as walkable: enemies push them open on contact.
    return at(x, y).kind != TileKind::Wall;
  };

  std::deque<int> queue;
  flow_[idx(sx, sy)] = 0;
  queue.push_back(int(idx(sx, sy)));

  const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
  const int dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};

  while (!queue.empty()) {
    const int cur = queue.front();
    queue.pop_front();
    const int cx = cur % w;
    const int cy = cur / w;
    const int base = flow_[size_t(cur)];
    for (int k = 0; k < 8; ++k) {
      const int nx = cx + dx[k], ny = cy + dy[k];
      if (!walkable(nx, ny)) continue;
      const size_t ni = idx(nx, ny);
      if (flow_[ni] != -1) continue;
      // Diagonals must not squeeze through wall corners.
      if (dx[k] != 0 && dy[k] != 0) {
        if (!walkable(cx + dx[k], cy) || !walkable(cx, cy + dy[k])) continue;
      }
      flow_[ni] = base + 1;
      queue.push_back(int(ni));
    }
  }
}

int Level::flowAt(int x, int y) const {
  if (!inside(x, y) || flow_.empty()) return -1;
  return flow_[size_t(y) * size_t(w) + size_t(x)];
}

bool Level::flowStep(int fromX, int fromY, int& outX, int& outY) const {
  const int here = flowAt(fromX, fromY);
  if (here <= 0) return false;
  const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
  const int dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
  int best = here;
  int bestX = -1, bestY = -1;
  for (int k = 0; k < 8; ++k) {
    const int nx = fromX + dx[k], ny = fromY + dy[k];
    if (!inside(nx, ny)) continue;
    if (at(nx, ny).kind == TileKind::Wall) continue;
    if (dx[k] != 0 && dy[k] != 0) {
      // No corner cutting.
      if (at(fromX + dx[k], fromY).kind == TileKind::Wall) continue;
      if (at(fromX, fromY + dy[k]).kind == TileKind::Wall) continue;
    }
    const int v = flowAt(nx, ny);
    if (v >= 0 && v < best) {
      best = v;
      bestX = nx;
      bestY = ny;
    }
  }
  if (bestX < 0) return false;
  outX = bestX;
  outY = bestY;
  return true;
}

Vec2 Level::randomOpenCell(Rng& rng, float minDistFrom, const Vec2& avoid) const {
  for (int attempt = 0; attempt < 400; ++attempt) {
    const int x = rng.irange(1, std::max(1, w - 2));
    const int y = rng.irange(1, std::max(1, h - 2));
    if (!inside(x, y)) continue;
    if (at(x, y).kind != TileKind::Empty) continue;
    const Vec2 c = cellCenter(x, y);
    if (collides(c, 0.35f)) continue;
    if (minDistFrom > 0.0f && distance(c, avoid) < minDistFrom) continue;
    return c;
  }
  return playerStart;
}

}  // namespace fps
