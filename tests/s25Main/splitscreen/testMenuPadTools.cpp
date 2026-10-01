// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "LocalGameFixture.h"
#include "PadFixture.h"
#include "controls/ctrlImageButton.h"
#include "controls/ctrlProgress.h"
#include "controls/ctrlTextButton.h"
#include "controls/ctrlTextDeepening.h"
#include "driver/MouseCoords.h"
#include "ingameWindows/iwHelp.h"
#include "ingameWindows/iwMainMenu.h"
#include "ingameWindows/iwPadSystemMenu.h"
#include "ingameWindows/iwTools.h"
#include "gameData/ToolConsts.h"
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <memory>
#include <set>
#include <stdexcept>
#include <thread>

namespace {
constexpr PadDeviceId pad = 106;

[[noreturn]] void throwCleanupProbe()
{
    throw std::runtime_error("tools cleanup probe");
}

struct ToolsPadFixture : uiHelper::Fixture, rttr::test::LocalGameFixture
{
    rttr::test::PadFeeder pads{*uiHelper::GetVideoDriver()};
    rttr::test::TestableGameInterface* dsk = nullptr;
    iwTools* window = nullptr;
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
        settings.setSelection(AddonId::TOOL_ORDERING, policy);
        lobby().ChangeGlobalGameSettings(settings);
        GAMECLIENT.GetGameLobby()->getSettings() = before;
        GAMECLIENT.GetGameLobby()->getSettings().setSelection(AddonId::TOOL_ORDERING, 1u - policy);
        pumpUntil(
          [policy] { return GAMECLIENT.GetGameLobby()->getSettings().getSelection(AddonId::TOOL_ORDERING) == policy; },
          "tool ordering addon broadcast");
        startGame();
        BOOST_TEST(world().GetGGS().getSelection(AddonId::TOOL_ORDERING) == policy);
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
        focusUntil(main->GetCtrl<ctrlImageButton>(2));
        press(PadButton::A);
        window = dynamic_cast<iwTools*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(window != nullptr);
        press(PadButton::Y);
        BOOST_TEST_REQUIRE(dsk->GetPlayerView(0).GetFocus().GetRoot() == window);
    }

    ctrlProgress& slider(const Tool tool) const { return *window->GetCtrl<ctrlProgress>(rttr::enum_cast(tool)); }
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

    using Orders = helpers::EnumArray<unsigned, Tool>;

