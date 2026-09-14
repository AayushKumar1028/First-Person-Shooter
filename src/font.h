// font.h - tiny 5x7 bitmap font, drawn into either a Framebuffer (HUD/menus)
// or a Texture (so signs baked into wall textures can use text too).
#pragma once

#include "framebuffer.h"
#include "image.h"

namespace fps {

constexpr int kGlyphW = 5;
constexpr int kGlyphH = 7;

// Row dump of one 5x7 glyph; each byte uses bits 4..0 for left..right.
const uint8_t* fontGlyph(char c);

inline int fontAdvance(int scale, int spacing) { return (kGlyphW + spacing) * scale; }

int fontTextWidth(const std::string& text, int scale = 1, int spacing = 1);
inline int fontTextHeight(int scale = 1) { return kGlyphH * scale; }

void fontDraw(Framebuffer& fb, int x, int y, const std::string& text, RGBA color, int scale = 1,
              int spacing = 1);

// Draws with a 1px (scaled) dark outline - keeps text readable over any backdrop.
void fontDrawOutlined(Framebuffer& fb, int x, int y, const std::string& text, RGBA color,
                      RGBA outline = 0xFF000000u, int scale = 1, int spacing = 1);

// Centres "text" horizontally inside [x, x+width).
void fontDrawCentered(Framebuffer& fb, int x, int y, int width, const std::string& text,
                      RGBA color, int scale = 1, int spacing = 1);

void fontDrawTexture(Texture& tex, int x, int y, const std::string& text, RGBA color, int scale = 1,
                     int spacing = 1);

}  // namespace fps
