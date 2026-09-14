// hud.cpp - everything drawn on top of the 3D view.
#include "hud.h"

#include <algorithm>

#include "font.h"

namespace fps {

namespace {

RGBA tintColor(RGBA c, RGBA tint) {
  if (tint == 0xFFFFFFFFu) return c;
  return rgba(redOf(c) * redOf(tint) / 255, greenOf(c) * greenOf(tint) / 255,
              blueOf(c) * blueOf(tint) / 255, alphaOf(c));
}

void panel(Framebuffer& fb, int x, int y, int w, int h, RGBA face) {
  fb.fillRect(x, y, w, h, face);
  fb.hline(x, y, w, ui::kBevelLight);
  fb.vline(x, y, h, ui::kBevelLight);
  fb.hline(x, y + h - 1, w, ui::kBevelDark);
  fb.vline(x + w - 1, y, h, ui::kBevelDark);
}

// Big Doom-style status-bar face that reacts to health.
void drawFace(Framebuffer& fb, int x, int y, int size, int health, bool dead, float time) {
  const int u = std::max(1, size / 16);  // "pixel" unit
  auto px = [&](int gx, int gy, int gw, int gh, RGBA c) {
    fb.fillRect(x + gx * u, y + gy * u, gw * u, gh * u, c);
  };
  const bool hurt = health <= 40;
  const bool critical = health <= 20;
  const RGBA skin = dead ? rgba(120, 110, 100) : (hurt ? rgba(196, 150, 120) : rgba(214, 176, 140));
  const RGBA helmet = rgba(84, 92, 72);
  const RGBA shadeSkin = shade(skin, 0.72f);

  px(0, 0, 16, 16, ui::kPanel);
  // Helmet.
  px(3, 1, 10, 3, helmet);
  px(2, 2, 12, 2, shade(helmet, 1.15f));
  // Face.
  px(4, 4, 8, 9, skin);
  px(4, 4, 8, 1, shade(skin, 1.1f));
  px(3, 5, 1, 6, shadeSkin);
  px(12, 5, 1, 6, shadeSkin);
  // Eyes (blink occasionally, X when dead).
  const bool blink = !dead && std::fmod(time, 3.4f) < 0.12f;
  if (dead) {
    px(5, 6, 2, 1, rgba(40, 30, 30));
    px(5, 7, 1, 1, rgba(40, 30, 30));
    px(9, 6, 2, 1, rgba(40, 30, 30));
  } else if (blink) {
    px(5, 7, 2, 1, rgba(60, 50, 45));
    px(9, 7, 2, 1, rgba(60, 50, 45));
  } else {
    px(5, 6, 2, 2, rgba(240, 240, 235));
    px(9, 6, 2, 2, rgba(240, 240, 235));
    px(5, 6, 1, 1, rgba(40, 40, 60));
    px(9, 6, 1, 1, rgba(40, 40, 60));
  }
  // Mouth.
  if (critical && !dead) {
    px(6, 10, 4, 2, rgba(90, 40, 40));
  } else if (hurt && !dead) {
    px(5, 11, 6, 1, rgba(90, 50, 50));
  } else if (!dead) {
    px(6, 11, 4, 1, rgba(90, 60, 60));
  }
  // Blood smears as the marine takes a beating.
  if (hurt) {
    px(4, 5, 1, 2, rgba(150, 24, 20));
    px(11, 8, 1, 2, rgba(150, 24, 24));
  }
  if (critical) {
    px(5, 9, 2, 1, rgba(160, 28, 22));
    px(10, 4, 1, 3, rgba(160, 28, 22));
    px(6, 12, 3, 1, rgba(140, 22, 20));
  }
}

void drawKeyIcon(Framebuffer& fb, int x, int y, int size) {
  const int u = std::max(1, size / 8);
  fb.fillRect(x, y + u, 2 * u, u, ui::kDanger);
  fb.fillRect(x + 2 * u, y, 4 * u, 3 * u, ui::kDanger);
  fb.fillRect(x + 4 * u, y + u, u, u, ui::kPanel);
  fb.fillRect(x, y, u, 3 * u, ui::kDanger);
}

void drawMenuArrow(Framebuffer& fb, int x, int y, int size, RGBA c) {
  for (int i = 0; i < size; ++i) {
    fb.fillRect(x + i, y + i, 1, std::max(1, (size - i) * 2 - 1), c);
  }
}

// Builds "012" style right-aligned numbers.
std::string pad(int value, int width) {
  std::string s = std::to_string(std::max(0, value));
  while (int(s.size()) < width) s = "0" + s;
  if (int(s.size()) > width) s = s.substr(s.size() - size_t(width));
  return s;
}

}  // namespace

void blitScaled(Framebuffer& fb, const Texture& tex, int dstX, int dstY, int dstW, int dstH,
                float alpha, RGBA tint) {
  if (tex.empty() || dstW <= 0 || dstH <= 0 || alpha <= 0.0f) return;
  for (int y = 0; y < dstH; ++y) {
    const int fy = dstY + y;
    if (fy < 0 || fy >= fb.h) continue;
    const int ty = clampi(y * tex.h / dstH, 0, tex.h - 1);
    const RGBA* src = tex.px.data() + size_t(ty) * size_t(tex.w);
    for (int x = 0; x < dstW; ++x) {
      const int fx = dstX + x;
      if (fx < 0 || fx >= fb.w) continue;
      const int tx = clampi(x * tex.w / dstW, 0, tex.w - 1);
      const RGBA c = src[tx];
      const int a = alphaOf(c);
      if (a == 0) continue;
      const RGBA lit = tintColor(c, tint);
      if (a == 255 && alpha >= 0.999f) {
        fb.set(fx, fy, lit);
      } else {
        fb.blend(fx, fy, lit, (float(a) / 255.0f) * alpha);
      }
    }
  }
}

void drawCrosshair(Framebuffer& fb, float spreadPixels) {
  const int cx = fb.w / 2;
  const int cy = fb.h / 2;
  const int gap = 2 + int(clampf(spreadPixels, 0.0f, 6.0f));
  const int len = std::max(2, fb.h / 90);
  const RGBA c = rgba(220, 240, 220, 190);
  const RGBA edge = rgba(20, 24, 20, 160);
  auto arm = [&](int x, int y, int w, int h) {
    fb.fillRect(x, y, w, h, edge);
    fb.fillRect(x + 1, y + 1, std::max(1, w - 2), std::max(1, h - 2), c);
  };
  arm(cx - gap - len, cy, len, 2);
  arm(cx + gap, cy, len, 2);
  arm(cx, cy - gap - len, 2, len);
  arm(cx, cy + gap, 2, len);
  fb.fillRect(cx, cy, 1, 1, c);
}

void drawViewModel(Framebuffer& fb, const Texture& view, const Player& player) {
  if (view.empty()) return;
  const int targetH = int(float(fb.h) * 0.46f);
  const int targetW = int(float(targetH) * float(view.w) / float(view.h));
  const float bobX = std::sin(player.bobPhase) * float(fb.w) * 0.013f * player.bobAmount;
  const float bobY = std::abs(std::cos(player.bobPhase)) * float(fb.h) * 0.016f * player.bobAmount;
  const float recoil = player.muzzleFlash * float(fb.h) * 0.05f;
  int x = (fb.w - targetW) / 2 + int(bobX);
  int y = fb.h - int(float(fb.h) * 0.055f) - targetH + int(bobY) + int(recoil);
  if (player.dead()) y += int(player.deathTime * float(fb.h) * 0.35f);
  blitScaled(fb, view, x, y, targetW, targetH);
}

void drawHud(Framebuffer& fb, const Assets& assets, const HudInfo& info) {
  const Player& p = *info.player;
  const int barH = clampi(fb.h / 6, 30, 72);
  const int top = fb.h - barH;
  panel(fb, 0, top, fb.w, barH, ui::kPanel);

  const int s = clampi(barH / 26, 1, 3);
  const int labelScale = s;
  const int valueScale = clampi(barH / 18, 2, 5);
  const int pad6 = 6 * s;

  // Column layout, in fractions of the bar width.
  const int colW = fb.w / 5;

  auto drawStat = [&](int col, const char* label, const std::string& value, RGBA valueColor,
                      const char* sub) {
    const int x = col * colW;
    fontDraw(fb, x + pad6, top + 4 * s, label, ui::kTextDim, labelScale, 1);
    fontDrawOutlined(fb, x + pad6, top + 4 * s + 9 * s, value, valueColor, ui::kBevelDark, valueScale,
                     1);
    if (sub && *sub) fontDraw(fb, x + pad6, top + barH - 9 * s, sub, ui::kTextDim, labelScale, 1);
  };

  // --- ammo -------------------------------------------------------------
  const WeaponDef& weapon = weaponDef(p.weapon);
  drawStat(0, "AMMO", pad(p.ammo[int(weapon.ammo)], 3), ui::kAccent, ammoName(weapon.ammo));

  // --- health -----------------------------------------------------------
  const RGBA healthColor = p.health > 60 ? ui::kHealth
                                          : (p.health > 25 ? ui::kAccent : ui::kDanger);
  drawStat(1, "HEALTH", pad(p.health, 3), healthColor, "%");

  // --- face + kills -----------------------------------------------------
  {
    const int faceSize = barH - 10 * s;
    const int fx = 2 * colW + (colW - faceSize) / 2;
    drawFace(fb, fx, top + 5 * s, faceSize, p.health, p.dead(), info.elapsed);
  }

  // --- armor ------------------------------------------------------------
  drawStat(3, "ARMOR", pad(p.armor, 3), ui::kArmor, "+");

  // --- weapon + key + progress -----------------------------------------
  {
    const int x = 4 * colW;
    fontDraw(fb, x + pad6, top + 4 * s, "WEAPON", ui::kTextDim, labelScale, 1);
    fontDraw(fb, x + pad6, top + 4 * s + 9 * s, weapon.name, ui::kText, labelScale, 1);

    std::string right = info.arena ? ("WAVE " + std::to_string(info.wave))
                                   : (info.level->name + " " + std::to_string(info.levelIndex + 1) +
                                      "/" + std::to_string(info.levelCount));
    fontDraw(fb, x + pad6, top + barH - 9 * s, right, ui::kTextDim, labelScale, 1);

    if (p.hasRedKey) drawKeyIcon(fb, x + colW - 12 * s - pad6, top + 6 * s, 8 * s);
  }

  // Kill/hostile counter under the health column.
  {
    const std::string counter = info.arena
                                    ? ("KILLS " + std::to_string(p.kills))
                                    : ("LEFT " + std::to_string(info.hostilesLeft));
    const RGBA color = (!info.arena && info.hostilesLeft == 0) ? ui::kHealth : ui::kTextDim;
    fontDraw(fb, fb.w / 5 + pad6, top + barH - 9 * s, counter, color, labelScale, 1);
  }
}

void drawScreenEffects(Framebuffer& fb, const Player& player) {
  if (player.damageFlash > 0.005f) {
    fb.fillRectBlend(0, 0, fb.w, fb.h, rgba(200, 20, 16),
                     clampf(player.damageFlash, 0.0f, 1.0f) * 0.38f);
  }
  if (player.pickupFlash > 0.005f) {
    fb.fillRectBlend(0, 0, fb.w, fb.h, rgba(240, 200, 90),
                     clampf(player.pickupFlash, 0.0f, 1.0f) * 0.14f);
  }
  if (player.dead()) {
    fb.fillRectBlend(0, 0, fb.w, fb.h, rgba(150, 10, 10),
                     clampf(player.deathTime * 0.5f, 0.0f, 0.45f));
  }
  // Subtle vignette keeps the eye on the centre of the screen.
  const int edge = std::max(4, fb.h / 22);
  for (int i = 0; i < edge; ++i) {
    const float a = 0.30f * (1.0f - float(i) / float(edge));
    fb.hline(0, i, fb.w, rgba(0, 0, 0, int(255 * a)));
    fb.hline(0, fb.h - 1 - i, fb.w, rgba(0, 0, 0, int(255 * a)));
    fb.vline(i, 0, fb.h, rgba(0, 0, 0, int(255 * a)));
    fb.vline(fb.w - 1 - i, 0, fb.h, rgba(0, 0, 0, int(255 * a)));
  }
}

void drawMessage(Framebuffer& fb, const std::string& text, float alpha) {
  if (text.empty() || alpha <= 0.01f) return;
  const int scale = clampi(fb.h / 110, 1, 3);
  const int tw = fontTextWidth(text, scale, 1);
  const int x = (fb.w - tw) / 2;
  const int y = fb.h - clampi(fb.h / 6, 30, 72) - fontTextHeight(scale) - 8 * scale;
  const RGBA color = rgba(240, 230, 200, int(255 * clampf(alpha, 0.0f, 1.0f)));
  const RGBA shadow = rgba(0, 0, 0, int(200 * clampf(alpha, 0.0f, 1.0f)));
  fontDraw(fb, x + scale, y + scale, text, shadow, scale, 1);
  fontDraw(fb, x, y, text, color, scale, 1);
}

void drawBigCenterText(Framebuffer& fb, const std::string& title, const std::string& subtitle,
                       RGBA titleColor) {
  fb.fillRectBlend(0, 0, fb.w, fb.h, rgba(0, 0, 0), 0.55f);
  const int titleScale = clampi(fb.h / 45, 2, 6);
  const int subScale = clampi(fb.h / 90, 1, 3);
  const int cx = 0;
  const int titleY = fb.h / 2 - fontTextHeight(titleScale);
  fontDrawOutlined(fb, cx + (fb.w - fontTextWidth(title, titleScale, 1)) / 2, titleY, title,
                   titleColor, ui::kBevelDark, titleScale, 1);
  if (!subtitle.empty()) {
    fontDraw(fb, cx + (fb.w - fontTextWidth(subtitle, subScale, 1)) / 2,
             titleY + fontTextHeight(titleScale) + 12 * subScale, subtitle, ui::kText, subScale, 1);
  }
}

void drawMenu(Framebuffer& fb, const std::string& title, const std::string& subtitle,
              const std::vector<std::string>& items, int selected,
              const std::vector<std::string>& footerLines) {
  fb.fillRectBlend(0, 0, fb.w, fb.h, rgba(4, 4, 8), 0.72f);

  const int titleScale = clampi(fb.h / 40, 2, 6);
  const int itemScale = clampi(fb.h / 85, 1, 3);
  const int footScale = clampi(fb.h / 130, 1, 2);

  int y = std::max(6, fb.h / 12);
  fontDrawOutlined(fb, (fb.w - fontTextWidth(title, titleScale, 1)) / 2, y, title, ui::kAccent,
                   ui::kBevelDark, titleScale, 1);
  y += fontTextHeight(titleScale) + 8 * titleScale;

  if (!subtitle.empty()) {
    fontDraw(fb, (fb.w - fontTextWidth(subtitle, itemScale, 1)) / 2, y, subtitle, ui::kTextDim,
             itemScale, 1);
    y += fontTextHeight(itemScale) + 14 * itemScale;
  }

  const int lineH = fontTextHeight(itemScale) + 10 * itemScale;
  const int blockW = 42 * itemScale * 3;
  const int x = (fb.w - blockW) / 2;
  for (size_t i = 0; i < items.size(); ++i) {
    const bool isSelected = int(i) == selected;
    const RGBA color = isSelected ? ui::kAccent : ui::kText;
    if (isSelected) {
      fb.fillRectBlend(x - 8 * itemScale, y - 3 * itemScale, blockW + 16 * itemScale,
                       fontTextHeight(itemScale) + 6 * itemScale, rgba(220, 160, 48), 0.18f);
      drawMenuArrow(fb, x - 7 * itemScale, y + 1 * itemScale, 3 * itemScale, ui::kAccent);
    }
    fontDraw(fb, x, y, items[i], color, itemScale, 1);
    y += lineH;
  }

  y += 6 * itemScale;
  for (const std::string& line : footerLines) {
    fontDraw(fb, (fb.w - fontTextWidth(line, footScale, 1)) / 2, y, line, ui::kTextDim, footScale, 1);
    y += fontTextHeight(footScale) + 5 * footScale;
  }
}

void drawKeyValueList(Framebuffer& fb, int x, int y, const std::vector<std::string>& lines,
                      RGBA labelColor, int scale) {
  for (const std::string& line : lines) {
    fontDraw(fb, x, y, line, labelColor, scale, 1);
    y += fontTextHeight(scale) + 6 * scale;
  }
}

}  // namespace fps