    ToolSettings actual()
    {
        ToolSettings result{};
        for(const auto tool : helpers::enumRange<Tool>())
            result[tool] = world().GetPlayer(0).GetToolPriority(tool);
        return result;
    }
    Orders actualOrders()
    {
        Orders result{};
        for(const auto tool : helpers::enumRange<Tool>())
            result[tool] = world().GetPlayer(0).GetToolsOrdered(tool);
        return result;
    }
    void expectControls(const ToolSettings& expected) const
    {
        for(const auto tool : helpers::enumRange<Tool>())
            BOOST_TEST(slider(tool).GetPosition() == expected[tool]);
    }
    void expectOrderTexts(const Orders& expected) const
    {
        for(const auto tool : helpers::enumRange<Tool>())
            BOOST_TEST(window->GetCtrl<ctrlTextDeepening>(200u + rttr::enum_cast(tool))->GetText()
                       == std::to_string(expected[tool]));
    }
    void expectWorld(const ToolSettings& expected, const Orders& orders)
    {
        for(const auto tool : helpers::enumRange<Tool>())
        {
            BOOST_TEST(world().GetPlayer(0).GetToolPriority(tool) == expected[tool]);
            BOOST_TEST(world().GetPlayer(0).GetToolsOrdered(tool) == orders[tool]);
            // Replays apply recorded deltas without the live client's speculative order queue.
            if(!GAMECLIENT.IsReplayModeOn())
                BOOST_TEST(world().GetPlayer(0).GetToolsOrderedVisual(tool) == orders[tool]);
        }
        BOOST_TEST(ci().numErrors == 0u);
        BOOST_TEST(ci().numAsync == 0u);
        BOOST_TEST(ci().numReplayAsync == 0u);
    }
    void awaitWorld(const ToolSettings& expected, const Orders& orders = {})
    {
        if(!GAMECLIENT.IsReplayModeOn())
            pumpUntilGF(GAMECLIENT.GetGFNumber() + 2u * GAMECLIENT.GetNWFLength());
        pumpUntil([this, &expected, &orders] { return actual() == expected && actualOrders() == orders; },
                  "tool settings and orders to reach the actual world", std::chrono::seconds(10));
        expectWorld(expected, orders);
    }
    void closeAndRoundtrip(const ToolSettings& expected, const Orders& orders = {})
    {
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(dynamic_cast<iwMainMenu*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        window = nullptr;
        awaitWorld(expected, orders);
    }
    void expectFocusPolicy(const unsigned policy)
    {
        std::set<const Window*> expected{window->GetCtrl<ctrlImageButton>(12), window->GetCtrl<ctrlImageButton>(13)};
        for(const auto tool : helpers::enumRange<Tool>())
        {
            expected.insert(&slider(tool));
            for(const auto id : {100u + 2u * rttr::enum_cast(tool), 101u + 2u * rttr::enum_cast(tool)})
            {
                const auto* control = window->GetCtrl<ctrlImageButton>(id);
                BOOST_TEST((control != nullptr) == (policy != 0u));
                if(policy)
                    expected.insert(control);
            }
        }
        const auto* zero = window->GetCtrl<ctrlImageButton>(15);
        BOOST_TEST((zero != nullptr) == (policy != 0u));
        if(policy)
            expected.insert(zero);
        focusUntil(window->GetCtrl<ctrlImageButton>(13));
        std::set<const Window*> visited;
        for(const auto direction : {PadButton::LeftShoulder, PadButton::RightShoulder})
        {
            for(unsigned i = 0; i < 42; ++i)
            {
                visited.insert(focused());
                press(direction);
            }
        }
        BOOST_TEST(visited == expected);
    }
    ToolSettings editAll()
    {
        auto expected = actual();
        for(const auto tool : helpers::enumRange<Tool>())
        {
            focusUntil(&slider(tool));
            for(unsigned n = 0; n < 12; ++n)
                press(PadButton::DpadLeft);
            BOOST_TEST(slider(tool).GetPosition() == 0u);
            for(unsigned n = 0; n < 12; ++n)
                press(PadButton::DpadRight);
            BOOST_TEST(slider(tool).GetPosition() == 10u);
            press(PadButton::DpadLeft);
            expected[tool] = 9;
            expectControls(expected);
        }
        return expected;
    }
    void helpAndReturn()
    {
        click(12);
        BOOST_TEST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        press(PadButton::Y);
    }
    void timerRoundtrip(const ToolSettings& expected, const Orders& orders = {})
    {
        // ctrlTimer measures wall time; changing the mock driver's tick count cannot trigger it.
        std::this_thread::sleep_for(std::chrono::milliseconds(2100));
        frame();
        awaitWorld(expected, orders);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(PadToolsTests)

BOOST_DATA_TEST_CASE_F(ToolsPadFixture, SliderBoundsAddonFocusDefaultAndReopenReachTheWorld,
                       boost::unit_test::data::make({0u, 1u}), policy)
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
        click(13);
        expectControls(defaults);
        closeAndRoundtrip(defaults);
        openFromMain();
        expectControls(defaults);
        if(policy)
        {
            click(15);
            expectControls({});
            closeAndRoundtrip({});
            openFromMain();
            expectControls({});
        }
    });
    BOOST_TEST(SETTINGS.global.debugMode == savedDebugMode);
}

BOOST_DATA_TEST_CASE_F(ToolsPadFixture, MouseTimerAndHelpPreserveTheIntendedSettings,
                       boost::unit_test::data::make({0u, 1u}), policy)
{
    run(policy, [this, policy] {
        enter();
        auto expected = actual();
        mouseClick(*slider(Tool::Tongs).GetCtrl<ctrlImageButton>(0));
        --expected[Tool::Tongs];
        mouseClick(*slider(Tool::Hammer).GetCtrl<ctrlImageButton>(1));
        ++expected[Tool::Hammer];
        expectControls(expected);
        Orders orders{};
        if(policy)
        {
            mouseClick(*window->GetCtrl<ctrlImageButton>(100));
            orders[Tool::Tongs] = 1;
            expectOrderTexts(orders);
        }
        timerRoundtrip(expected, orders);
        // A second transmission/close must not send an order delta twice.
        timerRoundtrip(expected, orders);
        helpAndReturn();
        expectControls(expected);
        closeAndRoundtrip(expected, orders);
    });
}

