// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MenuPadFixture.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlTable.h"
#include "desktops/dskDirectIP.h"
#include "desktops/dskLAN.h"
#include "desktops/dskMainMenu.h"
#include "desktops/dskMultiPlayer.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "ingameWindows/iwDirectIPConnect.h"
#include "ingameWindows/iwDirectIPCreate.h"
#include "ingameWindows/iwMsgbox.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/test/unit_test.hpp>
#include <memory>
#include <vector>

namespace {
constexpr PadDeviceId pad = 97;

struct NetworkMenuReturnFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder userData;
    rttr::test::ConfigOverride userDataOverride{"USERDATA", userData};
    ProxyType& proxyType = SETTINGS.proxy.type;
    const ProxyType oldProxy = proxyType;

    NetworkMenuReturnFixture() { proxyType = ProxyType::None; }
    ~NetworkMenuReturnFixture() override { proxyType = oldProxy; }

    void enter(const bool lan, const bool listedGame = false)
    {
        padInput().Reset();
        if(lan)
            WINDOWMANAGER.Switch(std::make_unique<dskLAN>());
        else
            WINDOWMANAGER.Switch(std::make_unique<dskDirectIP>());
        frame();
        if(listedGame)
        {
            // A listed row makes the table focusable; no connection is attempted by this Back-only test.
            const auto tables = desktop()->GetCtrls<ctrlTable>();
            BOOST_TEST_REQUIRE(tables.size() == 1u);
            tables.front()->AddRow({"0", "Test game", "Test map", "1/2", "test"});
        }
        pickUp(pad);
        BOOST_TEST_REQUIRE(isMenu(lan));
        BOOST_TEST_REQUIRE(router().GetSlot(pad) == 0u);
    }

    static bool isMenu(const bool lan)
    {
        return lan ? desktopAs<dskLAN>() != nullptr : desktopAs<dskDirectIP>() != nullptr;
    }

    void focusUntil(const Window* target)
    {
        for(unsigned i = 0; i < 25 && focused(0) != target; ++i)
            press(pad, PadButton::RightShoulder);
        for(unsigned i = 0; i < 25 && focused(0) != target; ++i)
            press(pad, PadButton::LeftShoulder);
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
        BOOST_FAIL("Expected network-menu button missing"); // LCOV_EXCL_LINE
    }

