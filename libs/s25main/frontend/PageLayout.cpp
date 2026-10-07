// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "frontend/PageLayout.h"
#include <algorithm>
#include <limits>

namespace frontend {

namespace {
    unsigned clampSub(unsigned a, unsigned b)
    {
        return a > b ? a - b : 0;
    }
} // namespace

PageFrame LayoutFrame(const Extent& size, const unsigned footerLines, const unsigned lineHeight)
{
    PageFrame frame;
    const unsigned footerHeight = std::max(1u, footerLines) * lineHeight + 8;
    frame.header = Rect(DrawPoint(0, 0), Extent(size.x, std::min(headerHeight, size.y)));
    const unsigned footerTop = clampSub(size.y, footerHeight);
    frame.footer = Rect(DrawPoint(0, static_cast<int>(footerTop)), Extent(size.x, size.y - footerTop));

    const unsigned contentTop = frame.header.getSize().y + pageMargin;
    const unsigned contentBottom = clampSub(footerTop, pageMargin);
    const unsigned contentWidth = std::min(clampSub(size.x, 2 * pageMargin), maxContentWidth);
    const unsigned left = (size.x - contentWidth) / 2;
    frame.content = Rect(DrawPoint(static_cast<int>(left), static_cast<int>(contentTop)),
                         Extent(contentWidth, clampSub(contentBottom, contentTop)));
    return frame;
}

TileGrid LayoutTiles(const Rect& area, const unsigned count, const Extent& aspect, const Extent& maxTile,
                     const unsigned gap)
{
    TileGrid grid;
    if(count == 0 || aspect.x == 0 || aspect.y == 0)
        return grid;
    const Extent areaSize = area.getSize();
    // Try every column count and keep the one giving the largest tile. Few items, so brute force is fine.
    double bestScale = -std::numeric_limits<double>::infinity();
    for(unsigned cols = 1; cols <= count; ++cols)
    {
        const unsigned rows = (count + cols - 1) / cols;
        const double cellW = std::max(0.0, (static_cast<double>(areaSize.x) - (cols - 1) * double(gap)) / cols);
        const double cellH = std::max(0.0, (static_cast<double>(areaSize.y) - (rows - 1) * double(gap)) / rows);
        double scale = std::min(cellW / aspect.x, cellH / aspect.y);
        scale = std::min({scale, double(maxTile.x) / aspect.x, double(maxTile.y) / aspect.y});
        // Strictly greater: on a tie the fewer columns win, which keeps a short list in one column.
        if(scale > bestScale)
        {
            bestScale = scale;
            grid.columns = cols;
            grid.rows = rows;
        }
    }
    const auto tileW = static_cast<unsigned>(std::max(1.0, bestScale * aspect.x));
    const auto tileH = static_cast<unsigned>(std::max(1.0, bestScale * aspect.y));
    const unsigned gridW = grid.columns * tileW + (grid.columns - 1) * gap;
    const unsigned gridH = grid.rows * tileH + (grid.rows - 1) * gap;
    const DrawPoint origin =
      area.getOrigin()
      + DrawPoint(static_cast<int>(clampSub(areaSize.x, gridW) / 2), static_cast<int>(clampSub(areaSize.y, gridH) / 2));
    for(unsigned i = 0; i < count; ++i)
    {
        const unsigned col = i % grid.columns;
        const unsigned row = i / grid.columns;
        // A short last row is centred under the full ones, so a 7th tile does not hang off the left edge.
        const unsigned inRow = row + 1 == grid.rows ? count - row * grid.columns : grid.columns;
        const unsigned rowShift = (grid.columns - inRow) * (tileW + gap) / 2;
        grid.tiles.emplace_back(
          origin + DrawPoint(static_cast<int>(rowShift + col * (tileW + gap)), static_cast<int>(row * (tileH + gap))),
          Extent(tileW, tileH));
    }
    return grid;
}

std::vector<Rect> LayoutList(const Rect& area, const unsigned count, const unsigned rowHeight, const unsigned maxWidth,
                             const unsigned gap)
{
    std::vector<Rect> rows;
    if(count == 0)
        return rows;
    const Extent areaSize = area.getSize();
    const unsigned width = std::min(areaSize.x, maxWidth);
    const unsigned needed = count * rowHeight + (count - 1) * gap;
    unsigned height = rowHeight;
    if(needed > areaSize.y)
        height = std::max(rowHeight / 3, clampSub(areaSize.y, (count - 1) * gap) / count);
    const DrawPoint origin = area.getOrigin() + DrawPoint(static_cast<int>((areaSize.x - width) / 2), 0);
    for(unsigned i = 0; i < count; ++i)
        rows.emplace_back(origin + DrawPoint(0, static_cast<int>(i * (height + gap))), Extent(width, height));
    return rows;
}

std::vector<Rect> LayoutPlayerStrip(const Rect& header, const unsigned count, const Extent& badge, const unsigned gap)
{
    std::vector<Rect> badges;
    if(count == 0)
        return badges;
    const unsigned width = count * badge.x + (count - 1) * gap;
    const int left = header.right - static_cast<int>(pageMargin + width);
    const int top = header.top + static_cast<int>(clampSub(header.getSize().y, badge.y) / 2);
    for(unsigned i = 0; i < count; ++i)
        badges.emplace_back(DrawPoint(left + static_cast<int>(i * (badge.x + gap)), top), badge);
    return badges;
}

} // namespace frontend
