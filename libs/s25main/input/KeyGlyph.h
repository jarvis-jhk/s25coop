// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DrawPoint.h"
#include "input/PlayerBrief.h"
#include <functional>

class glFont;

namespace brief {

/// A measured label, optionally on a code-drawn button badge. Labels retain the
/// text identity of neutral buttons and distinguish stick motion from L3.
struct KeyRun
{
    std::string text;
    unsigned x = 0;
    unsigned width = 0;
    unsigned textColor = 0;
    unsigned badgeColor = 0;
};
struct KeyGlyphLine
{
    std::string text;
    std::vector<KeyRun> runs;
};

constexpr unsigned neutralBadgeColor = 0xFF455361;
constexpr unsigned keyTextPadding = 3;
unsigned KeyBadgeColor(const KeyHint& input);

/// Keep inputs with their action when possible. An oversized group uses the
/// ordinary font wrap, preserving every label in small views or long translations.
std::vector<KeyGlyphLine> LayoutKeyGlyphs(const std::vector<KeyHint>& keys, const glFont& font, unsigned short width,
                                          unsigned textColor);

/// The same geometry emitter is used by the desktop and by renderer-contract tests.
void EmitKeyBadge(const Rect& rect, unsigned color, const std::function<void(const Rect&, unsigned)>& emit);

/// Draw measured runs at `pos`: badges through `drawRect`, text with the font. The in-game brief and the
/// front-end footer both draw through this, so a key looks the same in the menu and in the game.
void DrawKeyRuns(const DrawPoint& pos, const std::vector<KeyRun>& runs, const glFont& font,
                 const std::function<void(const Rect&, unsigned)>& drawRect);

} // namespace brief
