// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DrawPoint.h"
#include "Rect.h"
#include <vector>

/// Geometry of the new front-end pages (doc/coop/FrontEnd.md, slice F1).
///
/// Pure arithmetic on render-size rectangles: no Window, no font, no driver, so every screen size
/// (800×600, the Deck's 1024×640 at 125 %, a 4K TV) can be checked without OpenGL. The pages lay
/// themselves out in actual render units instead of stretching an 800×600 design like the old
/// desktops do, which distorts everything on the Deck's 16:10 screen.
namespace frontend {

/// Space around the frame and between its parts.
constexpr unsigned pageMargin = 16;
/// Header: page title, back button and the joined-player strip.
constexpr unsigned headerHeight = 48;
/// Content never gets wider than this; a 4K TV gets a centred column instead of tiles a metre apart.
constexpr unsigned maxContentWidth = 1100;
/// Default gap between tiles or list rows.
constexpr unsigned itemGap = 12;

struct PageFrame
{
    Rect header;
    Rect content;
    Rect footer;
};

/// Split a page of the given size into header, content and footer.
/// `footerLines` × `lineHeight` is the room the help line may use (it wraps on narrow screens).
PageFrame LayoutFrame(const Extent& size, unsigned footerLines, unsigned lineHeight);

/// A grid of equal tiles with the given aspect ratio, as large as fits into `area`.
struct TileGrid
{
    unsigned columns = 0;
    unsigned rows = 0;
    std::vector<Rect> tiles;
};
/// Tiles keep `aspect` (only the ratio matters) and never exceed `maxTile`. The grid is centred in
/// `area`. Every tile is placed even when `area` is too small; tiles then shrink to 1×1 and the gaps may
/// overflow. An empty `aspect` gives no tiles.
TileGrid LayoutTiles(const Rect& area, unsigned count, const Extent& aspect, const Extent& maxTile,
                     unsigned gap = itemGap);

/// One full-width row per item from the top of `area`, at most `maxWidth` wide and centred.
/// Rows shrink (down to a third of `rowHeight`) when there are too many to fit.
std::vector<Rect> LayoutList(const Rect& area, unsigned count, unsigned rowHeight, unsigned maxWidth,
                             unsigned gap = itemGap / 2);

/// Badges of the joined-player strip, right-aligned in the header, in slot order.
std::vector<Rect> LayoutPlayerStrip(const Rect& header, unsigned count, const Extent& badge, unsigned gap = 4);

} // namespace frontend
