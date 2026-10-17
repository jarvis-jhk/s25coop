// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Loader.h"
#include "RttrConfig.h"
#include "files.h"
#include "input/KeyGlyph.h"
#include "ogl/glArchivItem_Bitmap.h"
#include "ogl/glFont.h"
#include "uiHelper/uiHelpers.hpp"
#include "gameData/GuiConsts.h"
#include "libsiedler2/Archiv.h"
#include "libsiedler2/ColorBGRA.h"
#include "libsiedler2/libsiedler2.h"
#include <boost/test/unit_test.hpp>

using brief::KeyAction;
using brief::KeyHint;
using brief::KeyInput;

namespace {
/// The shipped folder, loaded the way Loader loads it (folder archive, GL bitmaps).
struct GlyphFixture : uiHelper::Fixture
{
    libsiedler2::Archiv archive;
    GlyphFixture()
    {
        const auto folder = RTTRCONFIG.ExpandPath(s25::folders::assetsBase) / "pad_glyphs";
        BOOST_TEST_REQUIRE(libsiedler2::LoadFolder(libsiedler2::ReadFolderInfo(folder), archive) == 0);
    }
    glArchivItem_Bitmap* get(unsigned i) { return dynamic_cast<glArchivItem_Bitmap*>(archive[i]); }
    brief::GlyphLookup lookup()
    {
        return [this](unsigned i) { return get(i); };
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(PadGlyphs)

BOOST_FIXTURE_TEST_CASE(EveryGlyphLoadsTransparentAroundAndFitsTheFont, GlyphFixture)
{
    BOOST_TEST(archive.size() == brief::padGlyphCount);
    for(unsigned i = 0; i < brief::padGlyphCount; ++i)
    {
        BOOST_TEST_CONTEXT("glyph " << i)
        {
            auto* bmp = get(i);
            BOOST_TEST_REQUIRE(bmp);
            BOOST_TEST(bmp->getHeight() <= 13u);
            BOOST_TEST(bmp->getHeight() <= NormalFont->getHeight() + 4u);
            BOOST_TEST(bmp->getPixel(0, 0).getAlpha() == 0u);
            BOOST_TEST(bmp->getPixel(bmp->getWidth() / 2, bmp->getHeight() / 2).getAlpha() == 255u);
        }
    }
}

BOOST_FIXTURE_TEST_CASE(InputsBecomeImagesAndADpadGroupBecomesOne, GlyphFixture)
{
    const std::vector<KeyHint> hints = {{PadButton::A, KeyAction::Choose},
                                        {PadButton::DpadUp, KeyAction::MoveFocus},
                                        {PadButton::DpadDown, KeyAction::MoveFocus},
                                        {PadButton::B, KeyAction::LeaveFocus},
                                        {PadButton::A, KeyAction::ZoomIn, KeyInput::RightTriggerAxis}};
    const auto lines = brief::LayoutKeyGlyphs(hints, *NormalFont, 1000, COLOR_WHITE, lookup());
    BOOST_TEST_REQUIRE(lines.size() == 1u);
    std::vector<glArchivItem_Bitmap*> drawn;
    for(const auto& run : lines[0].runs)
    {
        BOOST_TEST(run.badgeColor == 0u); // no code-drawn badge left
        if(run.glyph)
        {
            drawn.push_back(run.glyph);
            BOOST_TEST(run.width == run.glyph->getWidth());
        }
    }
    const std::vector<glArchivItem_Bitmap*> expected = {get(brief::padGlyphA), get(brief::padGlyphDpad - 1 + 3),
                                                        get(brief::padGlyphB), get(brief::padGlyphRT)};
    BOOST_TEST(drawn == expected, boost::test_tools::per_element());

    // The images are drawn, no badge rectangle is emitted for them.
    unsigned rects = 0;
    brief::DrawKeyRuns(DrawPoint(0, 0), lines[0].runs, *NormalFont,
                       [&rects](const Rect&, unsigned) noexcept { ++rects; }); // LCOV_EXCL_LINE
    BOOST_TEST(rects == 0u);
}

BOOST_FIXTURE_TEST_CASE(AMissingImageKeepsItsTextBadge, GlyphFixture)
{
    const std::vector<KeyHint> hints = {{PadButton::A, KeyAction::Choose},
                                        {PadButton::DpadLeft, KeyAction::MoveFocus},
                                        {PadButton::DpadRight, KeyAction::MoveFocus}};
    const auto lines = brief::LayoutKeyGlyphs(hints, *NormalFont, 1000, COLOR_WHITE, [this](unsigned i) {
        return (i == brief::padGlyphA || i >= brief::padGlyphDpad) ? nullptr : get(i);
    });
    BOOST_TEST_REQUIRE(lines.size() == 1u);
    unsigned badges = 0, glyphs = 0;
    for(const auto& run : lines[0].runs)
    {
        badges += run.badgeColor ? 1 : 0;
        glyphs += run.glyph ? 1 : 0;
    }
    BOOST_TEST(glyphs == 0u);
    BOOST_TEST(badges == 3u); // A, Left, Right each as before
    // Same layout as without any images at all (the default lookup in tests: LOADER has no pad_glyphs).
    const auto plain = brief::LayoutKeyGlyphs(hints, *NormalFont, 1000, COLOR_WHITE);
    BOOST_TEST(plain[0].text == lines[0].text);
    BOOST_TEST(plain[0].runs.size() == lines[0].runs.size());
}

BOOST_AUTO_TEST_SUITE_END()