BOOST_FIXTURE_TEST_CASE(OrderBoundsAndMixedDeltasReachTheWorld, ToolsPadFixture)
{
    run(1, [this] {
        enter();
        const auto defaults = actual();
        Orders orders{};
        for(const auto tool : helpers::enumRange<Tool>())
        {
            const auto plus = 100u + 2u * rttr::enum_cast(tool);
            click(plus + 1u);
            expectOrderTexts(orders);
            focusUntil(window->GetCtrl<ctrlImageButton>(plus));
            for(unsigned n = 0; n < 101; ++n)
                press(PadButton::A);
            orders[tool] = 99;
            expectOrderTexts(orders);
        }
        closeAndRoundtrip(defaults, orders);
        openFromMain();
        expectOrderTexts(orders);
        for(const auto tool : helpers::enumRange<Tool>())
        {
            const auto plus = 100u + 2u * rttr::enum_cast(tool);
            focusUntil(window->GetCtrl<ctrlImageButton>(plus + 1u));
            for(unsigned n = 0; n < 101; ++n)
                press(PadButton::A);
            orders[tool] = 0;
            expectOrderTexts(orders);
            focusUntil(window->GetCtrl<ctrlImageButton>(plus));
            orders[tool] = rttr::enum_cast(tool) + 1u;
            for(unsigned n = 0; n < orders[tool]; ++n)
                press(PadButton::A);
            expectOrderTexts(orders);
        }
        // Pending positive and negative deltas may accompany a priority reset, but survive it.
        click(15);
        expectOrderTexts(orders);
        timerRoundtrip({}, orders);
        expectOrderTexts(orders);
        click(13);
        closeAndRoundtrip(defaults, orders);
        openFromMain();
        expectOrderTexts(orders);
    });
}

BOOST_DATA_TEST_CASE_F(ToolsPadFixture, ReplayReflectsRecordedValuesAndRejectsEditsWithoutCloseWarning,
                       boost::unit_test::data::make({0u, 1u}), policy)
{
    run(policy, [this, policy] {
        enter();
        const auto recorded = editAll();
        Orders recordedOrders{};
        if(policy)
        {
            for(const auto tool : helpers::enumRange<Tool>())
            {
                focusUntil(window->GetCtrl<ctrlImageButton>(100u + 2u * rttr::enum_cast(tool)));
                recordedOrders[tool] = rttr::enum_cast(tool) + 1u;
                for(unsigned n = 0; n < recordedOrders[tool]; ++n)
                    press(PadButton::A);
            }
            expectOrderTexts(recordedOrders);
        }
        closeAndRoundtrip(recorded, recordedOrders);
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
        const auto initialOrders = actualOrders();
        expectControls(initial);
        for(const auto tool : helpers::enumRange<Tool>())
        {
            focusUntil(&slider(tool));
            press(PadButton::DpadLeft);
            expectControls(initial);
            press(PadButton::DpadRight);
            expectControls(initial);
            for(const unsigned id : {0u, 1u})
            {
                mouseClick(*slider(tool).GetCtrl<ctrlImageButton>(id));
                expectControls(initial);
            }
            const MouseCoords mc(slider(tool).GetDrawPos() + DrawPoint(60, 12));
            WINDOWMANAGER.Msg_WheelUp(mc);
            frame();
            expectControls(initial);
            WINDOWMANAGER.Msg_WheelDown(mc);
            frame();
            expectControls(initial);
            mouseClick(slider(tool), DrawPoint(60, 12));
            expectControls(initial);
            if(policy)
            {
                for(const auto id : {100u + 2u * rttr::enum_cast(tool), 101u + 2u * rttr::enum_cast(tool)})
                {
                    click(id);
                    mouseClick(*window->GetCtrl<ctrlImageButton>(id));
                    expectOrderTexts(initialOrders);
                }
            }
        }
        for(const auto id : {13u, 15u})
        {
            if(window->GetCtrl<ctrlImageButton>(id))
            {
                click(id);
                mouseClick(*window->GetCtrl<ctrlImageButton>(id));
                expectControls(initial);
            }
        }
        expectWorld(initial, initialOrders);
        // Close before the periodic refresh: rejected edits must never leave a pending warning.
        closeAndRoundtrip(initial, initialOrders);
        openFromMain();
        helpAndReturn();
        GAMECLIENT.skiptogf = GAMECLIENT.GetLastReplayGF();
        pumpUntil([this] { return ci().replayEnded || ci().numErrors || ci().numReplayAsync; }, "replay to finish");
        std::this_thread::sleep_for(std::chrono::milliseconds(2100));
        frame();
        BOOST_TEST(ci().replayEnded);
        expectWorld(recorded, recordedOrders);
        expectControls(recorded);
        if(policy)
            expectOrderTexts(recordedOrders);
        click(13);
        expectControls(recorded);
        closeAndRoundtrip(recorded, recordedOrders);
    });
}

BOOST_AUTO_TEST_CASE(ExceptionalCleanupRestoresSettingsBeforeFixtureDestruction)
{
    const auto autosaveBefore = SETTINGS.interface.autosaveInterval;
    const auto debugBefore = SETTINGS.global.debugMode;
    {
        ToolsPadFixture fixture;
        bool reachedProbe = false;
        BOOST_CHECK_THROW(fixture.run(1,
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
