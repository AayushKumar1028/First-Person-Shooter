// levels.cpp - the campaign. See level.cpp for the full ASCII legend.
//
// Layout convention: the outer border is walls, and each level ends with a 'Z'
// exit door set into the bottom wall. Levels grow in size and enemy count.
#include "levels.h"

namespace fps {

namespace {

std::vector<std::string> splitLines(const char* text) {
  std::vector<std::string> rows;
  std::string current;
  for (const char* p = text; *p; ++p) {
    if (*p == '\n' || *p == '\r') {
      if (!current.empty()) rows.push_back(current);
      current.clear();
    } else {
      current.push_back(*p);
    }
  }
  if (!current.empty()) rows.push_back(current);
  return rows;
}

// --- level 1: a compact tech base, one keycard, five hostiles --------------
const char* kLevel1 = R"(
################################
#........#..........#..........#
#..P.....#..........#..........#
#....w...#..........#.......z..#
#........#...%%%%...#....o.....#
#........#...%..%...#..........#
#........#...%..%...#....K.....#
#........#...%%%%...#.....h....#
#....z...#..........#......o...#
####.#####..........#..........#
#........#..........#..........#
#........#..........#..........#
#........D..........D..........#
#........#.....z....#..........#
#........#.....i....#.....i....#
#...o....#..........#..........#
#....n...#..........#..........#
#..h.....#..........#.....m....#
#........#....z.....#..z.......#
##############L#################
#....h....o.......k......V.....#
##############Z#################
)";

// --- level 2: refinery, tighter corridors, locked vault --------------------
const char* kLevel2 = R"(
##################################
#..........#..........#..........#
#..P.......#..........#..........#
#..........#..........#....w.....#
#....o.....#..........#..........#
#..........#.....K....#..........#
#......z...#..z.......#..........#
#..........#..........#....n.....#
#####.######..........#..........#
#..........#....z.....#.....n....#
#..........#....i.....#..........#
#..........D..........D..........#
#..........#..........#..........#
#..........#...%%%%...#.....o....#
#..........#...%..%...#..........#
#..........#...%..%...#.....m....#
#..........#...%%%%...#..........#
#....z.....#..........#...i......#
#..........#..........#..........#
###########L#########L############
#................................#
#....h......k.....V.....n....H...#
#..............B.................#
#################Z################
)";

// --- level 3: the finale, wide arena with a gated exit --------------------
const char* kLevel3 = R"(
####################################
#..........#............#..........#
#..P.......#............#......h...#
#..........#............D..........#
#....o.....#....%%%%....#....o.....#
#..........#....%..%....#..........#
#..........#....%..%....#.....B....#
#..........#....%%%%....#..........#
#..........#............#....v.....#
#####.######............#..........#
#..........#....i.......#..........#
#..........#............#....q.....#
#..........#............D....e.....#
#..........#......i.....#..........#
#..........#....%%%%....#.....j....#
#..........#....%..%....#..........#
#..........#....%..%....#....V.....#
#..........#.....B......#..........#
#..........#....B.......#..........#
#..........#............#....i.....#
#....z.....#............#..........#
#..........#......z.....#....k.....#
#..........#............#.....z....#
#..........L............D.....n....#
#....K.....#............#....i.....#
################Z###################
)";

void addLevel(std::vector<CampaignLevel>& out, const char* name, const char* intro, const char* map,
              Tex floor, Tex ceil, float angle) {
  CampaignLevel level;
  level.name = name;
  level.intro = intro;
  level.rows = splitLines(map);
  level.floor = floor;
  level.ceil = ceil;
  level.playerAngle = angle;
  out.push_back(std::move(level));
}

}  // namespace

std::vector<CampaignLevel> buildCampaign() {
  std::vector<CampaignLevel> levels;
  addLevel(levels, "DOCKING BAY", "FIND THE EXIT - CLEAR THE SECTOR", kLevel1, Tex::FloorCobble,
           Tex::CeilStone, 0.0f);
  addLevel(levels, "REFINERY", "RECOVER THE KEYCARD - WATCH THE CORNERS", kLevel2, Tex::FloorMetal,
           Tex::CeilRust, 0.0f);
  addLevel(levels, "THE PIT", "EVERYTHING WANTS YOU DEAD", kLevel3, Tex::FloorTech,
           Tex::CeilTech, 0.0f);
  return levels;
}

}  // namespace fps
