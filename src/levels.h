// levels.h - the hand-authored single-player campaign.
#pragma once

#include <string>
#include <vector>

#include "level.h"

namespace fps {

struct CampaignLevel {
  std::string name;
  std::string intro;
  std::vector<std::string> rows;
  Tex floor = Tex::FloorStone;
  Tex ceil = Tex::CeilStone;
  float playerAngle = 0.0f;
};

// Three levels, increasing in size and difficulty.
std::vector<CampaignLevel> buildCampaign();

}  // namespace fps
