// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GameLobby.h"
#include "ILobbyClient.hpp"
#include "JoinPlayerInfo.h"
#include "LocalGameFixture.h"
#include "MenuPadFixture.h"
#include "controls/ctrlCheck.h"
#include "controls/ctrlComboBox.h"
#include "controls/ctrlTextButton.h"
#include "desktops/dskGameLobby.h"
#include "gameData/MaxPlayers.h"
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>

namespace {
constexpr PadDeviceId hostPad = 110;
constexpr PadDeviceId guestPad = 111;
constexpr unsigned seatButton = 48 + 2 * MAX_PLAYERS;
constexpr unsigned seatLabel = seatButton + MAX_VIEWPORTS;
constexpr unsigned togetherId = seatLabel + 7;
constexpr unsigned goalsId = 21;
constexpr std::array<VideoMode, 4> sizes{{{800, 600}, {1280, 800}, {1280, 720}, {1920, 1080}}};

bool overlaps(const Rect& a, const Rect& b)
{
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

struct LobbyLayoutFixture : rttr::test::LocalGameFixture, rttr::test::MenuPadFixture
{
    const decltype(SETTINGS.video) savedVideo = SETTINGS.video;
    const VideoMode savedSize = VIDEODRIVER.GetWindowSize();
    const DisplayMode savedDisplayMode = VIDEODRIVER.GetDisplayMode();

    void frame() override
    {
        GAMECLIENT.Run();
        GAMESERVER.Run();
        MenuPadFixture::frame();
    }

    void enter(const VideoMode size, const bool campaign = false)
    {
        hostAndEnterLobby();
        GAMECLIENT.SetHostingCampaign(campaign);
        VIDEODRIVER.ResizeScreen(size, DisplayMode::Windowed);
        WINDOWMANAGER.Switch(std::make_unique<dskGameLobby>(ServerType::Local, GAMECLIENT.GetGameLobby(),
                                                            GAMECLIENT.GetPlayerId(), nullptr));
        frame();
    }

    static dskGameLobby& screen()
    {
        auto* lobby = desktopAs<dskGameLobby>();
        BOOST_TEST_REQUIRE(lobby != nullptr);
        return *lobby;
    }

    static ctrlCheck& together()
    {
        auto* check = screen().GetCtrl<ctrlCheck>(togetherId);
        BOOST_TEST_REQUIRE(check != nullptr);
        return *check;
    }

    void cleanup()
    {
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        WINDOWMANAGER.Draw();
        VIDEODRIVER.ResizeScreen(savedSize, savedDisplayMode);
        SETTINGS.video = savedVideo;
    }

    template<class Body>
    void guarded(const Body& body)
    {
        try
        {
            body();
        } catch(...)
        {
            cleanup();
            throw;
        }
        cleanup();
    }

    void focusTogether()
    {
        // Start at the first control even when the previous toggle left focus at the end.
        for(unsigned i = 0; i < 100; ++i)
        {
            const auto* before = focused(0);
            press(hostPad, PadButton::LeftShoulder);
            if(focused(0) == before)
                break;
        }
        for(unsigned i = 0; i < 100 && focused(0) != &together(); ++i)
            press(hostPad, PadButton::RightShoulder);
        BOOST_TEST_REQUIRE(focused(0) == &together());
    }

    void mouseToggle()
    {
        // ctrlCheck toggles on mouse-down, unlike the neighbouring buttons.
        MouseCoords mouse(together().GetDrawRect().getOrigin() + DrawPoint(5, 5));
        WINDOWMANAGER.Msg_MouseMove(mouse);
        mouse.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mouse);
        mouse.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mouse);
        frame();
    }

    static void expectSeparated()
    {
        const auto& lobby = screen();
        const auto* check = lobby.GetCtrl<ctrlCheck>(togetherId);
        BOOST_TEST_REQUIRE(check != nullptr);
        BOOST_TEST_REQUIRE(check->IsVisible());
        const Rect rect = check->GetDrawRect();
        const Rect bounds = lobby.GetDrawRect();
        BOOST_TEST(rect.left >= bounds.left);
        BOOST_TEST(rect.top >= bounds.top);
        BOOST_TEST(rect.right <= bounds.right);
        BOOST_TEST(rect.bottom <= bounds.bottom);
        // Include labels, map preview and settings as well as every seat card. This catches
        // a move that fixes Goals but covers a different control instead.
        for(const auto* control : lobby.GetCtrls<Window>())
        {
            if(control == check || !control->IsVisible())
                continue;
            BOOST_TEST_CONTEXT("Neighbour control " << control->GetID())
            {
                BOOST_TEST(!overlaps(rect, control->GetBoundaryRect()));
            }
        }
        const unsigned numCards = check->isChecked() ?
                                    MAX_VIEWPORTS :
                                    std::min<unsigned>(MAX_VIEWPORTS, GAMECLIENT.GetGameLobby()->getNumPlayers());
        for(unsigned i = 0; i < numCards; ++i)
        {
            const auto* card = lobby.GetCtrl<ctrlTextButton>(seatButton + i);
            BOOST_TEST_REQUIRE(card != nullptr);
            const Rect cardRect = card->GetDrawRect();
            BOOST_TEST(cardRect.bottom < rect.top);
            BOOST_TEST(cardRect.left >= bounds.left);
            BOOST_TEST(cardRect.right <= bounds.right);
        }
    }

