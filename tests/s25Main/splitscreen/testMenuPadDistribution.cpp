// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "LeatherLoader.h"
#include "LocalGameFixture.h"
#include "PadFixture.h"
#include "controls/ctrlGroup.h"
#include "controls/ctrlImageButton.h"
#include "controls/ctrlProgress.h"
#include "controls/ctrlTab.h"
#include "controls/ctrlTextButton.h"
#include "driver/MouseCoords.h"
#include "ingameWindows/iwDistribution.h"
#include "ingameWindows/iwHelp.h"
#include "ingameWindows/iwMainMenu.h"
#include "ingameWindows/iwPadSystemMenu.h"
#include "rttr/test/TmpFolder.hpp"
#include <libsiedler2/Archiv.h>
#include <libsiedler2/ArchivItem_Bitmap_Raw.h>
#include <libsiedler2/PixelBufferBGRA.h>
#include <libsiedler2/libsiedler2.h>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
constexpr PadDeviceId pad = 109;

[[noreturn]] void throwCleanupProbe()
{
    throw std::runtime_error("distribution cleanup probe");
}

struct DistributionPadFixture : uiHelper::Fixture, rttr::test::LocalGameFixture
{
    rttr::test::PadFeeder pads{*uiHelper::GetVideoDriver()};
    rttr::test::TestableGameInterface* dsk = nullptr;
    iwDistribution* window = nullptr;
    const bool savedDebugMode = SETTINGS.global.debugMode;
    const decltype(SETTINGS.windows.persistentSettings) savedPersistent = SETTINGS.windows.persistentSettings;
    rttr::test::TmpFolder graphics;
    unsigned policy = 0;

