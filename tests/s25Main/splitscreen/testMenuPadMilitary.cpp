// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "LocalGameFixture.h"
#include "PadFixture.h"
#include "controls/ctrlImageButton.h"
#include "controls/ctrlProgress.h"
#include "controls/ctrlTextButton.h"
#include "driver/MouseCoords.h"
#include "ingameWindows/iwHelp.h"
#include "ingameWindows/iwMainMenu.h"
#include "ingameWindows/iwMilitary.h"
#include "ingameWindows/iwPadSystemMenu.h"
#include "gameData/SettingTypeConv.h"
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>
#include <array>
#include <chrono>
#include <memory>
#include <set>
#include <stdexcept>
#include <thread>

namespace {
constexpr PadDeviceId pad = 105;

[[noreturn]] void throwCleanupProbe()
{
    throw std::runtime_error("military cleanup probe");
}

struct MilitaryPadFixture : uiHelper::Fixture, rttr::test::LocalGameFixture
{
    rttr::test::PadFeeder pads{*uiHelper::GetVideoDriver()};
    rttr::test::TestableGameInterface* dsk = nullptr;
    iwMilitary* window = nullptr;
    const bool savedDebugMode = SETTINGS.global.debugMode;

    template<class F>
    void run(const unsigned policy, F&& body)
    {
        try
        {
            beginGame(policy);
            body();
        } catch(...)
        {
            cleanup();
            throw;
        }
        cleanup();
    }

    void cleanup()
    {
        cleanupDesktop();
        SETTINGS.global.debugMode = savedDebugMode;
    }

    void cleanupDesktop()
    {
        // The windows retain their viewer/factory: destroy them before stopping the backend.
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        WINDOWMANAGER.Draw();
        if(ci().game)
            world().SetGameInterface(nullptr);
        dsk = nullptr;
        window = nullptr;
    }

    void beginGame(const unsigned policy)
    {
        SETTINGS.global.debugMode = false;
        hostAndEnterLobby();
        lobby().SetPlayerState(1, PlayerState::AI, AI::Info(AI::Type::Dummy));
        lobby().SetPlayerState(2, PlayerState::AI, AI::Info(AI::Type::Dummy));
        pumpUntil(
          [] {
              const auto lobby = GAMECLIENT.GetGameLobby();
              return lobby->getPlayer(1).ps == PlayerState::AI && lobby->getPlayer(2).ps == PlayerState::AI;
          },
          "dummy AI configuration");
        const auto before = GAMECLIENT.GetGameLobby()->getSettings();
        auto settings = before;
        settings.setSelection(AddonId::DEFENDER_BEHAVIOR, (policy & 1u) ? 1 : 0);
        settings.setSelection(AddonId::SEA_ATTACK, (policy & 2u) ? 0 : 2);
        lobby().ChangeGlobalGameSettings(settings);
        // The lobby edits its copy eagerly; only the real broadcast proves this configuration.
        GAMECLIENT.GetGameLobby()->getSettings() = before;
        GAMECLIENT.GetGameLobby()->getSettings().setSelection(AddonId::DEFENDER_BEHAVIOR, 2);
        pumpUntil(
          [policy] {
              const auto& received = GAMECLIENT.GetGameLobby()->getSettings();
              return received.getSelection(AddonId::DEFENDER_BEHAVIOR) == ((policy & 1u) ? 1u : 0u)
                     && received.getSelection(AddonId::SEA_ATTACK) == ((policy & 2u) ? 0u : 2u);
          },
          "military addon settings");
        startGame();
        BOOST_TEST(world().GetGGS().getSelection(AddonId::DEFENDER_BEHAVIOR) == ((policy & 1u) ? 1u : 0u));
        BOOST_TEST(world().GetGGS().getSelection(AddonId::SEA_ATTACK) == ((policy & 2u) ? 0u : 2u));
        makeDesktop();
    }

    void makeDesktop()
    {
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
        // Closing a window may reactivate the desktop and replace the backend observer.
        GAMECLIENT.SetInterface(&ci());
    }
    void press(const PadButton button)
    {
        pads.tap(pad, button);
        frame();
    }
    Window* focused() const { return dsk->GetPlayerView(0).GetFocus().GetFocused(); }

    void focusUntil(const Window* target)
    {
        if(dsk->GetPlayerView(0).GetRing().IsOpen())
        {
            for(unsigned sector = 0; sector < 8 && focused() != target; ++sector)
                press(PadButton::DpadRight);
        } else
        {
            // Shoulder navigation does not wrap; returning to an earlier control needs Left.
            for(const auto direction : {PadButton::RightShoulder, PadButton::LeftShoulder})
            {
                for(unsigned i = 0; i < 40 && focused() != target; ++i)
                    press(direction);
            }
        }
        BOOST_TEST_REQUIRE(focused() == target);
    }

