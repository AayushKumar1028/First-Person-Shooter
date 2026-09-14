// hud.h - status bar, first-person weapon, menus and screen overlays.
#pragma once

#include <string>
#include <vector>

#include "assets.h"
#include "entities.h"
#include "framebuffer.h"

namespace fps {

struct HudInfo {
  const Player* player = nullptr;
  const Level* level = nullptr;
  int levelIndex = 0;
  int levelCount = 1;
  int wave = 0;          // arena mode only (0 = campaign)
  int score = 0;
  int bestScore = 0;
  bool arena = false;
  int hostilesLeft = 0;
  float elapsed = 0.0f;
};

// Nearest-neighbour scaled blit with alpha and optional colour tint.
void blitScaled(Framebuffer& fb, const Texture& tex, int dstX, int dstY, int dstW, int dstH,
                float alpha = 1.0f, RGBA tint = 0xFFFFFFFFu);

void drawCrosshair(Framebuffer& fb, float spreadPixels);
void drawViewModel(Framebuffer& fb, const Texture& view, const Player& player);
void drawHud(Framebuffer& fb, const Assets& assets, const HudInfo& info);
void drawScreenEffects(Framebuffer& fb, const Player& player);
void drawMessage(Framebuffer& fb, const std::string& text, float alpha);

void drawBigCenterText(Framebuffer& fb, const std::string& title, const std::string& subtitle,
                       RGBA titleColor);
void drawMenu(Framebuffer& fb, const std::string& title, const std::string& subtitle,
              const std::vector<std::string>& items, int selected,
              const std::vector<std::string>& footerLines);
void drawKeyValueList(Framebuffer& fb, int x, int y, const std::vector<std::string>& lines,
                      RGBA labelColor, int scale);

// Shared UI palette so menus and the HUD stay consistent.
namespace ui {
constexpr RGBA kPanel = 0xFF17181Cu;
constexpr RGBA kPanelLight = 0xFF2A2C33u;
constexpr RGBA kBevelLight = 0xFF4A4E58u;
constexpr RGBA kBevelDark = 0xFF0B0C0Eu;
constexpr RGBA kText = 0xFFD8D4C8u;
constexpr RGBA kTextDim = 0xFF8A8778u;
constexpr RGBA kAccent = 0xFFE0A030u;
constexpr RGBA kAccentAlt = 0xFF60C0E0u;
constexpr RGBA kDanger = 0xFFD04030u;
constexpr RGBA kHealth = 0xFF60C060u;
constexpr RGBA kArmor = 0xFF60A0D0u;
}  // namespace ui

}  // namespace fps
