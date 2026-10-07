// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MenuPadFixture.h"
#include "PointOutput.h"
#include "SteamDeckUi.h"
#include "controls/ctrlButton.h"
#include "desktops/dskFrontEndPage.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "frontend/PageLayout.h"
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <memory>

// The F1 page framework through the real input paths only: pad events via the mock driver, mouse and
// keyboard via the WindowManager, exactly as the game delivers them (see MenuPadFixture).

namespace {
constexpr PadDeviceId pad = 77;

bool inside(const Rect& inner, const Rect& outer)
{
    return inner.left >= outer.left && inner.top >= outer.top && inner.right <= outer.right
           && inner.bottom <= outer.bottom;
}

/// A page with `count` tiles; choosing a tile opens the same kind of page one level deeper.
class TestPage : public dskFrontEndPage
{
public:
    static constexpr unsigned firstItem = ID_FIRST_FREE;

    TestPage(unsigned depth, unsigned count) : dskFrontEndPage("Level " + std::to_string(depth)), depth(depth)
    {
        UseTiles(Extent(4, 3), Extent(240, 180));
        for(unsigned i = 0; i < count; ++i)
            AddItem(firstItem + i, "Item " + std::to_string(i));
    }
    static Factory make(unsigned depth, unsigned count)
    {
        return [depth, count] { return std::make_unique<TestPage>(depth, count); };
    }
    const unsigned depth;
    bool rootBackHandled = false;
    bool handleRootBack = false;

protected:
    void OnChoose(unsigned id) override
    {
        if(id >= firstItem)
            Open(make(depth + 1, 4));
    }
    bool OnBackAtRoot() override
    {
        rootBackHandled = true;
        return handleRootBack;
    }
};

struct FrontEndPageFixture : rttr::test::MenuPadFixture
{
    const decltype(SETTINGS.video) savedVideo = SETTINGS.video;
    const VideoMode savedSize = VIDEODRIVER.GetWindowSize();
    const DisplayMode savedDisplayMode = VIDEODRIVER.GetDisplayMode();
    const unsigned savedScale = VIDEODRIVER.getGuiScale().percent();
    const unsigned savedReference = VIDEODRIVER.getUiReferenceHeight();

    /// Set before a test changes the screen; the destructor puts it back on every exit, also after a
    /// failed REQUIRE.
    bool screenChanged = false;

    // NOLINTNEXTLINE(bugprone-exception-escape)
    ~FrontEndPageFixture() override
    {
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        WINDOWMANAGER.Draw();
        if(screenChanged)
            restoreScreen();
    }

    void restoreScreen()
    {
        SETTINGS.video = savedVideo;
        VIDEODRIVER.setUiReferenceHeight(savedReference);
        VIDEODRIVER.setGuiScalePercent(savedScale);
        VIDEODRIVER.ResizeScreen(savedSize, savedDisplayMode);
        BOOST_TEST((VIDEODRIVER.GetRenderSize() == Extent(savedSize.width, savedSize.height)));
    }

