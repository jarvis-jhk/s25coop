// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GlobalVars.h"
#include "Loader.h"
#include "MenuPadFixture.h"
#include "controls/ctrlButton.h"
#include "desktops/dskCampaignSelection.h"
#include "desktops/dskCredits.h"
#include "desktops/dskHome.h"
#include "desktops/dskMainMenu.h"
#include "desktops/dskMultiPlayer.h"
#include "desktops/dskOptions.h"
#include "desktops/dskSelectMap.h"
#include "desktops/dskSinglePlayer.h"
#include "desktops/dskTitle.h"
#include "driver/MouseCoords.h"
#include "frontend/MenuRoutes.h"
#include "ingameWindows/iwSave.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/test/unit_test.hpp>
#include <memory>

// F3: the home page, and that every upstream desktop it opens comes back to it - through the real pad,
// mouse and back-button paths only.

namespace {
constexpr PadDeviceId pad = 61;

struct HomeFixture : rttr::test::MenuPadFixture
{
    // No savegames: "Resume last game" must be disabled.
    rttr::test::TmpFolder userData;
    rttr::test::ConfigOverride userDataOverride{"USERDATA", userData};
    rttr::test::ConfigOverride gameOverride{"GAME", userData};
    bool& running = GLOBALVARS.notdone;
    const bool wasRunning = running;

    HomeFixture()
    {
        // dskOptions reads the language archive, which the test data lacks (see testMenuPadOptions.cpp).
        LOADER.LoadDummyLanguageFiles();
        running = true;
        dskHome::ForgetLastChoice();
    }
    // NOLINTNEXTLINE(bugprone-exception-escape)
    ~HomeFixture() override
    {
        running = wasRunning;
        dskHome::ForgetLastChoice();
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        WINDOWMANAGER.Draw();
    }

