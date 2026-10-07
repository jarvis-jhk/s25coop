// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "frontend/PageLayout.h"
#include <boost/test/unit_test.hpp>
#include <cmath>

using namespace frontend;

namespace {
bool inside(const Rect& inner, const Rect& outer)
{
    return inner.left >= outer.left && inner.top >= outer.top && inner.right <= outer.right
           && inner.bottom <= outer.bottom;
}
bool overlaps(const Rect& a, const Rect& b)
{
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}
} // namespace

BOOST_AUTO_TEST_SUITE(FrontEndLayout)

BOOST_AUTO_TEST_CASE(FrameSplitsEveryScreenWithoutOverlap)
{
    for(const Extent size : {Extent(800, 600), Extent(1024, 640), Extent(1280, 720), Extent(3840, 2160)})
    {
        const Rect screen(DrawPoint(0, 0), size);
        const PageFrame frame = LayoutFrame(size, 2, 14);
        BOOST_TEST(inside(frame.header, screen));
        BOOST_TEST(inside(frame.footer, screen));
        BOOST_TEST(inside(frame.content, screen));
        BOOST_TEST(frame.header.getSize().x == size.x);
        BOOST_TEST(frame.footer.bottom == static_cast<int>(size.y));
        BOOST_TEST(frame.footer.getSize().y == 2u * 14u + 8u);
        BOOST_TEST(frame.content.top >= frame.header.bottom + static_cast<int>(pageMargin));
        BOOST_TEST(frame.content.bottom <= frame.footer.top - static_cast<int>(pageMargin));
        BOOST_TEST(frame.content.getSize().x <= maxContentWidth);
        // Centred
        BOOST_TEST(frame.content.left == static_cast<int>(size.x) - frame.content.right);
    }
    // The Deck's screen gives more room than 800×600 - in BOTH directions, which is the point of laying out
    // in render units instead of stretching.
    BOOST_TEST(LayoutFrame(Extent(1024, 640), 2, 14).content.getSize().x
               > LayoutFrame(Extent(800, 600), 2, 14).content.getSize().x);
}

BOOST_AUTO_TEST_CASE(TilesFitKeepAspectAndDoNotOverlap)
{
    const Extent aspect(4, 3);
    for(const Extent size : {Extent(800, 600), Extent(1024, 640), Extent(1920, 1080)})
    {
        const Rect area = LayoutFrame(size, 2, 14).content;
        for(unsigned count = 1; count <= 12; ++count)
        {
            const TileGrid grid = LayoutTiles(area, count, aspect, Extent(320, 240));
            BOOST_TEST_REQUIRE(grid.tiles.size() == count);
            BOOST_TEST(grid.columns * grid.rows >= count);
            BOOST_TEST((grid.columns - 1) * grid.rows < count); // no empty column
            for(unsigned i = 0; i < count; ++i)
            {
                const Rect& t = grid.tiles[i];
                BOOST_TEST(inside(t, area));
                BOOST_TEST(t.getSize().x <= 320u);
                BOOST_TEST(t.getSize().y <= 240u);
                const double ratio = double(t.getSize().x) / t.getSize().y;
                BOOST_TEST(std::abs(ratio - 4.0 / 3.0) < 0.05);
                for(unsigned j = i + 1; j < count; ++j)
                    BOOST_TEST(!overlaps(t, grid.tiles[j]));
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(TilesAreLargerOnTheDeckAndShortRowIsCentred)
{
    const Rect small = LayoutFrame(Extent(800, 600), 2, 14).content;
    const Rect deck = LayoutFrame(Extent(1024, 640), 2, 14).content;
    const auto a = LayoutTiles(small, 8, Extent(4, 3), Extent(1000, 1000));
    const auto b = LayoutTiles(deck, 8, Extent(4, 3), Extent(1000, 1000));
    BOOST_TEST(b.tiles[0].getSize().x >= a.tiles[0].getSize().x);

    // 7 tiles in 4 columns: the last row (3 tiles) is centred under the full one.
    const Rect area(DrawPoint(0, 0), Extent(4 * 100 + 3 * 10, 2 * 75 + 10));
    const auto grid = LayoutTiles(area, 7, Extent(4, 3), Extent(100, 75), 10);
    BOOST_TEST_REQUIRE(grid.columns == 4u);
    const int fullLeft = grid.tiles[0].left;
    const int fullRight = grid.tiles[3].right;
    const int shortLeft = grid.tiles[4].left;
    const int shortRight = grid.tiles[6].right;
    BOOST_TEST(shortLeft - fullLeft == fullRight - shortRight);
    BOOST_TEST(grid.tiles[4].top > grid.tiles[0].top);
}

BOOST_AUTO_TEST_CASE(TilesSurviveAnAreaTooSmallForTheirGaps)
{
    for(const Extent size : {Extent(0, 0), Extent(5, 5), Extent(30, 0)})
    {
        const auto grid = LayoutTiles(Rect(DrawPoint(10, 10), size), 4, Extent(4, 3), Extent(100, 75));
        BOOST_TEST_REQUIRE(grid.tiles.size() == 4u);
        BOOST_TEST(grid.columns >= 1u);
        for(const Rect& t : grid.tiles)
            BOOST_TEST(t.getSize().x >= 1u);
    }
    BOOST_TEST(LayoutTiles(Rect(DrawPoint(0, 0), Extent(100, 100)), 3, Extent(0, 3), Extent(10, 10)).tiles.empty());
}

BOOST_AUTO_TEST_CASE(ListRowsStackAndShrinkWhenFull)
{
    const Rect area(DrawPoint(100, 50), Extent(600, 300));
    const auto rows = LayoutList(area, 4, 40, 400, 6);
    BOOST_TEST_REQUIRE(rows.size() == 4u);
    for(unsigned i = 0; i < rows.size(); ++i)
    {
        BOOST_TEST(inside(rows[i], area));
        BOOST_TEST((rows[i].getSize() == Extent(400, 40)));
        BOOST_TEST(rows[i].left == 200); // centred
        if(i)
            BOOST_TEST(rows[i].top == rows[i - 1].bottom + 6);
    }
    const auto full = LayoutList(area, 12, 40, 400, 6);
    BOOST_TEST(full.back().bottom <= area.bottom);
    BOOST_TEST(full.front().getSize().y < 40u);
}

BOOST_AUTO_TEST_CASE(PlayerStripIsRightAlignedInTheHeader)
{
    const Rect header(DrawPoint(0, 0), Extent(1024, headerHeight));
    BOOST_TEST(LayoutPlayerStrip(header, 0, Extent(30, 22)).empty());
    const auto strip = LayoutPlayerStrip(header, 4, Extent(30, 22), 4);
    BOOST_TEST_REQUIRE(strip.size() == 4u);
    BOOST_TEST(strip.back().right == 1024 - static_cast<int>(pageMargin));
    for(unsigned i = 0; i < 4; ++i)
    {
        BOOST_TEST(inside(strip[i], header));
        if(i)
            BOOST_TEST(strip[i].left == strip[i - 1].right + 4);
    }
}

BOOST_AUTO_TEST_SUITE_END()