    void expectMultiplayer()
    {
        BOOST_TEST(desktopAs<dskMultiPlayer>() != nullptr);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(video.padEvents_.empty());
        frame();
        BOOST_TEST(desktopAs<dskMultiPlayer>() != nullptr);
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(MenuPadNetworkReturnTests)

BOOST_FIXTURE_TEST_CASE(BReturnsFromEveryFocusedNetworkMenuControl, NetworkMenuReturnFixture)
{
    for(const bool lan : {false, true})
    {
        enter(lan);
        std::vector<unsigned> ids;
        for(const auto* button : desktop()->GetCtrls<ctrlButton>())
            ids.push_back(button->GetID());
        if(lan)
        {
            const auto tables = desktop()->GetCtrls<ctrlTable>();
            BOOST_TEST_REQUIRE(tables.size() == 1u);
            ids.push_back(tables.front()->GetID());
        }
        BOOST_TEST_REQUIRE(ids.size() == (lan ? 4u : 3u));
        for(const auto id : ids)
        {
            enter(lan, lan);
            focusUntil(desktop()->GetCtrl<Window>(id));
            press(pad, PadButton::B);
            expectMultiplayer();
        }
    }
}

BOOST_FIXTURE_TEST_CASE(BClosesJoinDialogBeforeLeavingDirectIP, NetworkMenuReturnFixture)
{
    enter(false);
    focusAtY(210);
    press(pad, PadButton::A);
    BOOST_TEST_REQUIRE(dynamic_cast<iwDirectIPConnect*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
    press(pad, PadButton::B);
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    BOOST_TEST_REQUIRE(isMenu(false));
    press(pad, PadButton::B);
    expectMultiplayer();
}

BOOST_FIXTURE_TEST_CASE(BRetainsCreateDialogUntilItsExplicitBackAction, NetworkMenuReturnFixture)
{
    for(const bool lan : {false, true})
    {
        enter(lan);
        focusAtY(lan ? 250 : 180);
        press(pad, PadButton::A);
        auto* create = dynamic_cast<iwDirectIPCreate*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(create != nullptr);
        press(pad, PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == create);
        BOOST_TEST_REQUIRE(isMenu(lan));
        focusUntil(create->GetCtrl<ctrlButton>(8)); // The window's explicit Back action.
        press(pad, PadButton::A);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST_REQUIRE(isMenu(lan));
        press(pad, PadButton::B);
        expectMultiplayer();
    }
}

BOOST_FIXTURE_TEST_CASE(BDoesNotBypassTheProxyWarning, NetworkMenuReturnFixture)
{
    proxyType = ProxyType::Socks5;
    for(const bool lan : {false, true})
    {
        enter(lan);
        focusAtY(lan ? 250 : 180);
        press(pad, PadButton::A);
        auto* warning = dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(warning != nullptr);
        press(pad, PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == warning);
        BOOST_TEST_REQUIRE(isMenu(lan));
        press(pad, PadButton::A);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST_REQUIRE(isMenu(lan));
        press(pad, PadButton::B);
        expectMultiplayer();
    }
}

BOOST_FIXTURE_TEST_CASE(StartAndNavigationDoNotLeaveNetworkMenus, NetworkMenuReturnFixture)
{
    for(const bool lan : {false, true})
    {
        enter(lan);
        for(const auto button : {PadButton::Start, PadButton::DpadDown, PadButton::RightShoulder})
        {
            press(pad, button);
            BOOST_TEST_REQUIRE(isMenu(lan));
            BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        }
    }
}

BOOST_FIXTURE_TEST_CASE(VisibleBackStillWorksWithMouseAndControllerA, NetworkMenuReturnFixture)
{
    for(const bool lan : {false, true})
    {
        const int backY = lan ? 530 : 250;
        enter(lan);
        focusAtY(backY);
        press(pad, PadButton::A);
        expectMultiplayer();
        enter(lan);
        MouseCoords mc(Position(lan ? 650 : 225, backY + 10));
        mc.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mc);
        mc.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mc);
        frame();
        expectMultiplayer();
    }
}

BOOST_FIXTURE_TEST_CASE(KeyboardEscapeRetainsExistingMenuAndWindowBehavior, NetworkMenuReturnFixture)
{
    for(const bool lan : {false, true})
    {
        enter(lan);
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
        frame();
        BOOST_TEST_REQUIRE(isMenu(lan));
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        if(!lan)
        {
            focusAtY(210);
            press(pad, PadButton::A);
            BOOST_TEST_REQUIRE(dynamic_cast<iwDirectIPConnect*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
            WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
            frame();
            BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
            BOOST_TEST_REQUIRE(isMenu(lan));
        }
        focusAtY(lan ? 250 : 180);
        press(pad, PadButton::A);
        auto* create = dynamic_cast<iwDirectIPCreate*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(create != nullptr);
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
        frame();
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == create);
        BOOST_TEST_REQUIRE(isMenu(lan));
        MouseCoords mc(create->GetDrawPos() + Position(180, 250));
        mc.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mc);
        mc.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mc);
        frame();
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST_REQUIRE(isMenu(lan));
    }
}

BOOST_FIXTURE_TEST_CASE(BackBurstStaysAtMultiplayerUntilANewPress, NetworkMenuReturnFixture)
{
    for(const bool lan : {false, true})
    {
        enter(lan);
        tap(pad, PadButton::B);
        tap(pad, PadButton::B);
        frame();
        expectMultiplayer();
        press(pad, PadButton::B);
        BOOST_TEST_REQUIRE(desktopAs<dskMainMenu>() != nullptr);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    }
}

BOOST_AUTO_TEST_SUITE_END()
