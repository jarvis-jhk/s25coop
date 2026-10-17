// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DrawPoint.h"
#include "input/PlayerBrief.h"
#include <functional>

class glFont;
class glArchivItem_Bitmap;

namespace brief {

/// A measured label, optionally on a code-drawn button badge or drawn as a pixel-art
/// button image. Labels retain the text identity of neutral buttons and distinguish
/// stick motion from L3; `text` stays filled when a glyph is drawn instead.
struct KeyRun
{
    std::string text;
    unsigned x = 0;
    unsigned width = 0;
    unsigned textColor = 0;
    unsigned badgeColor = 0;
    glArchivItem_Bitmap* glyph = nullptr;
};
struct KeyGlyphLine
{
    std::string text;
    std::vector<KeyRun> runs;
};

constexpr unsigned neutralBadgeColor = 0xFF455361;
constexpr unsigned keyTextPadding = 3;
unsigned KeyBadgeColor(const KeyHint& input);

/// Image indices in the archive "pad_glyphs" (data/RTTR/assets/base/pad_glyphs, made by
/// tools/coop/padglyphs.py - keep the order in sync). The D-pad has one image per set of
/// lit directions: padGlyphDpad - 1 + mask, bit 0 up, 1 down, 2 left, 3 right.
enum PadGlyph : unsigned
{
    padGlyphA,
    padGlyphB,
    padGlyphX,
    padGlyphY,
    padGlyphLB,
    padGlyphRB,
    padGlyphLT,
    padGlyphRT,
    padGlyphView,
    padGlyphMenu,
    padGlyphGuide,
    padGlyphLeftStick,
    padGlyphRightStick,
    padGlyphL3,
    padGlyphR3,
    padGlyphDpad,
    padGlyphCount = padGlyphDpad + 15
};
/// D-pad direction bit of an input, 0 for everything that is not a D-pad direction.
unsigned DpadBit(const KeyHint& input);
/// Image index of a single input (a D-pad direction maps to its one-direction image).
unsigned PadGlyphIndex(const KeyHint& input);

/// Where the images come from: LOADER's "pad_glyphs" by default, a test archive in tests.
/// Returning nullptr for an index makes that input fall back to its text badge.
using GlyphLookup = std::function<glArchivItem_Bitmap*(unsigned)>;
glArchivItem_Bitmap* LoaderGlyph(unsigned index);

/// Keep inputs with their action when possible. An oversized group uses the
/// ordinary font wrap, preserving every label in small views or long translations.
/// Inputs with an image are drawn as that image; the D-pad directions of one group share one image.
std::vector<KeyGlyphLine> LayoutKeyGlyphs(const std::vector<KeyHint>& keys, const glFont& font, unsigned short width,
                                          unsigned textColor, const GlyphLookup& glyphs = LoaderGlyph);

/// The same geometry emitter is used by the desktop and by renderer-contract tests.
void EmitKeyBadge(const Rect& rect, unsigned color, const std::function<void(const Rect&, unsigned)>& emit);

/// Draw measured runs at `pos`: badges through `drawRect`, text with the font. The in-game brief and the
/// front-end footer both draw through this, so a key looks the same in the menu and in the game.
void DrawKeyRuns(const DrawPoint& pos, const std::vector<KeyRun>& runs, const glFont& font,
                 const std::function<void(const Rect&, unsigned)>& drawRect);

} // namespace brief
