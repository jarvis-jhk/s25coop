// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GlobalVars.h"
#include "Loader.h"
#include "MenuPadFixture.h"
#include "controls/ctrlButton.h"
#include "desktops/dskMainMenu.h"
#include "desktops/dskMultiPlayer.h"
#include "desktops/dskSinglePlayer.h"
#include "driver/MouseCoords.h"
#include "ingameWindows/iwLobbyConnect.h"
#include "ingameWindows/iwMsgbox.h"
#include "ingameWindows/iwPlayReplay.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/test/unit_test.hpp>
#include <memory>
#include <vector>

namespace {
constexpr PadDeviceId pad = 96;

struct PlayerMenuReturnFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder userData;
    rttr::test::ConfigOverride userDataOverride{"USERDATA", userData};
    const bool wasRunning = GLOBALVARS.notdone;

    PlayerMenuReturnFixture() { GLOBALVARS.notdone = true; }
    ~PlayerMenuReturnFixture() override { GLOBALVARS.notdone = wasRunning; }

    void enter(const bool multiplayer)
    {
        padInput().Reset();
        if(multiplayer)
            WINDOWMANAGER.Switch(std::make_unique<dskMultiPlayer>());
        else
            WINDOWMANAGER.Switch(std::make_unique<dskSinglePlayer>());
        frame();
        pickUp(pad);
        BOOST_TEST_REQUIRE(isMenu(multiplayer));
        BOOST_TEST_REQUIRE(router().GetSlot(pad) == 0u);
    }

    static bool isMenu(const bool multiplayer)
    {
        return multiplayer ? desktopAs<dskMultiPlayer>() != nullptr : desktopAs<dskSinglePlayer>() != nullptr;
    }

    void focusUntil(const Window* target)
    {
        for(unsigned i = 0; i < 20 && focused(0) != target; ++i)
            press(pad, PadButton::RightShoulder);
        BOOST_TEST_REQUIRE(focused(0) == target);
    }

    void focusAtY(const int y)
    {
        for(const auto* button : desktop()->GetCtrls<ctrlButton>())
        {
            if(button->GetPos().y == y)
            {
                focusUntil(button);
                return;
            }
        }
        BOOST_FAIL("Expected player-menu button missing");
    }

    void expectMainMenu()
    {
        BOOST_TEST_REQUIRE(desktopAs<dskMainMenu>() != nullptr);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(GLOBALVARS.notdone); // Quit directly clears this flag, without opening a dialog.
        BOOST_TEST(video.padEvents_.empty());
        frame();
        BOOST_TEST_REQUIRE(desktopAs<dskMainMenu>() != nullptr);
        BOOST_TEST(GLOBALVARS.notdone);
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(MenuPadPlayerReturnTests)

BOOST_FIXTURE_TEST_CASE(BReturnsFromEveryFocusedPlayerMenuAction, PlayerMenuReturnFixture)
{
    for(const bool multiplayer : {false, true})
    {
        enter(multiplayer);
        std::vector<unsigned> ids;
        for(const auto* button : desktop()->GetCtrls<ctrlButton>())
            ids.push_back(button->GetID());
        BOOST_TEST_REQUIRE(ids.size() == (multiplayer ? 4u : 6u));
        for(const auto id : ids)
        {
            enter(multiplayer);
            focusUntil(desktop()->GetCtrl<ctrlButton>(id));
            press(pad, PadButton::B);
            expectMainMenu();
        }
    }
}

BOOST_FIXTURE_TEST_CASE(BClosesReplayAndLobbyLoginBeforeLeavingTheMenu, PlayerMenuReturnFixture)
{
    for(const bool multiplayer : {false, true})
    {
        enter(multiplayer);
        focusAtY(multiplayer ? 180 : 320); // Internet lobby / Play Replay.
        press(pad, PadButton::A);
        auto* overlay = WINDOWMANAGER.GetTopMostWindow();
        BOOST_TEST_REQUIRE(overlay != nullptr);
        if(multiplayer)
            BOOST_TEST_REQUIRE(dynamic_cast<iwLobbyConnect*>(overlay) != nullptr);
        else
            BOOST_TEST_REQUIRE(dynamic_cast<iwPlayReplay*>(overlay) != nullptr);
        press(pad, PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST_REQUIRE(isMenu(multiplayer));
        press(pad, PadButton::B);
        expectMainMenu();
    }
}

BOOST_FIXTURE_TEST_CASE(BDoesNotBypassTheMissingSaveConfirmation, PlayerMenuReturnFixture)
{
    enter(false);
    focusAtY(180); // Resume last game; isolated userdata contains no saves.
    press(pad, PadButton::A);
    auto* error = dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow());
    BOOST_TEST_REQUIRE(error != nullptr);
    press(pad, PadButton::B);
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == error);
    BOOST_TEST_REQUIRE(isMenu(false));
    press(pad, PadButton::A);
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    BOOST_TEST_REQUIRE(isMenu(false));
    press(pad, PadButton::B);
    expectMainMenu();
}

BOOST_FIXTURE_TEST_CASE(StartAndNavigationDoNotActivateOrLeavePlayerMenus, PlayerMenuReturnFixture)
{
    for(const bool multiplayer : {false, true})
    {
        enter(multiplayer);
        for(const auto button : {PadButton::Start, PadButton::DpadDown, PadButton::RightShoulder})
        {
            press(pad, button);
            BOOST_TEST_REQUIRE(isMenu(multiplayer));
            BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        }
    }
}

BOOST_FIXTURE_TEST_CASE(VisibleBackStillWorksWithMouseAndControllerA, PlayerMenuReturnFixture)
{
    for(const bool multiplayer : {false, true})
    {
        const int backY = multiplayer ? 290 : 390;
        enter(multiplayer);
        focusAtY(backY);
        press(pad, PadButton::A);
        expectMainMenu();
        enter(multiplayer);
        MouseCoords mc(Position(225, backY + 10));
        mc.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mc);
        mc.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mc);
        frame();
        expectMainMenu();
    }
}

BOOST_FIXTURE_TEST_CASE(BackBurstCannotActivateOrCloseTheMainMenu, PlayerMenuReturnFixture)
{
    for(const bool multiplayer : {false, true})
    {
        enter(multiplayer);
        tap(pad, PadButton::B);
        tap(pad, PadButton::B);
        frame();
        expectMainMenu();
        press(pad, PadButton::B);
        expectMainMenu();
        focusAtY(410); // Main-menu Quit program: B must not activate even this focused control.
        press(pad, PadButton::B);
        expectMainMenu();
        BOOST_TEST(focused(0) != nullptr);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    }
}

BOOST_AUTO_TEST_SUITE_END()