    static TestPage& page()
    {
        auto* p = desktopAs<TestPage>();
        BOOST_TEST_REQUIRE(p != nullptr);
        return *p;
    }
    void start(unsigned count = 6)
    {
        dskFrontEndPage::Show(TestPage::make(0, count));
        frame();
        pickUp(pad);
        frame();
        BOOST_TEST_REQUIRE(page().depth == 0u);
    }
    static bool hasKey(PadButton button, brief::KeyAction action)
    {
        const auto& keys = page().GetFooterKeys();
        return std::find(keys.begin(), keys.end(), brief::KeyHint{button, action}) != keys.end();
    }
    void click(const Window& ctrl)
    {
        const Rect r = ctrl.GetDrawRect();
        MouseCoords mc(Position((r.left + r.right) / 2, (r.top + r.bottom) / 2));
        mc.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mc);
        mc.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mc);
        frame();
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(FrontEndPageTests)

BOOST_FIXTURE_TEST_CASE(PadEntersOnFirstTileAndFooterTellsTheTruth, FrontEndPageFixture)
{
    start();
    BOOST_TEST(focusedId(0) == TestPage::firstItem);
    // Root page: nowhere to go back to, so neither the back button nor B is offered.
    BOOST_TEST(!page().CanGoBack());
    BOOST_TEST(!page().GetCtrl<ctrlButton>(dskFrontEndPage::ID_btBack)->IsVisible());
    BOOST_TEST(hasKey(PadButton::A, brief::KeyAction::Choose));
    BOOST_TEST(!hasKey(PadButton::B, brief::KeyAction::PageBack));
    // The top-left tile can move right and down, not left (no wrap at the window edge).
    BOOST_TEST(hasKey(PadButton::DpadRight, brief::KeyAction::MoveFocus));
    BOOST_TEST(hasKey(PadButton::DpadDown, brief::KeyAction::MoveFocus));
    BOOST_TEST(!hasKey(PadButton::DpadLeft, brief::KeyAction::MoveFocus));
    // ... and the footer's promise holds: right really moves to the next tile.
    press(pad, PadButton::DpadRight);
    BOOST_TEST(focusedId(0) == TestPage::firstItem + 1);
    BOOST_TEST(hasKey(PadButton::DpadLeft, brief::KeyAction::MoveFocus));
    // B on a root page that does not handle it changes nothing.
    press(pad, PadButton::B);
    BOOST_TEST(page().rootBackHandled);
    BOOST_TEST(page().depth == 0u);
    // The joined-player strip shows the controller in hand, inside the header.
    BOOST_TEST_REQUIRE(page().GetPlayerStrip().size() == 1u);
    BOOST_TEST(inside(page().GetPlayerStrip()[0], page().GetFrame().header));
}

BOOST_FIXTURE_TEST_CASE(AOpensBReturnsToTheChosenTile, FrontEndPageFixture)
{
    start();
    press(pad, PadButton::DpadRight);
    press(pad, PadButton::DpadRight);
    const unsigned chosen = focusedId(0);
    BOOST_TEST_REQUIRE(chosen == TestPage::firstItem + 2);
    press(pad, PadButton::A);
    frame();
    BOOST_TEST_REQUIRE(page().depth == 1u);
    BOOST_TEST(page().GetTrail().size() == 1u);
    BOOST_TEST(page().CanGoBack());
    BOOST_TEST(page().GetCtrl<ctrlButton>(dskFrontEndPage::ID_btBack)->IsVisible());
    BOOST_TEST(focusedId(0) == TestPage::firstItem);
    BOOST_TEST(hasKey(PadButton::B, brief::KeyAction::PageBack));

    // One level deeper and all the way back: every level restores the tile it was left on.
    press(pad, PadButton::DpadDown);
    const unsigned chosen1 = focusedId(0);
    BOOST_TEST_REQUIRE(chosen1 != TestPage::firstItem);
    press(pad, PadButton::A);
    frame();
    BOOST_TEST_REQUIRE(page().depth == 2u);
    BOOST_TEST(page().GetTrail().size() == 2u);

    press(pad, PadButton::B);
    frame();
    BOOST_TEST_REQUIRE(page().depth == 1u);
    BOOST_TEST(focusedId(0) == chosen1);
    press(pad, PadButton::B);
    frame();
    BOOST_TEST_REQUIRE(page().depth == 0u);
    BOOST_TEST(focusedId(0) == chosen);
    BOOST_TEST(page().GetTrail().empty());
    BOOST_TEST(!page().CanGoBack());
}

BOOST_FIXTURE_TEST_CASE(TwoPressesInOneFrameSwitchOnlyOnce, FrontEndPageFixture)
{
    start();
    // A and B before the frame that performs the switch: the page is already leaving, B must not
    // queue "back" over "open".
    tap(pad, PadButton::A);
    tap(pad, PadButton::B);
    frame();
    frame();
    BOOST_TEST_REQUIRE(page().depth == 1u);
    // B, B in one frame goes back exactly one level.
    press(pad, PadButton::A);
    frame();
    BOOST_TEST_REQUIRE(page().depth == 2u);
    tap(pad, PadButton::B);
    tap(pad, PadButton::B);
    frame();
    frame();
    BOOST_TEST(page().depth == 1u);
}

BOOST_FIXTURE_TEST_CASE(MouseAndKeyboardWorkWithoutAController, FrontEndPageFixture)
{
    dskFrontEndPage::Show(TestPage::make(0, 6));
    frame();
    BOOST_TEST(page().GetFooterKeys().empty()); // nothing to explain without a controller in hand
    click(*page().GetCtrl<Window>(TestPage::firstItem + 3));
    frame();
    BOOST_TEST_REQUIRE(page().depth == 1u);
    // The back button is clickable ...
    click(*page().GetCtrl<Window>(dskFrontEndPage::ID_btBack));
    frame();
    BOOST_TEST_REQUIRE(page().depth == 0u);
    // ... and Escape is back as well.
    click(*page().GetCtrl<Window>(TestPage::firstItem + 4));
    frame();
    BOOST_TEST_REQUIRE(page().depth == 1u);
    WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
    frame();
    BOOST_TEST_REQUIRE(page().depth == 0u);
    // The clicked tile is where a controller picked up afterwards starts.
    pickUp(pad);
    BOOST_TEST(focusedId(0) == TestPage::firstItem + 4);
}

BOOST_FIXTURE_TEST_CASE(LayoutFollowsTheScreenIncludingTheDeck, FrontEndPageFixture)
{
    const auto check = [](const Extent& expectedSize) {
        TestPage& p = page();
        BOOST_TEST((p.GetSize() == expectedSize));
        const auto& frame = p.GetFrame();
        BOOST_TEST(frame.header.getSize().x == expectedSize.x);
        BOOST_TEST(frame.footer.bottom == static_cast<int>(expectedSize.y));
        for(const unsigned id : p.GetItems())
        {
            const Window& item = *p.GetCtrl<Window>(id);
            BOOST_TEST(inside(item.GetDrawRect(), frame.content));
        }
    };
    {
        start(8);
        check(VIDEODRIVER.GetRenderSize());
        const Extent small = page().GetCtrl<Window>(TestPage::firstItem)->GetSize();
        // The Deck: 1280×800 at its automatic 125 % gives 1024×640 render units (PR #41).
        screenChanged = true;
        SETTINGS.video.steamDeckUi = true;
        SETTINGS.video.guiScale = 0;
        SETTINGS.video.tvMode = false;
        VIDEODRIVER.ResizeScreen(VideoMode(1280, 800), DisplayMode::Windowed);
        VIDEODRIVER.setUiReferenceHeight(deck::UiReferenceHeight(false, true));
        VIDEODRIVER.setGuiScalePercent(0);
        frame();
        BOOST_TEST_REQUIRE((VIDEODRIVER.GetRenderSize() == Extent(1024, 640)));
        check(Extent(1024, 640));
        // Not stretched: tiles keep their 4:3 shape on the 16:10 screen.
        const Extent deck = page().GetCtrl<Window>(TestPage::firstItem)->GetSize();
        BOOST_TEST(std::abs(double(deck.x) / deck.y - 4.0 / 3.0) < 0.05);
        BOOST_TEST(deck.x >= small.x);
        // A page created on the Deck screen lays out the same way, and the pad still moves between tiles.
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(page().depth == 1u);
        check(Extent(1024, 640));
        press(pad, PadButton::DpadRight);
        BOOST_TEST(focusedId(0) == TestPage::firstItem + 1);

        // A resize while a page waits for its switch reaches only the current desktop. The queued page
        // was built for 1024×640 and must still fit the 800×600 it is shown on (the game's start: the
        // first page is queued, then the window gets its configured size).
        dskFrontEndPage::Show(TestPage::make(5, 8));
        BOOST_TEST_REQUIRE(WINDOWMANAGER.IsSwitchPending());
        VIDEODRIVER.setUiReferenceHeight(savedReference);
        VIDEODRIVER.setGuiScalePercent(100);
        VIDEODRIVER.ResizeScreen(VideoMode(800, 600), DisplayMode::Windowed);
        frame();
        BOOST_TEST_REQUIRE(page().depth == 5u);
        BOOST_TEST_REQUIRE((VIDEODRIVER.GetRenderSize() == Extent(800, 600)));
        check(Extent(800, 600));
    }
}

BOOST_AUTO_TEST_SUITE_END()
