// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AsyncChecksum.h"
#include "LocalGameFixture.h"
#include "PadFixture.h"
#include "controls/ctrlImageButton.h"
#include "controls/ctrlIngameMinimap.h"
#include "controls/ctrlTextButton.h"
#include "driver/MouseCoords.h"
#include "helpers/EnumRange.h"
#include "ingameWindows/iwMinimap.h"
#include "ingameWindows/iwPadSystemMenu.h"
#include <boost/test/unit_test.hpp>
#include <memory>
#include <stdexcept>

namespace {
constexpr PadDeviceId pad = 113;

[[noreturn]] void throwCleanupProbe()
{
    throw std::runtime_error("minimap cleanup probe");
}

struct MinimapPadFixture : uiHelper::Fixture, rttr::test::LocalGameFixture
{
    rttr::test::PadFeeder pads{*uiHelper::GetVideoDriver()};
    rttr::test::TestableGameInterface* dsk = nullptr;
    iwMinimap* window = nullptr;
    const VideoMode savedWindowSize = VIDEODRIVER.GetWindowSize();
    const DisplayMode savedDisplayMode = VIDEODRIVER.GetDisplayMode();
    const decltype(SETTINGS.video) savedVideo = SETTINGS.video;
    const bool savedExtended = SETTINGS.ingame.minimapExtended;
    const bool savedDebugMode = SETTINGS.global.debugMode;
    const decltype(SETTINGS.windows.persistentSettings) savedWindows = SETTINGS.windows.persistentSettings;