    void enter()
    {
        WINDOWMANAGER.Switch(dskHome::Create());
        frame();
        pickUp(pad);
        BOOST_TEST_REQUIRE(desktopAs<dskHome>() != nullptr);
        BOOST_TEST((frontend::GetMenuStyle() == frontend::MenuStyle::FrontEnd));
    }
    /// Walk the focus with RB/LB like a player would (no wrap), until the tile is focused.
    void focusTile(unsigned id)
    {
        for(unsigned i = 0; i < 12 && focusedId(0) != id; ++i)
            press(pad, focusedId(0) < id ? PadButton::RightShoulder : PadButton::LeftShoulder);
        BOOST_TEST_REQUIRE(focusedId(0) == id);
    }
    void choose(unsigned id)
    {
        focusTile(id);
        press(pad, PadButton::A);
        frame();
    }
    void expectHomeFocusedOn(unsigned id)
    {
        frame();
        BOOST_TEST_REQUIRE(desktopAs<dskHome>() != nullptr);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(focusedId(0) == id);
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
        frame();
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(FrontEndHomeTests)

BOOST_FIXTURE_TEST_CASE(EntryAndFooterWithoutSavegames, HomeFixture)
{
    enter();
    auto& home = *desktopAs<dskHome>();
    BOOST_TEST(!home.GetCtrl<ctrlButton>(dskHome::ID_Continue)->GetEnabled());
    // The pad starts on the first tile it can use, not on the disabled one.
    BOOST_TEST(focusedId(0) == dskHome::ID_Campaigns);
    BOOST_TEST(!home.CanGoBack());
    BOOST_TEST(!home.GetFooterKeys().empty());
    // All ten tiles are inside the content area.
    BOOST_TEST(home.GetItems().size() == 10u);
    for(const unsigned id : home.GetItems())
    {
        const Rect r = home.GetCtrl<Window>(id)->GetDrawRect();
        const Rect& c = home.GetFrame().content;
        BOOST_TEST((r.left >= c.left && r.right <= c.right && r.top >= c.top && r.bottom <= c.bottom));
    }
    // B on Home leads to the title page, where more players can join.
    BOOST_TEST(home.HasBackAction());
    press(pad, PadButton::B);
    frame();
    BOOST_TEST(desktopAs<dskTitle>() != nullptr);
    BOOST_TEST(running);
}

BOOST_FIXTURE_TEST_CASE(EveryUpstreamDesktopComesBackToItsTile, HomeFixture)
{
    enter();
    // Maps first: Campaigns is also where the pad starts, so it would not prove the restore.
    choose(dskHome::ID_Maps);
    BOOST_TEST_REQUIRE(desktopAs<dskSelectMap>() != nullptr);
    press(pad, PadButton::B);
    expectHomeFocusedOn(dskHome::ID_Maps);

    choose(dskHome::ID_Campaigns);
    BOOST_TEST_REQUIRE(desktopAs<dskCampaignSelection>() != nullptr);
    press(pad, PadButton::B);
    expectHomeFocusedOn(dskHome::ID_Campaigns);

    choose(dskHome::ID_Online);
    BOOST_TEST_REQUIRE(desktopAs<dskMultiPlayer>() != nullptr);
    press(pad, PadButton::B);
    expectHomeFocusedOn(dskHome::ID_Online);

    choose(dskHome::ID_Options);
    BOOST_TEST_REQUIRE(desktopAs<dskOptions>() != nullptr);
    press(pad, PadButton::B);
    expectHomeFocusedOn(dskHome::ID_Options);

    choose(dskHome::ID_Credits);
    BOOST_TEST_REQUIRE(desktopAs<dskCredits>() != nullptr);
    press(pad, PadButton::B);
    expectHomeFocusedOn(dskHome::ID_Credits);

    // Load game opens the map selection with the load window on top: B closes the window, B again is back.
    choose(dskHome::ID_Load);
    BOOST_TEST_REQUIRE(desktopAs<dskSelectMap>() != nullptr);
    BOOST_TEST_REQUIRE(dynamic_cast<iwLoad*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
    press(pad, PadButton::B);
    frame();
    BOOST_TEST_REQUIRE(desktopAs<dskSelectMap>() != nullptr);
    press(pad, PadButton::B);
    expectHomeFocusedOn(dskHome::ID_Load);
}

BOOST_FIXTURE_TEST_CASE(ClassicMenusKeepTheClassicReturns, HomeFixture)
{
    enter();
    choose(dskHome::ID_Classic);
    BOOST_TEST_REQUIRE(desktopAs<dskMainMenu>() != nullptr);
    BOOST_TEST((frontend::GetMenuStyle() == frontend::MenuStyle::Classic));
    // The old chain: main menu -> single player -> back lands in the old main menu, not on Home.
    // dskMainMenu's "Singleplayer" is its first own id (dskMenuBase::ID_FIRST_FREE).
    const auto* single = desktop()->GetCtrl<ctrlButton>(dskMenuBase::ID_FIRST_FREE);
    BOOST_TEST_REQUIRE(single != nullptr);
    for(unsigned i = 0; i < 10 && focused(0) != single; ++i)
        press(pad, PadButton::RightShoulder);
    BOOST_TEST_REQUIRE(focused(0) == single);
    press(pad, PadButton::A);
    frame();
    BOOST_TEST_REQUIRE(desktopAs<dskSinglePlayer>() != nullptr);
    press(pad, PadButton::B);
    frame();
    BOOST_TEST_REQUIRE(desktopAs<dskMainMenu>() != nullptr);
    // ... and "New menus" is the way back to Home, which then owns the returns again.
    const auto* newMenus = desktop()->GetCtrl<ctrlButton>(dskMenuBase::ID_FIRST_FREE + 10);
    BOOST_TEST_REQUIRE(newMenus != nullptr);
    for(unsigned i = 0; i < 12 && focused(0) != newMenus; ++i)
        press(pad, PadButton::RightShoulder);
    BOOST_TEST_REQUIRE(focused(0) == newMenus);
    press(pad, PadButton::A);
    frame();
    BOOST_TEST_REQUIRE(desktopAs<dskHome>() != nullptr);
    BOOST_TEST((frontend::GetMenuStyle() == frontend::MenuStyle::FrontEnd));
}

BOOST_FIXTURE_TEST_CASE(RoutesFollowTheMenuShownLast, HomeFixture)
{
    // The style changes when a menu is SHOWN, not when one is merely built.
    {
        const auto unused = std::make_unique<dskMainMenu>();
    }
    BOOST_TEST((frontend::GetMenuStyle() == frontend::MenuStyle::Classic));
    BOOST_TEST(dynamic_cast<dskMainMenu*>(frontend::MainMenu().get()) != nullptr);
    BOOST_TEST(dynamic_cast<dskSinglePlayer*>(frontend::SinglePlayerMenu().get()) != nullptr);
    enter();
    {
        const auto unused = std::make_unique<dskMainMenu>();
    }
    BOOST_TEST((frontend::GetMenuStyle() == frontend::MenuStyle::FrontEnd));
    BOOST_TEST(dynamic_cast<dskHome*>(frontend::MainMenu().get()) != nullptr);
    BOOST_TEST(dynamic_cast<dskHome*>(frontend::SinglePlayerMenu().get()) != nullptr);
}

BOOST_FIXTURE_TEST_CASE(MouseOpensAndReturnsAndQuitQuits, HomeFixture)
{
    WINDOWMANAGER.Switch(dskHome::Create());
    frame();
    BOOST_TEST_REQUIRE(desktopAs<dskHome>() != nullptr);
    click(*desktop()->GetCtrl<Window>(dskHome::ID_Online));
    BOOST_TEST_REQUIRE(desktopAs<dskMultiPlayer>() != nullptr);
    // dskMultiPlayer's own Back button (id 6)
    click(*desktop()->GetCtrl<Window>(6));
    BOOST_TEST_REQUIRE(desktopAs<dskHome>() != nullptr);
    click(*desktop()->GetCtrl<Window>(dskHome::ID_Quit));
    BOOST_TEST(!running);
}

BOOST_AUTO_TEST_SUITE_END()