    template<class F>
    void run(const unsigned addons, F&& body)
    {
        try
        {
            beginGame(addons);
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
        SETTINGS.windows.persistentSettings = savedPersistent;
    }

    void expectRestoredSettings() const
    {
        BOOST_TEST(SETTINGS.global.debugMode == savedDebugMode);
        BOOST_TEST_REQUIRE(SETTINGS.windows.persistentSettings.size() == savedPersistent.size());
        for(const auto& entry : savedPersistent)
        {
            const auto it = SETTINGS.windows.persistentSettings.find(entry.first);
            BOOST_TEST_REQUIRE((it != SETTINGS.windows.persistentSettings.end()));
            const auto& restored = it->second;
            BOOST_TEST(restored.isOpen == entry.second.isOpen);
            BOOST_TEST(restored.isPinned == entry.second.isPinned);
            BOOST_TEST(restored.isMinimized == entry.second.isMinimized);
            BOOST_TEST((restored.lastPos == entry.second.lastPos));
            BOOST_TEST((restored.restorePos == entry.second.restorePos));
        }
    }

    void cleanupDesktop()
    {
        // Settings pointers and viewer/factory references must outlive the windows.
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        WINDOWMANAGER.Draw();
        if(ci().game)
            world().SetGameInterface(nullptr);
        dsk = nullptr;
        window = nullptr;
    }

    void beginGame(const unsigned addons)
    {
        policy = addons;
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
        for(const auto& addon : {std::make_pair(AddonId::WINE, 1u), std::make_pair(AddonId::LEATHER, 2u),
                                 std::make_pair(AddonId::CHARBURNER, 4u)})
            settings.setSelection(addon.first, (policy & addon.second) ? 1 : 0);
        lobby().ChangeGlobalGameSettings(settings);
        // The lobby writes eagerly. Poison just its local copy and require the real broadcast.
        GAMECLIENT.GetGameLobby()->getSettings() = before;
        GAMECLIENT.GetGameLobby()->getSettings().setSelection(AddonId::WINE, (policy & 1u) ? 0 : 1);
        pumpUntil([this] { return matchesPolicy(GAMECLIENT.GetGameLobby()->getSettings()); }, "addon broadcast");
        startGame();
        BOOST_TEST(matchesPolicy(world().GetGGS()));
        makeDesktop();
    }

    bool matchesPolicy(const GlobalGameSettings& settings) const
    {
        return settings.isEnabled(AddonId::WINE) == !!(policy & 1u)
               && settings.isEnabled(AddonId::LEATHER) == !!(policy & 2u)
               && settings.isEnabled(AddonId::CHARBURNER) == !!(policy & 4u);
    }

    void makeDesktop()
    {
        // Desktop switches preserve isOpen flags; isolate every physical entry, including replay.
        for(auto& entry : SETTINGS.windows.persistentSettings)
            entry.second.isOpen = false;
        LOADER.LoadDummyMapFiles();
        const auto path = graphics / "leather_bobs.lst";
        boost::filesystem::create_directories(path);
        const auto icon = leatheraddon::bobIndex[leatheraddon::BobType::DistributionOfPigsIcon];
        libsiedler2::Archiv sprite;
        auto bitmap = std::make_unique<libsiedler2::ArchivItem_Bitmap_Raw>();
        bitmap->create(libsiedler2::PixelBufferBGRA(1, 1));
        sprite.push(std::move(bitmap));
        BOOST_TEST_REQUIRE(libsiedler2::Write(path / (std::to_string(icon) + ".bmp"), sprite) == 0);
        BOOST_TEST_REQUIRE(LOADER.LoadFiles({path.string()}));
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
    Window* focused() const { return dsk->GetPlayerView(0).GetFocus().GetFocused(); }
    void focusUntil(const Window* target)
    {
        if(dsk->GetPlayerView(0).GetRing().IsOpen())
        {
            for(unsigned sector = 0; sector < 8 && focused() != target; ++sector)
                press(PadButton::DpadRight);
        } else
        {
            for(const auto direction : {PadButton::RightShoulder, PadButton::LeftShoulder})
            {
                for(unsigned i = 0; i < 48 && focused() != target; ++i)
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
        focusUntil(main->GetCtrl<ctrlImageButton>(0));
        press(PadButton::A);
        window = dynamic_cast<iwDistribution*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(window != nullptr);
        press(PadButton::Y);
        BOOST_TEST_REQUIRE(dsk->GetPlayerView(0).GetFocus().GetRoot() == window);
    }

    // Explicit specification of the displayed mapping, separate from the window's filtering.
    using Groups = std::vector<std::vector<unsigned>>;
    Groups expectedGroups() const
    {
        Groups result{{0, 1, 2, 3}, {5, 6, 7, 8}, {10, 11}, {12, 13, 14}};
        if(policy & 1u)
            result[0].push_back(4);
        if(policy & 4u)
            result[1].push_back(9);
        std::vector<unsigned> wood{15};
        if(policy & 4u)
            wood.push_back(16);
        if(policy & 1u)
            wood.push_back(17);
        if(wood.size() > 1u)
            result.push_back(wood);
        result.push_back({18, 19, 20});
        if(policy & 2u)
            result.back().push_back(21);
        result.push_back({22, 23, 24, 25});
        if(policy & 1u)
            result.back().push_back(26);
        if(policy & 2u)
            result.push_back({27, 28});
        return result;
    }

    ctrlTab& tabs() const { return *window->GetCtrl<ctrlTab>(0); }
    ctrlProgress& slider(const unsigned group, const unsigned entry) const
    {
        return *tabs().GetGroup(group)->GetCtrl<ctrlProgress>(entry);
    }
    void selectTab(const unsigned group, const bool mouse = false)
    {
        auto* header = tabs().GetCtrl<ctrlImageButton>(group);
        if(mouse)
            mouseClick(*header);
        else
        {
            focusUntil(header);
            press(PadButton::A);
        }
        BOOST_TEST(tabs().GetCurrentTab() == group);
        for(unsigned i = 0; i < tabs().GetNumTabs(); ++i)
            BOOST_TEST(tabs().GetGroup(i)->IsVisible() == (i == group));
    }
    void expectControls(const Distributions& expected) const
    {
        const auto groups = expectedGroups();
        BOOST_TEST_REQUIRE(tabs().GetNumTabs() == groups.size());
        for(unsigned g = 0; g < groups.size(); ++g)
        {
            BOOST_TEST_REQUIRE(tabs().GetGroup(g) != nullptr);
            for(unsigned e = 0; e < groups[g].size(); ++e)
            {
                BOOST_TEST_REQUIRE(tabs().GetGroup(g)->GetCtrl<ctrlProgress>(e) != nullptr);
                BOOST_TEST(slider(g, e).GetPosition() == expected[groups[g][e]]);
            }
            BOOST_TEST(tabs().GetGroup(g)->GetCtrl<ctrlProgress>(groups[g].size()) == nullptr);
        }
    }
    void expectFocus(const unsigned group)
    {
        std::set<const Window*> expected{window->GetCtrl<ctrlImageButton>(2), window->GetCtrl<ctrlImageButton>(10)};
        for(unsigned g = 0; g < tabs().GetNumTabs(); ++g)
            expected.insert(tabs().GetCtrl<ctrlImageButton>(g));
        const auto groups = expectedGroups();
        for(unsigned e = 0; e < groups[group].size(); ++e)
            expected.insert(&slider(group, e));
        focusUntil(window->GetCtrl<ctrlImageButton>(10));
        std::set<const Window*> visited;
        for(const auto direction : {PadButton::LeftShoulder, PadButton::RightShoulder})
        {
            for(unsigned i = 0; i < 30; ++i)
            {
                visited.insert(focused());
                press(direction);
            }
        }
        BOOST_TEST(visited == expected);
    }
    Distributions actual()
    {
        VisualSettings result{};
        world().GetPlayer(0).FillVisualSettings(result);
        return result.distribution;
    }
    void expectWorld(const Distributions& expected)
    {
        BOOST_TEST(actual() == expected);
        BOOST_TEST(ci().numErrors == 0u);
        BOOST_TEST(ci().numAsync == 0u);
        BOOST_TEST(ci().numReplayAsync == 0u);
    }
    void awaitWorld(const Distributions& expected)
    {
        if(!GAMECLIENT.IsReplayModeOn())
            pumpUntilGF(GAMECLIENT.GetGFNumber() + 2u * GAMECLIENT.GetNWFLength());
        pumpUntil([this, &expected] { return actual() == expected; }, "distribution to reach actual world",
                  std::chrono::seconds(10));
        expectWorld(expected);
    }
    void closeAndRoundtrip(const Distributions& expected)
    {
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(dynamic_cast<iwMainMenu*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        window = nullptr;
        awaitWorld(expected);
    }
    Distributions editAll()
    {
        auto expected = actual();
        const auto groups = expectedGroups();
        for(unsigned g = 0; g < groups.size(); ++g)
        {
            selectTab(g);
            expectFocus(g);
            for(unsigned e = 0; e < groups[g].size(); ++e)
            {
                focusUntil(&slider(g, e));
                for(unsigned n = 0; n < 12; ++n)
                    press(PadButton::DpadLeft);
                BOOST_TEST(slider(g, e).GetPosition() == 0u);
                for(unsigned n = 0; n < 12; ++n)
                    press(PadButton::DpadRight);
                BOOST_TEST(slider(g, e).GetPosition() == 10u);
                const auto value = static_cast<uint8_t>(1u + groups[g][e] % 8u);
                for(unsigned n = value; n < 10u; ++n)
                    press(PadButton::DpadLeft);
                expected[groups[g][e]] = value;
                expectControls(expected);
            }
        }
        return expected;
    }
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
    void helpAndReturn()
    {
        click(2);
        BOOST_TEST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        press(PadButton::Y);
    }
    void timerRoundtrip(const Distributions& expected)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2100));
        frame();
        awaitWorld(expected);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(PadDistributionTests)

BOOST_DATA_TEST_CASE_F(DistributionPadFixture, EveryAddonTabSliderBoundsDefaultAndReopenReachTheWorld,
                       boost::unit_test::data::make({0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u}), policy)
{
    run(policy, [this] {
        enter();
        const auto defaults = actual();
        expectControls(defaults);
        const auto edited = editAll();
        closeAndRoundtrip(edited);
        openFromMain();
        expectControls(edited);
        click(10);
        expectControls(defaults);
        closeAndRoundtrip(defaults);
        openFromMain();
        expectControls(defaults);
    });
    BOOST_TEST(SETTINGS.global.debugMode == savedDebugMode);
    expectRestoredSettings();
}

BOOST_DATA_TEST_CASE_F(DistributionPadFixture, MouseTabsTimerAndHelpPreserveTheIntendedSettings,
                       boost::unit_test::data::make({0u, 7u}), policy)
{
    run(policy, [this] {
        enter();
        auto expected = actual();
        const auto groups = expectedGroups();
        for(unsigned g = 0; g < groups.size(); ++g)
        {
            selectTab(g, true);
            mouseClick(*slider(g, 0).GetCtrl<ctrlImageButton>(0));
            --expected[groups[g][0]];
            mouseClick(*slider(g, 1).GetCtrl<ctrlImageButton>(1));
            ++expected[groups[g][1]];
            expectControls(expected);
        }
        timerRoundtrip(expected);
        helpAndReturn();
        expectControls(expected);
        closeAndRoundtrip(expected);
    });
}

BOOST_DATA_TEST_CASE_F(DistributionPadFixture, ReplayBrowsesEveryTabButRejectsPhysicalEditsWithoutCloseWarning,
                       boost::unit_test::data::make({0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u}), policy)
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
        const auto groups = expectedGroups();
        for(unsigned g = 0; g < groups.size(); ++g)
        {
            selectTab(g);
            for(unsigned e = 0; e < groups[g].size(); ++e)
            {
                focusUntil(&slider(g, e));
                press(PadButton::DpadLeft);
                expectControls(initial);
                press(PadButton::DpadRight);
                expectControls(initial);
                for(const unsigned id : {0u, 1u})
                {
                    mouseClick(*slider(g, e).GetCtrl<ctrlImageButton>(id));
                    expectControls(initial);
                }
                const MouseCoords mc(slider(g, e).GetDrawPos() + DrawPoint(60, 12));
                WINDOWMANAGER.Msg_WheelUp(mc);
                frame();
                expectControls(initial);
                WINDOWMANAGER.Msg_WheelDown(mc);
                frame();
                expectControls(initial);
                mouseClick(slider(g, e), DrawPoint(60, 12));
                expectControls(initial);
            }
        }
        click(10);
        mouseClick(*window->GetCtrl<ctrlImageButton>(10));
        expectControls(initial);
        expectWorld(initial);
        closeAndRoundtrip(initial);
        openFromMain();
        helpAndReturn();
        GAMECLIENT.skiptogf = GAMECLIENT.GetLastReplayGF();
        pumpUntil([this] { return ci().replayEnded || ci().numErrors || ci().numReplayAsync; }, "replay to finish");
        std::this_thread::sleep_for(std::chrono::milliseconds(2100));
        frame();
        BOOST_TEST(ci().replayEnded);
        expectWorld(recorded);
        expectControls(recorded);
        click(10);
        expectControls(recorded);
        closeAndRoundtrip(recorded);
    });
    expectRestoredSettings();
}

BOOST_AUTO_TEST_CASE(ExceptionalCleanupRestoresSettingsBeforeFixtureDestruction)
{
    const auto debugBefore = SETTINGS.global.debugMode;
    const auto autosaveBefore = SETTINGS.interface.autosaveInterval;
    {
        DistributionPadFixture fixture;
        bool reachedProbe = false;
        BOOST_CHECK_THROW(fixture.run(7,
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
        fixture.expectRestoredSettings();
    }
    BOOST_TEST(SETTINGS.interface.autosaveInterval == autosaveBefore);
}

BOOST_AUTO_TEST_SUITE_END()
