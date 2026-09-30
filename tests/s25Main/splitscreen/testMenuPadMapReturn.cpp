// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MenuPadFixture.h"
#include "WindowManager.h"
#include "desktops/dskDirectIP.h"
#include "desktops/dskLAN.h"
#include "desktops/dskSelectMap.h"
#include "desktops/dskSinglePlayer.h"
#include "driver/MouseCoords.h"
#include "ingameWindows/IngameWindow.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/test/unit_test.hpp>
#include <memory>

namespace {
constexpr PadDeviceId pad = 91;

struct MapReturnFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder userData;
    rttr::test::ConfigOverride userDataOverride{"USERDATA", userData};

    void enter(const ServerType type)
    {
        padInput().Reset();
        WINDOWMANAGER.Switch(std::make_unique<dskSelectMap>(CreateServerInfo(type, 12345, "Controller return")));
        frame();
        pickUp(pad);
        BOOST_TEST_REQUIRE(desktopAs<dskSelectMap>() != nullptr);
        BOOST_TEST_REQUIRE(router().GetSlot(pad) == 0u);
    }

    bool isExpectedParent(const ServerType type) const
    {
        if(type == ServerType::Local)
            return desktopAs<dskSinglePlayer>() != nullptr;
        if(type == ServerType::LAN)
            return desktopAs<dskLAN>() != nullptr;
        // The fixture is not logged into the public lobby: its existing fallback is Direct-IP.
        return desktopAs<dskDirectIP>() != nullptr;
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(MenuPadMapReturnTests)

BOOST_FIXTURE_TEST_CASE(BReturnsToTheOriginalMenu, MapReturnFixture)
{
    for(const auto type : {ServerType::Local, ServerType::Direct, ServerType::LAN, ServerType::Lobby})
    {
        enter(type);
        press(pad, PadButton::B);
        BOOST_TEST_REQUIRE(isExpectedParent(type));
        BOOST_TEST(video.padEvents_.empty());
    }
}

BOOST_FIXTURE_TEST_CASE(MouseBackRetainsTheSameDestinations, MapReturnFixture)
{
    for(const auto type : {ServerType::Local, ServerType::Direct, ServerType::LAN, ServerType::Lobby})
    {
        enter(type);
        MouseCoords mc(Position(450, 570));
        WINDOWMANAGER.Msg_MouseMove(mc);
        mc.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mc);
        mc.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mc);
        frame();
        BOOST_TEST_REQUIRE(isExpectedParent(type));
    }
}

BOOST_FIXTURE_TEST_CASE(BClosesTheLoadDialogBeforeLeavingCreateGame, MapReturnFixture)
{
    enter(ServerType::Local);
    for(unsigned i = 0; i < 20 && focusedId(0) != 4u; ++i)
        press(pad, PadButton::RightShoulder);
    BOOST_TEST_REQUIRE(focusedId(0) == 4u); // Load game
    press(pad, PadButton::A);
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() != nullptr);
    press(pad, PadButton::B);
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    BOOST_TEST_REQUIRE(desktopAs<dskSelectMap>() != nullptr);
    press(pad, PadButton::B);
    BOOST_TEST(desktopAs<dskSinglePlayer>() != nullptr);
}

BOOST_FIXTURE_TEST_CASE(StartAndNavigationDoNotLeaveCreateGame, MapReturnFixture)
{
    enter(ServerType::Local);
    for(const auto button : {PadButton::Start, PadButton::DpadLeft, PadButton::RightShoulder})
    {
        press(pad, button);
        BOOST_TEST_REQUIRE(desktopAs<dskSelectMap>() != nullptr);
        BOOST_TEST(video.padEvents_.empty());
    }
}

BOOST_FIXTURE_TEST_CASE(BDoesNotDismissCustomMapSettingsOrLeaveCreateGame, MapReturnFixture)
{
    enter(ServerType::Local);
    for(unsigned i = 0; i < 20 && focusedId(0) != 7u; ++i)
        press(pad, PadButton::RightShoulder);
    BOOST_TEST_REQUIRE(focusedId(0) == 7u); // Random-map settings require explicit confirmation.
    press(pad, PadButton::A);
    const auto* settingsWindow = WINDOWMANAGER.GetTopMostWindow();
    BOOST_TEST_REQUIRE(settingsWindow != nullptr);
    press(pad, PadButton::B);
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == settingsWindow);
    BOOST_TEST(desktopAs<dskSelectMap>() != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