    template<class F>
    void run(F&& body, const VideoMode size = VideoMode(800, 600))
    {
        try
        {
            beginGame(size);
            body();
        } catch(...)
        {
            cleanup();
            throw;
        }
        cleanup();
        expectRestored();
    }
    void expectRestored() const
    {
        BOOST_TEST((VIDEODRIVER.GetWindowSize() == savedWindowSize));
        BOOST_TEST((VIDEODRIVER.GetDisplayMode() == savedDisplayMode));
        BOOST_TEST((SETTINGS.video.windowedSize == savedVideo.windowedSize));
        BOOST_TEST((SETTINGS.video.fullscreenSize == savedVideo.fullscreenSize));
        BOOST_TEST((SETTINGS.video.displayMode == savedVideo.displayMode));
        BOOST_TEST(SETTINGS.ingame.minimapExtended == savedExtended);
        BOOST_TEST(SETTINGS.global.debugMode == savedDebugMode);
        BOOST_TEST_REQUIRE(SETTINGS.windows.persistentSettings.size() == savedWindows.size());
        for(const auto& entry : savedWindows)
        {
            const auto it = SETTINGS.windows.persistentSettings.find(entry.first);
            BOOST_TEST_REQUIRE((it != SETTINGS.windows.persistentSettings.end()));
            const auto& actual = it->second;
            BOOST_TEST(actual.isOpen == entry.second.isOpen);
            BOOST_TEST(actual.isMinimized == entry.second.isMinimized);
            BOOST_TEST(actual.isPinned == entry.second.isPinned);
            BOOST_TEST((actual.lastPos == entry.second.lastPos));
            BOOST_TEST((actual.restorePos == entry.second.restorePos));
        }
    }
    void cleanup()
    {
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        WINDOWMANAGER.Draw();
        if(ci().game)
            world().SetGameInterface(nullptr);
        dsk = nullptr;
        window = nullptr;
        // Windows retain pointers into persistent settings until they are destroyed.
        SETTINGS.windows.persistentSettings = savedWindows;
        SETTINGS.global.debugMode = savedDebugMode;
        SETTINGS.ingame.minimapExtended = savedExtended;
        VIDEODRIVER.ResizeScreen(savedWindowSize, savedDisplayMode);
        SETTINGS.video = savedVideo;
    }
    void beginGame(const VideoMode size)
    {
        SETTINGS.global.debugMode = false;
        hostAndEnterLobby();
        ci().numErrors = 0;
        lobby().SetPlayerState(1, PlayerState::AI, AI::Info(AI::Type::Dummy));
        lobby().SetPlayerState(2, PlayerState::AI, AI::Info(AI::Type::Dummy));
        pumpUntil(
          [] {
              const auto lobby = GAMECLIENT.GetGameLobby();
              return lobby->getPlayer(1).ps == PlayerState::AI && lobby->getPlayer(2).ps == PlayerState::AI;
          },
          "dummy AI slots");
        startGame();
        for(auto& entry : SETTINGS.windows.persistentSettings)
            entry.second.isOpen = false;
        VIDEODRIVER.ResizeScreen(size, DisplayMode::Windowed);
        LOADER.LoadDummyMapFiles();
        auto desktop = std::make_unique<rttr::test::TestableGameInterface>(ci().game, GAMECLIENT.GetNWFInfo(),
                                                                           GAMECLIENT.GetPlayerId(), false);
        dsk = desktop.get();
        WINDOWMANAGER.Switch(std::move(desktop));
        WINDOWMANAGER.Draw();
        GAMECLIENT.SetInterface(&ci());
        BOOST_TEST_REQUIRE(dsk->GetNumViews() == 1u);
        pads.pickUp(pad);
        frame();
        BOOST_TEST_REQUIRE(dsk->GetPlayerView(0).HasPadCursor());
    }
    void frame()
    {
        dsk->UpdateInput(16, Position(-10000, -10000));
        WINDOWMANAGER.Draw();
        GAMECLIENT.SetInterface(&ci());
    }
    void press(const PadButton button)
    {
        pads.tap(pad, button);
        frame();
    }
    void enter()
    {
        press(PadButton::Back);
        auto* system = dynamic_cast<iwPadSystemMenu*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(system != nullptr);
        // Browse away and back: entry must exercise the actual system ring too.
        press(PadButton::DpadRight);
        press(PadButton::DpadLeft);
        BOOST_TEST_REQUIRE(dsk->GetPlayerView(0).GetFocus().GetFocused()
                           == system->GetCtrl<ctrlTextButton>(iwPadSystemMenu::ID_MINIMAP));
        press(PadButton::A);
        window = dynamic_cast<iwMinimap*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(window != nullptr);
        BOOST_TEST_REQUIRE(dsk->GetPlayerView(0).GetFocus().GetRoot() == window);
    }
    void focusTo(const unsigned id)
    {
        const auto* target = window->GetCtrl<ctrlImageButton>(id);
        BOOST_TEST_REQUIRE(target != nullptr);
        auto& focus = dsk->GetPlayerView(0).GetFocus();
        for(const auto direction : {PadButton::RightShoulder, PadButton::LeftShoulder})
            for(unsigned i = 0; i < 8 && focus.GetFocused() != target; ++i)
                press(direction);
        BOOST_TEST_REQUIRE(focus.GetFocused() == target);
    }
    void activate(const unsigned id)
    {
        focusTo(id);
        press(PadButton::A);
    }
    void close()
    {
        // First B returns from cursor mode after mouse input; the next closes the window.
        press(PadButton::Y);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        window = nullptr;
    }
    const ctrlIngameMinimap& map() const
    {
        const auto* result = window->GetCtrl<ctrlIngameMinimap>(0);
        BOOST_TEST_REQUIRE(result != nullptr);
        return *result;
    }
    void expectBounds() const
    {
        const auto area = map().GetMapDrawArea();
        const auto origin = window->GetDrawPos();
        const auto size = window->GetSize();
        BOOST_TEST(area.getSize().x > 0u);
        BOOST_TEST(area.getSize().y > 0u);
        BOOST_TEST(area.getOrigin().x >= origin.x);
        BOOST_TEST(area.getOrigin().y >= origin.y);
        BOOST_TEST(area.getOrigin().x + static_cast<int>(area.getSize().x) <= origin.x + static_cast<int>(size.x));
        BOOST_TEST(area.getOrigin().y + static_cast<int>(area.getSize().y) <= origin.y + static_cast<int>(size.y));
        for(unsigned id = 1; id <= 4; ++id)
        {
            const auto* button = window->GetCtrl<ctrlImageButton>(id);
            BOOST_TEST_REQUIRE(button != nullptr);
            BOOST_TEST(button->GetPos().x >= 0);
            BOOST_TEST(button->GetPos().y >= static_cast<int>(map().GetPos().y + map().GetSize().y));
            BOOST_TEST(button->GetPos().x + static_cast<int>(button->GetSize().x) <= static_cast<int>(size.x));
            BOOST_TEST(button->GetPos().y + static_cast<int>(button->GetSize().y) <= static_cast<int>(size.y));
        }
    }
    DrawPoint expectedOffset(const MapPoint point)
    {
        auto result = world().GetNodePos(point) - dsk->GetPlayerView(0).GetView().GetSize() / 2u;
        const DrawPoint mapSize(world().GetWidth() * TR_W, world().GetHeight() * TR_H);
        result.x = (result.x % mapSize.x + mapSize.x) % mapSize.x;
        result.y = (result.y % mapSize.y + mapSize.y) % mapSize.y;
        return result;
    }
    void expectWorld(const Inventory& inventory, const AsyncChecksum& checksum)
    {
        for(const auto good : helpers::EnumRange<GoodType>{})
            BOOST_TEST(world().GetPlayer(0).GetInventory()[good] == inventory[good]);
        for(const auto job : helpers::EnumRange<Job>{})
            BOOST_TEST(world().GetPlayer(0).GetInventory()[job] == inventory[job]);
        BOOST_TEST((AsyncChecksum::create(*ci().game) == checksum));
        BOOST_TEST(ci().numErrors == 0u);
        BOOST_TEST(ci().numAsync == 0u);
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(PadMinimapTests)

BOOST_FIXTURE_TEST_CASE(ControllerZoomPersistsThroughCloseAndReopenWithoutWorldChanges, MinimapPadFixture)
{
    run([this] {
        SETTINGS.ingame.minimapExtended = false;
        const auto inventory = world().GetPlayer(0).GetInventory();
        const auto checksum = AsyncChecksum::create(*ci().game);
        enter();
        const auto smallMap = map().GetCurMapSize();
        const auto smallWindow = window->GetSize();
        expectBounds();
        activate(4);
        BOOST_TEST(SETTINGS.ingame.minimapExtended);
        const auto bigMap = map().GetCurMapSize();
        const auto bigWindow = window->GetSize();
        BOOST_TEST(bigMap.x > smallMap.x);
        BOOST_TEST(bigMap.y > smallMap.y);
        BOOST_TEST(bigWindow.x > smallWindow.x);
        BOOST_TEST(bigWindow.y > smallWindow.y);
        expectBounds();
        close();
        enter();
        BOOST_TEST((map().GetCurMapSize() == bigMap));
        BOOST_TEST((window->GetSize() == bigWindow));
        activate(4);
        BOOST_TEST(!SETTINGS.ingame.minimapExtended);
        BOOST_TEST((map().GetCurMapSize() == smallMap));
        BOOST_TEST((window->GetSize() == smallWindow));
        press(PadButton::RightShoulder);
        BOOST_TEST(dsk->GetPlayerView(0).GetFocus().GetFocused() == window->GetCtrl<ctrlImageButton>(4));
        for(unsigned id = 3; id > 0; --id)
        {
            press(PadButton::LeftShoulder);
            BOOST_TEST(dsk->GetPlayerView(0).GetFocus().GetFocused() == window->GetCtrl<ctrlImageButton>(id));
        }
        press(PadButton::LeftShoulder);
        BOOST_TEST(dsk->GetPlayerView(0).GetFocus().GetFocused() == window->GetCtrl<ctrlImageButton>(1));
        close();
        enter();
        BOOST_TEST((map().GetCurMapSize() == smallMap));
        expectBounds();
        expectWorld(inventory, checksum);
    });
}

BOOST_FIXTURE_TEST_CASE(MouseClickAndDragCenterOnIndependentMapCoordinatesAtBothZoomLevels, MinimapPadFixture)
{
    run([this] {
        SETTINGS.ingame.minimapExtended = false;
        const auto inventory = world().GetPlayer(0).GetInventory();
        const auto checksum = AsyncChecksum::create(*ci().game);
        enter();
        for(unsigned zoom = 0; zoom < 2; ++zoom)
        {
            const auto area = map().GetMapDrawArea();
            const auto size = area.getSize();
            const DrawPoint first(size.x / 4u, size.y / 3u);
            const DrawPoint last(size.x - 1u, size.y - 1u);
            const auto mapPoint = [this, size](const DrawPoint pixel) {
                return MapPoint(static_cast<unsigned>(pixel.x) * world().GetWidth() / size.x,
                                static_cast<unsigned>(pixel.y) * world().GetHeight() / size.y);
            };
            MouseCoords down(area.getOrigin() + first);
            down.ldown = true;
            WINDOWMANAGER.Msg_LeftDown(down);
            frame();
            BOOST_TEST((dsk->GetPlayerView(0).GetView().GetOffset() == expectedOffset(mapPoint(first))));
            MouseCoords drag(area.getOrigin() + last);
            drag.ldown = true;
            WINDOWMANAGER.Msg_MouseMove(drag);
            frame();
            BOOST_TEST((dsk->GetPlayerView(0).GetView().GetOffset() == expectedOffset(mapPoint(last))));
            // Leaving the map while held and an unheld move over the map must not scroll it.
            const auto offset = dsk->GetPlayerView(0).GetView().GetOffset();
            drag.pos = area.getOrigin() + DrawPoint(-1, -1);
            WINDOWMANAGER.Msg_MouseMove(drag);
            frame();
            BOOST_TEST((dsk->GetPlayerView(0).GetView().GetOffset() == offset));
            WINDOWMANAGER.Msg_LeftUp(MouseCoords(area.getOrigin() + last));
            WINDOWMANAGER.Msg_MouseMove(MouseCoords(area.getOrigin() + first));
            frame();
            BOOST_TEST((dsk->GetPlayerView(0).GetView().GetOffset() == offset));
            activate(4);
        }
        expectWorld(inventory, checksum);
    });
}

BOOST_FIXTURE_TEST_CASE(MouseZoomAndBidirectionalButtonFocusLeaveWorldUntouched, MinimapPadFixture)
{
    run(
      [this] {
          SETTINGS.ingame.minimapExtended = true;
          const auto inventory = world().GetPlayer(0).GetInventory();
          const auto checksum = AsyncChecksum::create(*ci().game);
          enter();
          const auto bigMap = map().GetCurMapSize();
          focusTo(4);
          focusTo(1);
          const auto* zoom = window->GetCtrl<ctrlImageButton>(4);
          const MouseCoords click(zoom->GetDrawPos() + zoom->GetSize() / 2u);
          WINDOWMANAGER.Msg_LeftDown(click);
          WINDOWMANAGER.Msg_LeftUp(click);
          frame();
          BOOST_TEST(!SETTINGS.ingame.minimapExtended);
          BOOST_TEST(map().GetCurMapSize().x < bigMap.x);
          expectBounds();
          expectWorld(inventory, checksum);
      },
      VideoMode(1280, 800));
}

BOOST_AUTO_TEST_CASE(ExceptionalCleanupRestoresSettingsWhileFixtureStillExists)
{
    MinimapPadFixture fixture;
    bool reachedProbe = false;
    BOOST_CHECK_THROW(fixture.run([&fixture, &reachedProbe] {
        fixture.enter();
        fixture.activate(4);
        reachedProbe = true;
        throwCleanupProbe();
    }),
                      std::runtime_error);
    BOOST_TEST(reachedProbe);
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    fixture.expectRestored();
}

BOOST_AUTO_TEST_SUITE_END()