    void expectSharedJoin()
    {
        pickUp(guestPad);
        press(guestPad, PadButton::A);
        frame();
        BOOST_TEST(GAMECLIENT.GetSharedLocalViews() == 1u);
        BOOST_TEST(GAMECLIENT.GetAdditionalLocalPlayers().empty());
        BOOST_TEST(router().GetSlot(guestPad) == 1u);
        press(guestPad, PadButton::B);
        frame();
        BOOST_TEST(GAMECLIENT.GetSharedLocalViews() == 0u);
    }
};

[[noreturn]] void throwAfterLayoutProbe()
{
    throw std::runtime_error("layout cleanup probe");
}
} // namespace

BOOST_AUTO_TEST_SUITE(MenuPadLobbyLayoutTests)

BOOST_FIXTURE_TEST_CASE(SeatModeIsSeparateAtCommonResolutionsAndAfterResize, LobbyLayoutFixture)
{
    guarded([&] {
        enter(sizes.front());
        pickUp(hostPad);
        for(const auto size : sizes)
        {
            VIDEODRIVER.ResizeScreen(size, DisplayMode::Windowed);
            frame();
            expectSeparated();
            focusTogether();
            press(hostPad, PadButton::A);
            BOOST_TEST_REQUIRE(together().isChecked());
            // Toggling rebuilds the cards at the current resolution.
            expectSeparated();
            expectSharedJoin();
            focusTogether();
            press(hostPad, PadButton::A);
            BOOST_TEST_REQUIRE(!together().isChecked());
            expectSeparated();
        }
    });
}

BOOST_FIXTURE_TEST_CASE(MouseToggleKeepsGameGoalsAndPlayerSlots, LobbyLayoutFixture)
{
    guarded([&] {
        enter(VideoMode(1280, 800));
        pickUp(hostPad);
        auto* goals = screen().GetCtrl<ctrlComboBox>(goalsId);
        BOOST_TEST_REQUIRE(goals != nullptr);
        const auto selection = goals->GetSelection();
        const auto objective = GAMECLIENT.GetGameLobby()->getSettings().objective;
        const auto playerState = GAMECLIENT.GetGameLobby()->getPlayer(1).ps;
        mouseToggle();
        BOOST_TEST_REQUIRE(together().isChecked());
        expectSeparated();
        expectSharedJoin();
        mouseToggle();
        BOOST_TEST(!together().isChecked());
        BOOST_TEST((goals->GetSelection() == selection));
        BOOST_TEST((GAMECLIENT.GetGameLobby()->getSettings().objective == objective));
        BOOST_TEST((GAMECLIENT.GetGameLobby()->getPlayer(1).ps == playerState));
    });
}

BOOST_FIXTURE_TEST_CASE(CampaignModeIsSeparateAndReadonly, LobbyLayoutFixture)
{
    guarded([&] {
        enter(VideoMode(1280, 800), true);
        pickUp(hostPad);
        BOOST_TEST_REQUIRE(together().isChecked());
        BOOST_TEST_REQUIRE(together().isReadOnly());
        expectSeparated();
        mouseToggle();
        BOOST_TEST(together().isChecked());
        expectSharedJoin();
    });
}

BOOST_FIXTURE_TEST_CASE(SeatModeVisibilityFollowsConnectedPads, LobbyLayoutFixture)
{
    guarded([&] {
        enter(VideoMode(800, 600));
        BOOST_TEST(!together().IsVisible());
        connect(hostPad);
        frame();
        expectSeparated();
        disconnect(hostPad);
        frame();
        BOOST_TEST(!together().IsVisible());
    });
}

BOOST_FIXTURE_TEST_CASE(ExceptionRestoresVideoBeforeFixtureDestruction, LobbyLayoutFixture)
{
    bool reachedProbe = false;
    BOOST_CHECK_THROW(guarded([&] {
                          enter(VideoMode(1280, 800));
                          pickUp(hostPad);
                          expectSeparated();
                          reachedProbe = true;
                          throwAfterLayoutProbe();
                      }),
                      std::runtime_error);
    BOOST_TEST(reachedProbe);
    BOOST_TEST(desktopAs<dskGameLobby>() == nullptr);
    BOOST_TEST((VIDEODRIVER.GetWindowSize() == savedSize));
    BOOST_TEST((VIDEODRIVER.GetDisplayMode() == savedDisplayMode));
    BOOST_TEST((SETTINGS.video.windowedSize == savedVideo.windowedSize));
}

BOOST_AUTO_TEST_SUITE_END()