    void enter()
    {
        press(PadButton::Back);
        auto* system = dynamic_cast<iwPadSystemMenu*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(system != nullptr);
        focusUntil(system->GetCtrl<ctrlTextButton>(iwPadSystemMenu::ID_MAIN_SELECTION));
        press(PadButton::A);
        openFromMain();
    }

    void openFromMain()
    {
        auto* main = dynamic_cast<iwMainMenu*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(main != nullptr);
        press(PadButton::Y);
        focusUntil(main->GetCtrl<ctrlImageButton>(8));
        press(PadButton::A);
        window = dynamic_cast<iwMilitary*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(window != nullptr);
        press(PadButton::Y);
        BOOST_TEST_REQUIRE(dsk->GetPlayerView(0).GetFocus().GetRoot() == window);
    }

    ctrlProgress& slider(const unsigned index) const { return *window->GetCtrl<ctrlProgress>(index + 2u); }
    void click(const unsigned id)
    {
        focusUntil(window->GetCtrl<ctrlImageButton>(id));
        press(PadButton::A);
    }
    void mouseClick(const Window& control, const DrawPoint& offset = DrawPoint(10, 5))
    {
        MouseCoords mc(control.GetDrawPos() + offset);
        mc.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mc);
        mc.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mc);
        frame();
    }

    MilitarySettings actual()
    {
        MilitarySettings result{};
        for(unsigned i = 0; i < result.size(); ++i)
            result[i] = world().GetPlayer(0).GetMilitarySetting(i);
        return result;
    }
    void expectControls(const MilitarySettings& expected) const
    {
        for(unsigned i = 0; i < expected.size(); ++i)
            BOOST_TEST(slider(i).GetPosition() == expected[i]);
    }
    void expectWorld(const MilitarySettings& expected)
    {
        const auto settings = actual();
        for(unsigned i = 0; i < expected.size(); ++i)
            BOOST_TEST(settings[i] == expected[i]);
        BOOST_TEST(ci().numErrors == 0u);
        BOOST_TEST(ci().numAsync == 0u);
        BOOST_TEST(ci().numReplayAsync == 0u);
    }
    void awaitWorld(const MilitarySettings& expected)
    {
        if(!GAMECLIENT.IsReplayModeOn())
            pumpUntilGF(GAMECLIENT.GetGFNumber() + 2u * GAMECLIENT.GetNWFLength());
        pumpUntil([this, &expected] { return actual() == expected; }, "military settings to reach the actual world",
                  std::chrono::seconds(10));
        expectWorld(expected);
    }
    void closeAndRoundtrip(const MilitarySettings& expected)
    {
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(dynamic_cast<iwMainMenu*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        window = nullptr;
        awaitWorld(expected);
    }
    void expectFocusPolicy(const unsigned policy)
    {
        std::set<const Window*> expected{window->GetCtrl<ctrlImageButton>(0), window->GetCtrl<ctrlImageButton>(1)};
        for(unsigned i = 0; i < MILITARY_SETTINGS_SCALE.size(); ++i)
        {
            const bool visible = (i != 2u || !(policy & 1u)) && (i != 6u || (policy & 2u));
            BOOST_TEST(slider(i).IsVisible() == visible);
            if(visible)
                expected.insert(&slider(i));
        }
        // Start at the last control, then physically traverse back and forwards to the limits.
        focusUntil(window->GetCtrl<ctrlImageButton>(1));
        std::set<const Window*> visited;
        for(const auto direction : {PadButton::LeftShoulder, PadButton::RightShoulder})
        {
            for(unsigned i = 0; i < 12; ++i)
            {
                visited.insert(focused());
                press(direction);
            }
        }
        BOOST_TEST(visited == expected);
    }
    MilitarySettings editAll()
    {
        auto expected = actual();
        for(unsigned i = 0; i < expected.size(); ++i)
        {
            if(!slider(i).IsVisible())
                continue;
            focusUntil(&slider(i));
            for(unsigned n = 0; n < MILITARY_SETTINGS_SCALE[i] + 2u; ++n)
                press(PadButton::DpadLeft);
            BOOST_TEST(slider(i).GetPosition() == 0u);
            for(unsigned n = 0; n < MILITARY_SETTINGS_SCALE[i] + 2u; ++n)
                press(PadButton::DpadRight);
            BOOST_TEST(slider(i).GetPosition() == MILITARY_SETTINGS_SCALE[i]);
            press(PadButton::DpadLeft);
            expected[i] = MILITARY_SETTINGS_SCALE[i] - 1u;
            expectControls(expected);
        }
        return expected;
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(PadMilitaryTests)

BOOST_DATA_TEST_CASE_F(MilitaryPadFixture, BoundsAddonFocusDefaultAndReopenReachTheWorld,
                       boost::unit_test::data::make({0u, 1u, 2u, 3u}), policy)
{
    run(policy, [this, policy] {
        enter();
        const auto defaults = actual();
        expectControls(defaults);
        expectFocusPolicy(policy);
        const auto edited = editAll();
        closeAndRoundtrip(edited);
        openFromMain();
        expectControls(edited);
        click(1);
        expectControls(defaults);
        closeAndRoundtrip(defaults);
        openFromMain();
        expectControls(defaults);
    });
    BOOST_TEST(SETTINGS.global.debugMode == savedDebugMode);
}

BOOST_DATA_TEST_CASE_F(MilitaryPadFixture, MouseTimerAndHelpPreserveTheIntendedSettings,
                       boost::unit_test::data::make({0u, 3u}), policy)
{
    run(policy, [this] {
        enter();
        auto expected = actual();
        mouseClick(*slider(0).GetCtrl<ctrlImageButton>(0));
        --expected[0];
        expectControls(expected);
        mouseClick(*slider(1).GetCtrl<ctrlImageButton>(1));
        ++expected[1];
        expectControls(expected);
        // Settings transmit periodically without closing; ctrlTimer measures actual wall time.
        std::this_thread::sleep_for(std::chrono::milliseconds(2100));
        frame();
        awaitWorld(expected);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        click(0);
        BOOST_TEST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        expectControls(expected);
        press(PadButton::Y);
        closeAndRoundtrip(expected);
    });
}

BOOST_DATA_TEST_CASE_F(MilitaryPadFixture, ReplayReflectsRecordedValuesAndRejectsEditsWithoutCloseWarning,
                       boost::unit_test::data::make({0u, 1u, 2u, 3u}), policy)
{
    run(policy, [this] {
        enter();
        const auto recorded = editAll();
        closeAndRoundtrip(recorded);
        pumpUntilGF(GAMECLIENT.GetGFNumber() + 40u);
        const auto replay = RTTRCONFIG.ExpandPath(s25::folders::replays) / GAMECLIENT.GetReplayFilename();
        cleanupDesktop();
        GAMECLIENT.Stop();
        GAMESERVER.Stop();
        BOOST_TEST_REQUIRE(boost::filesystem::is_regular_file(replay));
        GAMECLIENT.SetInterface(&ci());
        BOOST_TEST_REQUIRE(GAMECLIENT.StartReplay(replay));
        GAMECLIENT.GameLoaded();
        makeDesktop();
        GAMECLIENT.SetPause(false);
        enter();
        const auto initial = actual();
        expectControls(initial);
        for(unsigned i = 0; i < initial.size(); ++i)
        {
            if(!slider(i).IsVisible())
                continue;
            focusUntil(&slider(i));
            press(PadButton::DpadLeft);
            expectControls(initial);
            press(PadButton::DpadRight);
            expectControls(initial);
            for(const unsigned id : {0u, 1u})
            {
                mouseClick(*slider(i).GetCtrl<ctrlImageButton>(id));
                expectControls(initial);
            }
            const MouseCoords mc(slider(i).GetDrawPos() + DrawPoint(60, 12));
            WINDOWMANAGER.Msg_WheelUp(mc);
            frame();
            expectControls(initial);
            WINDOWMANAGER.Msg_WheelDown(mc);
            frame();
            expectControls(initial);
            mouseClick(slider(i), DrawPoint(60, 12));
            expectControls(initial);
        }
        click(1);
        expectControls(initial);
        mouseClick(*window->GetCtrl<ctrlImageButton>(1));
        expectControls(initial);
        expectWorld(initial);
        // A rejected edit must not leave pending settings and cause a discard warning on close.
        closeAndRoundtrip(initial);
        openFromMain();
        GAMECLIENT.skiptogf = GAMECLIENT.GetLastReplayGF();
        pumpUntil([this] { return ci().replayEnded || ci().numErrors || ci().numReplayAsync; }, "replay to finish");
        std::this_thread::sleep_for(std::chrono::milliseconds(2100));
        frame();
        BOOST_TEST(ci().replayEnded);
        expectWorld(recorded);
        expectControls(recorded);
        click(1);
        expectControls(recorded);
        closeAndRoundtrip(recorded);
    });
}

BOOST_AUTO_TEST_CASE(ExceptionalCleanupRestoresSettingsBeforeFixtureDestruction)
{
    const auto autosaveBefore = SETTINGS.interface.autosaveInterval;
    const auto debugBefore = SETTINGS.global.debugMode;
    {
        MilitaryPadFixture fixture;
        bool reachedProbe = false;
        BOOST_CHECK_THROW(fixture.run(0,
                                      [&fixture, &reachedProbe] {
                                          fixture.enter();
                                          reachedProbe = true;
                                          throwCleanupProbe();
                                      }),
                          std::runtime_error);
        BOOST_TEST(reachedProbe);
        BOOST_TEST(dynamic_cast<dskGameInterface*>(WINDOWMANAGER.GetCurrentDesktop()) == nullptr);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(SETTINGS.global.debugMode == debugBefore);
    }
    BOOST_TEST(SETTINGS.interface.autosaveInterval == autosaveBefore);
    BOOST_TEST(SETTINGS.global.debugMode == debugBefore);
}

BOOST_AUTO_TEST_SUITE_END()
