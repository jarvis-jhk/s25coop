// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AsyncChecksum.h"
#include "LocalGameFixture.h"
#include "PadFixture.h"
#include "controls/ctrlImageButton.h"
#include "controls/ctrlOptionGroup.h"
#include "controls/ctrlText.h"
#include "controls/ctrlTextButton.h"
#include "driver/MouseCoords.h"
#include "ingameWindows/iwHelp.h"
#include "ingameWindows/iwMainMenu.h"
#include "ingameWindows/iwPadSystemMenu.h"
#include "ingameWindows/iwStatistics.h"
#include "gameTypes/StatisticTypes.h"
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
constexpr PadDeviceId pad = 107;
constexpr unsigned typeGroup = MAX_PLAYERS + 1u;
constexpr unsigned timeGroup = typeGroup + 9u;
constexpr unsigned headlineId = timeGroup + 5u;
constexpr unsigned timeLabelStart = headlineId + 1u;
constexpr unsigned maxY = timeLabelStart + iwStatistics::MAX_TIME_LABELS;
constexpr unsigned minY = maxY + 1u;
constexpr unsigned helpId = minY + 1u;

[[noreturn]] void throwCleanupProbe()
{
    throw std::runtime_error("statistics cleanup probe");
}

struct StatisticsPadFixture : uiHelper::Fixture, rttr::test::LocalGameFixture
{
    rttr::test::PadFeeder pads{*uiHelper::GetVideoDriver()};
    rttr::test::TestableGameInterface* dsk = nullptr;
    iwStatistics* window = nullptr;
    const bool savedDebugMode = SETTINGS.global.debugMode;
    const bool savedScale = SETTINGS.ingame.scaleStatistics;
    const decltype(SETTINGS.windows.persistentSettings) savedWindows = SETTINGS.windows.persistentSettings;

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
        SETTINGS.ingame.scaleStatistics = savedScale;
        SETTINGS.windows.persistentSettings = savedWindows;
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
        SETTINGS.ingame.scaleStatistics = policy >= 3u;
        preventWindowRestore();
        hostAndEnterLobby();
        lobby().SetPlayerState(1, PlayerState::AI, AI::Info(AI::Type::Dummy));
        lobby().SetPlayerState(2, PlayerState::AI, AI::Info(AI::Type::Dummy));
        pumpUntil(
          [] {
              const auto lobby = GAMECLIENT.GetGameLobby();
              return lobby->getPlayer(1).ps == PlayerState::AI && lobby->getPlayer(2).ps == PlayerState::AI;
          },
          "dummy AI configuration");
        lobby().SetTeam(0, Team::Team1);
        lobby().SetTeam(1, Team::Team1);
        lobby().SetTeam(2, Team::Team2);
        pumpUntil(
          [] {
              const auto lobby = GAMECLIENT.GetGameLobby();
              return lobby->getPlayer(0).team == Team::Team1 && lobby->getPlayer(1).team == Team::Team1
                     && lobby->getPlayer(2).team == Team::Team2;
          },
          "team broadcast");
        const auto before = GAMECLIENT.GetGameLobby()->getSettings();
        auto settings = before;
        settings.setSelection(AddonId::STATISTICS_VISIBILITY, policy % 3u);
        lobby().ChangeGlobalGameSettings(settings);
        GAMECLIENT.GetGameLobby()->getSettings() = before;
        GAMECLIENT.GetGameLobby()->getSettings().setSelection(AddonId::STATISTICS_VISIBILITY, (policy + 1u) % 3u);
        pumpUntil(
          [policy] {
              return GAMECLIENT.GetGameLobby()->getSettings().getSelection(AddonId::STATISTICS_VISIBILITY)
                     == policy % 3u;
          },
          "statistics visibility broadcast");
        startGame();
        BOOST_TEST(world().GetGGS().getSelection(AddonId::STATISTICS_VISIBILITY) == policy % 3u);
        BOOST_TEST(world().GetPlayer(0).IsAlly(1));
        BOOST_TEST(!world().GetPlayer(0).IsAlly(2));
        makeDesktop();
    }

    void preventWindowRestore()
    {
        // Desktop destruction preserves open-window intent; each input path starts with no windows.
        for(auto& entry : SETTINGS.windows.persistentSettings)
            entry.second.isOpen = false;
    }

    void expectRestored() const
    {
        BOOST_TEST(SETTINGS.global.debugMode == savedDebugMode);
        BOOST_TEST(SETTINGS.ingame.scaleStatistics == savedScale);
        BOOST_TEST(SETTINGS.windows.persistentSettings.size() == savedWindows.size());
        for(const auto& entry : savedWindows)
        {
            const auto it = SETTINGS.windows.persistentSettings.find(entry.first);
            BOOST_REQUIRE(it != SETTINGS.windows.persistentSettings.end());
            const auto& actual = it->second;
            BOOST_TEST(actual.isOpen == entry.second.isOpen);
            BOOST_TEST(actual.isMinimized == entry.second.isMinimized);
            BOOST_TEST(actual.isPinned == entry.second.isPinned);
            BOOST_TEST((actual.lastPos == entry.second.lastPos));
            BOOST_TEST((actual.restorePos == entry.second.restorePos));
        }
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
        focusUntil(main->GetCtrl<ctrlImageButton>(3));
        press(PadButton::A);
        window = dynamic_cast<iwStatistics*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(window != nullptr);
        press(PadButton::Y);
        BOOST_TEST_REQUIRE(dsk->GetPlayerView(0).GetFocus().GetRoot() == window);
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

    std::array<bool, 3> visible{};
    StatisticType category = StatisticType::Country;
    StatisticTime duration = StatisticTime::T15Minutes;

    void click(Window* control)
    {
        focusUntil(control);
        press(PadButton::A);
    }
    ctrlOptionGroup& group(const unsigned id) const { return *window->GetCtrl<ctrlOptionGroup>(id); }
    ctrlText& label(const unsigned id) const { return *window->GetCtrl<ctrlText>(id); }

    void initializeVisible(const unsigned policy)
    {
        for(unsigned p = 0; p < visible.size(); ++p)
            visible[p] = GAMECLIENT.IsReplayModeOn() || policy % 3u == 0u || p == 0u || (policy % 3u == 1u && p == 1u);
        category = StatisticType::Country;
        duration = StatisticTime::T15Minutes;
    }
    void seedHistory()
    {
        // Populate actual statistic history through its public update path, without fake UI state.
        for(unsigned n = 0; n < 64; ++n)
        {
            for(unsigned p = 0; p < visible.size(); ++p)
            {
                auto& player = world().GetPlayer(p);
                player.SetStatisticValue(StatisticType::Country, (p + 1u) * 100u + n);
                player.SetStatisticValue(StatisticType::Buildings, (p + 1u) * 20u + n);
                player.SetStatisticValue(StatisticType::Gold, (p + 1u) * 50u + n);
                player.SetStatisticValue(StatisticType::Vanquished, p * 30u + n);
                player.StatisticStep();
            }
        }
        BOOST_TEST(world().GetPlayer(2).GetStatistic(StatisticTime::T16Hours).data[StatisticType::Country][1] > 0u);
    }
    using History = std::array<helpers::EnumArray<GamePlayer::Statistic, StatisticTime>, 3>;
    History snapshotHistory()
    {
        History result{};
        for(unsigned p = 0; p < result.size(); ++p)
            for(const auto time : helpers::enumRange<StatisticTime>())
                result[p][time] = world().GetPlayer(p).GetStatistic(time);
        return result;
    }
    void expectHistoryUnchanged(const History& expected)
    {
        for(unsigned p = 0; p < expected.size(); ++p)
        {
            for(const auto time : helpers::enumRange<StatisticTime>())
            {
                const auto& actual = world().GetPlayer(p).GetStatistic(time);
                const auto& before = expected[p][time];
                BOOST_TEST(actual.currentIndex == before.currentIndex);
                BOOST_TEST(actual.counter == before.counter);
                for(const auto type : helpers::enumRange<StatisticType>())
                    for(unsigned i = 0; i < NUM_STAT_STEPS; ++i)
                        BOOST_TEST(actual.data[type][i] == before.data[type][i]);
                for(unsigned ware = 0; ware < NUM_STAT_MERCHANDISE_TYPES; ++ware)
                    for(unsigned i = 0; i < NUM_STAT_STEPS; ++i)
                        BOOST_TEST(actual.merchandiseData[ware][i] == before.merchandiseData[ware][i]);
            }
        }
    }

    void expectAxis()
    {
        unsigned maximum = 1;
        unsigned minimum = std::numeric_limits<unsigned>::max();
        for(unsigned p = 0; p < visible.size(); ++p)
        {
            if(!visible[p])
                continue;
            const auto& data = world().GetPlayer(p).GetStatistic(duration).data[category];
            maximum = std::max(maximum, *std::max_element(data.begin(), data.end()));
            minimum = std::min(minimum, *std::min_element(data.begin(), data.end()));
        }
        if(std::none_of(visible.begin(), visible.end(), [](const bool on) { return on; }))
            minimum = 0;
        if(SETTINGS.ingame.scaleStatistics && maximum == minimum)
        {
            --minimum;
            ++maximum;
        }
        BOOST_TEST(label(maxY).GetText() == std::to_string(maximum));
        BOOST_TEST(label(minY).IsVisible() == SETTINGS.ingame.scaleStatistics);
        if(SETTINGS.ingame.scaleStatistics)
            BOOST_TEST(label(minY).GetText() == std::to_string(minimum));
    }
    void expectTimeLabels() const
    {
        const std::array<std::vector<std::string>, 4> values{{{"0", "-15", "-12", "-9", "-6", "-3"},
                                                              {"0", "-60", "-50", "-40", "-30", "-20", "-10"},
                                                              {"0", "-240", "-180", "-120", "-60"},
                                                              {"0", "-960", "-720", "-480", "-240"}}};
        const auto& expected = values[rttr::enum_cast(duration)];
        for(unsigned i = 0; i < iwStatistics::MAX_TIME_LABELS; ++i)
        {
            const auto& text = label(timeLabelStart + i);
            BOOST_TEST(text.IsVisible() == (i < expected.size()));
            if(i < expected.size())
                BOOST_TEST(text.GetText() == expected[i]);
        }
    }
    void expectFocusPolicy(const unsigned policy)
    {
        std::set<const Window*> expected{window->GetCtrl<ctrlImageButton>(helpId)};
        for(unsigned p = 0; p < visible.size(); ++p)
        {
            auto* button = window->GetCtrl<ctrlImageButton>(p);
            BOOST_TEST_REQUIRE(button != nullptr);
            BOOST_TEST(button->GetEnabled() == visible[p]);
            if(visible[p])
                expected.insert(button);
            else
            {
                // Mouse cannot turn an unavailable opponent's statistic on either.
                mouseClick(*button);
                expectAxis();
            }
        }
        for(unsigned i = 0; i < 8; ++i)
            expected.insert(group(typeGroup).GetCtrl<ctrlImageButton>(typeGroup + 1u + i));
        for(unsigned i = 0; i < 4; ++i)
            expected.insert(group(timeGroup).GetCtrl<ctrlTextButton>(timeGroup + 1u + i));
        focusUntil(window->GetCtrl<ctrlImageButton>(helpId));
        std::set<const Window*> visited;
        for(const auto direction : {PadButton::LeftShoulder, PadButton::RightShoulder})
        {
            for(unsigned n = 0; n < 20; ++n)
            {
                visited.insert(focused());
                press(direction);
            }
        }
        BOOST_TEST(visited == expected);
        BOOST_TEST(world().GetGGS().getSelection(AddonId::STATISTICS_VISIBILITY) == policy % 3u);
    }
    void browseAll()
    {
        const std::array<const char*, 8> headers{{"Size of country", "Buildings", "Inhabitants", "Merchandise",
                                                  "Military strength", "Gold", "Productivity", "Vanquished enemies"}};
        for(const auto time : helpers::enumRange<StatisticTime>())
        {
            click(group(timeGroup).GetCtrl<ctrlTextButton>(timeGroup + 1u + rttr::enum_cast(time)));
            duration = time;
            BOOST_TEST(group(timeGroup).GetSelection() == timeGroup + 1u + rttr::enum_cast(time));
            expectTimeLabels();
            for(unsigned i = 0; i < headers.size(); ++i)
            {
                click(group(typeGroup).GetCtrl<ctrlImageButton>(typeGroup + 1u + i));
                category = static_cast<StatisticType>(i);
                BOOST_TEST(group(typeGroup).GetSelection() == typeGroup + 1u + i);
                BOOST_TEST(label(headlineId).GetText() == headers[i]);
                expectAxis();
            }
        }
    }
    void togglePlayers()
    {
        for(unsigned p = 0; p < visible.size(); ++p)
        {
            auto* button = window->GetCtrl<ctrlImageButton>(p);
            if(!button->GetEnabled())
                continue;
            click(button);
            visible[p] = !visible[p];
            expectAxis();
        }
        BOOST_TEST(std::none_of(visible.begin(), visible.end(), [](const bool on) { return on; }));
        browseAll();
        for(unsigned p = 0; p < visible.size(); ++p)
        {
            auto* button = window->GetCtrl<ctrlImageButton>(p);
            if(!button->GetEnabled())
                continue;
            mouseClick(*button);
            visible[p] = true;
            expectAxis();
        }
    }
    void helpCloseAndReopen(const unsigned policy)
    {
        click(window->GetCtrl<ctrlImageButton>(helpId));
        BOOST_TEST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        press(PadButton::Y);
        expectAxis();
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(dynamic_cast<iwMainMenu*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        window = nullptr;
        openFromMain();
        // This window intentionally starts at its defaults; there is no saved selection state.
        initializeVisible(policy);
        BOOST_TEST(group(typeGroup).GetSelection() == typeGroup + 1u);
        BOOST_TEST(group(timeGroup).GetSelection() == timeGroup + 1u);
        expectTimeLabels();
        expectAxis();
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(PadStatisticsTests)

BOOST_DATA_TEST_CASE_F(StatisticsPadFixture, CategoriesTimesVisibilityAndEmptyScaledChart,
                       boost::unit_test::data::make({0u, 1u, 2u, 3u, 4u, 5u}), policy)
{
    run(policy, [this, policy] {
        seedHistory();
        enter();
        initializeVisible(policy);
        const auto before = AsyncChecksum::create(*ci().game);
        const auto gf = GAMECLIENT.GetGFNumber();
        const auto history = snapshotHistory();
        expectAxis();
        expectFocusPolicy(policy);
        browseAll();
        togglePlayers();
        helpCloseAndReopen(policy);
        BOOST_TEST((AsyncChecksum::create(*ci().game) == before));
        BOOST_TEST(GAMECLIENT.GetGFNumber() == gf);
        expectHistoryUnchanged(history);
        BOOST_TEST(ci().numErrors == 0u);
        BOOST_TEST(ci().numAsync == 0u);
    });
    expectRestored();
}

BOOST_FIXTURE_TEST_CASE(ConstantPositiveHistoryKeepsThePopulatedScaleMargin, StatisticsPadFixture)
{
    run(5, [this] {
        auto& player = world().GetPlayer(0);
        for(unsigned i = 0; i < NUM_STAT_STEPS; ++i)
        {
            player.SetStatisticValue(StatisticType::Country, 42);
            player.StatisticStep();
        }
        enter();
        initializeVisible(5);
        BOOST_TEST(label(minY).GetText() == "41");
        BOOST_TEST(label(maxY).GetText() == "43");
        click(window->GetCtrl<ctrlImageButton>(0));
        BOOST_TEST(label(minY).GetText() == "0");
        BOOST_TEST(label(maxY).GetText() == "1");
        mouseClick(*window->GetCtrl<ctrlImageButton>(0));
        BOOST_TEST(label(minY).GetText() == "41");
        BOOST_TEST(label(maxY).GetText() == "43");
    });
    expectRestored();
}

BOOST_DATA_TEST_CASE_F(StatisticsPadFixture, ReplayAllowsOpponentBrowsingRegardlessOfLiveVisibility,
                       boost::unit_test::data::make({2u, 5u}), policy)
{
    run(policy, [this, policy] {
        pumpUntilGF(GAMECLIENT.GetGFNumber() + 40u);
        const auto replay = RTTRCONFIG.ExpandPath(s25::folders::replays) / GAMECLIENT.GetReplayFilename();
        cleanupDesktop();
        preventWindowRestore();
        GAMECLIENT.Stop();
        GAMESERVER.Stop();
        GAMECLIENT.SetInterface(&ci());
        BOOST_TEST_REQUIRE(GAMECLIENT.StartReplay(replay));
        GAMECLIENT.GameLoaded();
        makeDesktop();
        GAMECLIENT.SetPause(false);
        enter();
        initializeVisible(policy);
        const auto history = snapshotHistory();
        expectFocusPolicy(policy);
        browseAll();
        togglePlayers();
        helpCloseAndReopen(policy);
        expectHistoryUnchanged(history);
        GAMECLIENT.skiptogf = GAMECLIENT.GetLastReplayGF();
        pumpUntil([this] { return ci().replayEnded || ci().numErrors || ci().numReplayAsync; },
                  "statistics replay to finish");
        frame();
        BOOST_TEST(ci().replayEnded);
        expectAxis();
        BOOST_TEST(ci().numErrors == 0u);
        BOOST_TEST(ci().numReplayAsync == 0u);
    });
}

BOOST_AUTO_TEST_CASE(ExceptionalCleanupRestoresSettingsBeforeFixtureDestruction)
{
    const auto scaleBefore = SETTINGS.ingame.scaleStatistics;
    const auto debugBefore = SETTINGS.global.debugMode;
    {
        StatisticsPadFixture fixture;
        bool reachedProbe = false;
        BOOST_CHECK_THROW(fixture.run(3,
                                      [&fixture, &reachedProbe] {
                                          fixture.enter();
                                          reachedProbe = true;
                                          throwCleanupProbe();
                                      }),
                          std::runtime_error);
        BOOST_TEST(reachedProbe);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        fixture.expectRestored();
        BOOST_TEST(SETTINGS.ingame.scaleStatistics == scaleBefore);
        BOOST_TEST(SETTINGS.global.debugMode == debugBefore);
    }
    BOOST_TEST(SETTINGS.ingame.scaleStatistics == scaleBefore);
    BOOST_TEST(SETTINGS.global.debugMode == debugBefore);
}

BOOST_AUTO_TEST_SUITE_END()
