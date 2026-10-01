// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AsyncChecksum.h"
#include "LocalGameFixture.h"
#include "PadFixture.h"
#include "controls/ctrlImageButton.h"
#include "controls/ctrlMultiSelectGroup.h"
#include "controls/ctrlOptionGroup.h"
#include "controls/ctrlText.h"
#include "controls/ctrlTextButton.h"
#include "driver/MouseCoords.h"
#include "helpers/EnumRange.h"
#include "ingameWindows/iwHelp.h"
#include "ingameWindows/iwMainMenu.h"
#include "ingameWindows/iwMerchandiseStatistics.h"
#include "ingameWindows/iwPadSystemMenu.h"
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>

namespace {
constexpr PadDeviceId pad = 110;
constexpr std::array<GoodType, 14> goods = {GoodType::Wood,  GoodType::Boards, GoodType::Stones, GoodType::Fish,
                                            GoodType::Water, GoodType::Beer,   GoodType::Coal,   GoodType::IronOre,
                                            GoodType::Gold,  GoodType::Iron,   GoodType::Coins,  GoodType::Hammer,
                                            GoodType::Sword, GoodType::Boat};
constexpr unsigned numRanges = 4;

[[noreturn]] void throwCleanupProbe()
{
    throw std::runtime_error("merchandise statistics cleanup probe");
}

struct MerchandisePadFixture : uiHelper::Fixture, rttr::test::LocalGameFixture
{
    rttr::test::PadFeeder pads{*uiHelper::GetVideoDriver()};
    rttr::test::TestableGameInterface* dsk = nullptr;
    iwMerchandiseStatistics* window = nullptr;
    unsigned recordedFinalGF = 0;
    AsyncChecksum recordedChecksum;
    const bool savedDebugMode = SETTINGS.global.debugMode;
    const decltype(SETTINGS.windows.persistentSettings) savedWindows = SETTINGS.windows.persistentSettings;

    template<class F>
    void run(F&& body)
    {
        try
        {
            beginGame();
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
        BOOST_TEST(SETTINGS.global.debugMode == savedDebugMode);
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
    void cleanupDesktop()
    {
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        WINDOWMANAGER.Draw();
        if(ci().game)
            world().SetGameInterface(nullptr);
        dsk = nullptr;
        window = nullptr;
    }
    void cleanup()
    {
        // Windows hold pointers into this map; restore it only after they have been destroyed.
        cleanupDesktop();
        SETTINGS.windows.persistentSettings = savedWindows;
        SETTINGS.global.debugMode = savedDebugMode;
    }
    void beginGame()
    {
        SETTINGS.global.debugMode = false;
        hostAndEnterLobby();
        ci().numErrors = 0; // A recovered bind/connect attempt is not a gameplay error.
        lobby().SetPlayerState(1, PlayerState::AI, AI::Info(AI::Type::Dummy));
        lobby().SetPlayerState(2, PlayerState::AI, AI::Info(AI::Type::Dummy));
        pumpUntil(
          [] {
              const auto lobby = GAMECLIENT.GetGameLobby();
              return lobby->getPlayer(1).ps == PlayerState::AI && lobby->getPlayer(2).ps == PlayerState::AI;
          },
          "dummy AI slots");
        startGame();
        makeDesktop();
    }
    void makeDesktop()
    {
        for(auto& entry : SETTINGS.windows.persistentSettings)
            entry.second.isOpen = false;
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
    Window* focused() const { return dsk->GetPlayerView(0).GetFocus().GetFocused(); }
    void focusUntil(const Window* target)
    {
        if(dsk->GetPlayerView(0).GetRing().IsOpen())
        {
            for(unsigned i = 0; i < 8 && focused() != target; ++i)
                press(PadButton::DpadRight);
        } else
        {
            for(const auto direction : {PadButton::RightShoulder, PadButton::LeftShoulder})
            {
                for(unsigned i = 0; i < 30 && focused() != target; ++i)
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
        press(PadButton::DpadRight);
        focusUntil(system->GetCtrl<ctrlTextButton>(iwPadSystemMenu::ID_MAIN_SELECTION));
        press(PadButton::A);
        openFromMain();
    }
    void openFromMain()
    {
        auto* main = dynamic_cast<iwMainMenu*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(main != nullptr);
        press(PadButton::Y);
        focusUntil(main->GetCtrl<ctrlImageButton>(4));
        press(PadButton::A);
        window = dynamic_cast<iwMerchandiseStatistics*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(window != nullptr);
        press(PadButton::Y);
        BOOST_TEST_REQUIRE(dsk->GetPlayerView(0).GetFocus().GetRoot() == window);
    }
    ctrlMultiSelectGroup& types() const { return *window->GetCtrl<ctrlMultiSelectGroup>(22); }
    ctrlOptionGroup& times() const { return *window->GetCtrl<ctrlOptionGroup>(23); }
    void activate(Window* control)
    {
        BOOST_TEST_REQUIRE(control != nullptr);
        focusUntil(control);
        press(PadButton::A);
    }
    void mouseClick(const Window& control)
    {
        MouseCoords mc(control.GetDrawPos() + DrawPoint(10, 5));
        mc.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mc);
        mc.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mc);
        frame();
    }
    void close()
    {
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(dynamic_cast<iwMainMenu*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        window = nullptr;
    }
    void expectSelection(const std::set<unsigned>& expected) const
    {
        BOOST_TEST(types().GetSelection() == expected, boost::test_tools::per_element());
        for(unsigned id = 1; id <= goods.size(); ++id)
            BOOST_TEST(types().GetCtrl<ctrlImageButton>(id)->GetIlluminated() == (expected.count(id) != 0));
    }
    void expectMax(const unsigned value) const
    {
        BOOST_TEST(window->GetCtrl<ctrlText>(31)->GetText() == std::to_string(value));
    }
    void expectTime(const unsigned range) const
    {
        const std::array<std::array<const char*, 7>, 4> labels = {{{"-15", "-12", "-9", "-6", "-3", "", "0"},
                                                                   {"-60", "-50", "-40", "-30", "-20", "-10", "0"},
                                                                   {"-240", "-180", "-120", "-60", "", "", "0"},
                                                                   {"-960", "-720", "-480", "-240", "", "", "0"}}};
        BOOST_TEST(times().GetSelection() == 18u + range);
        for(unsigned i = 0; i < labels[range].size(); ++i)
        {
            const auto* label = window->GetCtrl<ctrlText>(32u + i);
            const bool visible = *labels[range][i] != '\0';
            BOOST_TEST(label->IsVisible() == visible);
            if(visible)
                BOOST_TEST(label->GetText() == labels[range][i]);
        }
    }
    void expectSelectedMax(const unsigned range, const std::set<unsigned>& selected)
    {
        const auto& data = world().GetPlayer(0).GetStatistic(static_cast<StatisticTime>(range)).merchandiseData;
        unsigned expected = 1;
        for(const unsigned id : selected)
            expected =
              std::max(expected, static_cast<unsigned>(*std::max_element(data[id - 1].begin(), data[id - 1].end())));
        expectMax(expected);
    }
    void seedHistory()
    {
        auto& player = world().GetPlayer(0);
        BOOST_TEST_REQUIRE(player.GetStatistic(StatisticTime::T15Minutes).currentIndex == 1u);
        BOOST_TEST_REQUIRE(player.GetStatistic(StatisticTime::T15Minutes).counter == 1u);
        // Real aggregation, with all four circular histories wrapped. The wood spike is outside
        // the 15-minute window, but retained in an older hourly bucket.
        for(unsigned step = 1; step <= 2176; ++step)
        {
            for(unsigned i = 0; i < goods.size(); ++i)
                for(unsigned count = 0; count <= i; ++count)
                    player.IncreaseMerchandiseStatistic(goods[i]);
            if(step == 2100)
                for(unsigned count = 0; count < 20; ++count)
                    player.IncreaseMerchandiseStatistic(GoodType::Wood);
            player.StatisticStep();
        }
        const std::array<unsigned, 4> indices = {17, 4, 16, 4};
        const std::array<unsigned, 4> counters = {1, 0, 0, 34};
        for(unsigned range = 0; range < numRanges; ++range)
        {
            const auto& stat = player.GetStatistic(static_cast<StatisticTime>(range));
            const auto& boats = stat.merchandiseData[13];
            BOOST_TEST(stat.currentIndex == indices[range]);
            BOOST_TEST(stat.counter == counters[range]);
            BOOST_TEST(std::all_of(boats.begin(), boats.end(), [](const auto value) { return value > 0; }));
        }
        const auto& hourlyWood = player.GetStatistic(StatisticTime::T1Hour).merchandiseData[0];
        BOOST_TEST(*std::max_element(hourlyWood.begin(), hourlyWood.end()) == 24u);
        BOOST_TEST(hourlyWood[player.GetStatistic(StatisticTime::T1Hour).currentIndex] == 4u);
    }
    auto histories()
    {
        std::array<GamePlayer::Statistic, 4> result;
        for(unsigned range = 0; range < result.size(); ++range)
            result[range] = world().GetPlayer(0).GetStatistic(static_cast<StatisticTime>(range));
        return result;
    }
    void expectHistory(const std::array<GamePlayer::Statistic, 4>& expected)
    {
        const auto actual = histories();
        for(unsigned range = 0; range < actual.size(); ++range)
        {
            BOOST_TEST(actual[range].currentIndex == expected[range].currentIndex);
            BOOST_TEST(actual[range].counter == expected[range].counter);
            for(unsigned type = 0; type < goods.size(); ++type)
                // Boost's collection comparator calls std::begin on MultiArrayRef, whose
                // missing noexcept declaration is rejected by GCC16's -Wnoexcept.
                for(unsigned step = 0; step < NUM_STAT_STEPS; ++step)
                    BOOST_TEST(actual[range].merchandiseData[type][step]
                               == expected[range].merchandiseData[type][step]);
            for(const auto type : helpers::EnumRange<StatisticType>{})
                BOOST_TEST(actual[range].data[type] == expected[range].data[type], boost::test_tools::per_element());
        }
        BOOST_TEST(ci().numErrors == 0u);
        BOOST_TEST(ci().numAsync == 0u);
        BOOST_TEST(ci().numReplayAsync == 0u);
    }
    void browseRanges()
    {
        activate(types().GetCtrl<ctrlImageButton>(1));
        for(unsigned range = 0; range < numRanges; ++range)
        {
            activate(times().GetCtrl<ctrlTextButton>(18u + range));
            expectTime(range);
            expectSelectedMax(range, {1});
            activate(types().GetCtrl<ctrlImageButton>(14));
            expectSelection({1, 14});
            expectSelectedMax(range, {1, 14});
            activate(types().GetCtrl<ctrlImageButton>(14));
            expectSelection({1});
        }
        // Switch backwards physically: annotation visibility must recover as well as the value.
        mouseClick(*times().GetCtrl<ctrlTextButton>(19));
        expectTime(1);
        expectMax(24);
        mouseClick(*times().GetCtrl<ctrlTextButton>(18));
        expectTime(0);
        expectMax(1);
    }
    boost::filesystem::path recordSavedHistory()
    {
        const auto save = RTTRCONFIG.ExpandPath(s25::folders::save) / "merchandise.sav";
        boost::filesystem::create_directories(save.parent_path());
        BOOST_TEST_REQUIRE(GAMECLIENT.SaveToFile(save));
        cleanupDesktop();
        bool inLobby = false;
        for(unsigned attempt = 0; attempt < 10 && !inLobby; ++attempt)
        {
            // Retry binding AND connecting; a bound port can still fail the loopback connection.
            GAMECLIENT.Stop();
            GAMESERVER.Stop();
            GAMECLIENT.SetInterface(&ci());
            const auto port = static_cast<uint16_t>(rttr::test::randomValue(1024, 49151));
            if(GAMECLIENT.HostGame(CreateServerInfo(ServerType::Local, port, "Merchandise saved history"),
                                   MapDescription(save, MapType::Savegame)))
                inLobby =
                  pumpWhile([] { return GAMECLIENT.GetState() == ClientState::Config; }, std::chrono::seconds(10));
        }
        BOOST_TEST_REQUIRE(inLobby);
        ci().numErrors = 0;
        GameLobbyController controller(GAMECLIENT.GetGameLobby(), GAMECLIENT.GetMainPlayer());
        GAMECLIENT.Command_SetReady(true);
        controller.StartCountdown(0);
        pumpUntil([] { return GAMECLIENT.GetState() == ClientState::Loading; }, "saved history loading");
        GAMECLIENT.GameLoaded();
        pumpUntil([] { return GAMECLIENT.GetState() == ClientState::Game; }, "saved history game");
        GAMECLIENT.OnGameStart();
        BOOST_TEST_REQUIRE(ci().game->IsStarted());
        const MilitarySettings recorded{{1, 2, 3, 4, 5, 6, 7, 8}};
        BOOST_TEST_REQUIRE(GAMECLIENT.GetGCFactory(0)->ChangeMilitary(recorded));
        pumpUntil(
          [this, &recorded] {
              VisualSettings actual;
              world().GetPlayer(0).FillVisualSettings(actual);
              return actual.military_settings == recorded;
          },
          "recorded command roundtrip");
        pumpUntilGF(GAMECLIENT.GetGFNumber() + 40u);
        recordedFinalGF = GAMECLIENT.GetGFNumber();
        recordedChecksum = AsyncChecksum::create(*ci().game);
        const auto replay = RTTRCONFIG.ExpandPath(s25::folders::replays) / GAMECLIENT.GetReplayFilename();
        GAMECLIENT.Stop();
        GAMESERVER.Stop();
        BOOST_TEST_REQUIRE(boost::filesystem::is_regular_file(replay));
        return replay;
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(PadMerchandiseStatisticsTests)

BOOST_FIXTURE_TEST_CASE(AllFourteenNestedGoodsToggleIndependentlyAndClearWithoutChangingHistory, MerchandisePadFixture)
{
    run([this] {
        seedHistory();
        const auto before = histories();
        const auto checksum = AsyncChecksum::create(*ci().game);
        enter();
        expectSelection({});
        expectMax(1);
        std::set<unsigned> expected;
        for(unsigned id = 1; id <= goods.size(); ++id)
        {
            activate(types().GetCtrl<ctrlImageButton>(id));
            expected.insert(id);
            expectSelection(expected);
            expectMax(std::max(24u, id * 4u));
        }
        for(unsigned id = goods.size(); id > 0; --id)
        {
            activate(types().GetCtrl<ctrlImageButton>(id));
            expected.erase(id);
            expectSelection(expected);
            expectMax(id == 1 ? 1 : std::max(24u, (id - 1u) * 4u));
        }
        activate(types().GetCtrl<ctrlImageButton>(2));
        activate(types().GetCtrl<ctrlImageButton>(14));
        activate(window->GetCtrl<ctrlImageButton>(17));
        expectSelection({});
        expectMax(1);
        activate(window->GetCtrl<ctrlImageButton>(17));
        expectMax(1);
        close();
        openFromMain();
        expectSelection({});
        expectTime(1);
        expectMax(1);
        expectHistory(before);
        BOOST_TEST((AsyncChecksum::create(*ci().game) == checksum));
    });
}

BOOST_FIXTURE_TEST_CASE(AllTimeRangesUseWrappedRealHistoryAndRestoreAnnotationVisibility, MerchandisePadFixture)
{
    run([this] {
        seedHistory();
        const auto before = histories();
        enter();
        browseRanges();
        close();
        expectHistory(before);
    });
}

BOOST_FIXTURE_TEST_CASE(MouseMultiselectClearAndHelpRemainUsableAlongsideControllerFocus, MerchandisePadFixture)
{
    run([this] {
        seedHistory();
        const auto before = histories();
        const auto inventory = world().GetPlayer(0).GetInventory();
        enter();
        for(unsigned id = 1; id <= goods.size(); ++id)
        {
            mouseClick(*types().GetCtrl<ctrlImageButton>(id));
            expectSelection({id});
            expectMax(id == 1 ? 24 : id * 4u);
            mouseClick(*types().GetCtrl<ctrlImageButton>(id));
            expectSelection({});
            expectMax(1);
        }
        mouseClick(*types().GetCtrl<ctrlImageButton>(1));
        mouseClick(*types().GetCtrl<ctrlImageButton>(14));
        expectSelection({1, 14});
        mouseClick(*window->GetCtrl<ctrlImageButton>(17));
        expectSelection({});
        activate(window->GetCtrl<ctrlImageButton>(16));
        // ReplaceWindow replaces an existing Help window; Merchandise stays underneath.
        BOOST_TEST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        press(PadButton::Y);
        close();
        openFromMain();
        expectSelection({});
        activate(types().GetCtrl<ctrlImageButton>(14));
        expectMax(56);
        close();
        expectHistory(before);
        for(const auto good : helpers::EnumRange<GoodType>{})
            BOOST_TEST(world().GetPlayer(0).GetInventory()[good] == inventory[good]);
        for(const auto job : helpers::EnumRange<Job>{})
            BOOST_TEST(world().GetPlayer(0).GetInventory()[job] == inventory[job]);
    });
}

BOOST_FIXTURE_TEST_CASE(SavedHistoryIsRestoredInRealReplayAndBrowsingDoesNotChangeItsWorld, MerchandisePadFixture)
{
    run([this] {
        seedHistory();
        const auto before = histories();
        const auto replay = recordSavedHistory();

        GAMECLIENT.SetInterface(&ci());
        BOOST_TEST_REQUIRE(GAMECLIENT.StartReplay(replay));
        GAMECLIENT.GameLoaded();
        makeDesktop();
        expectHistory(before);
        GAMECLIENT.SetPause(false);
        enter();
        browseRanges();
        activate(window->GetCtrl<ctrlImageButton>(17));
        expectSelection({});
        expectMax(1);
        GAMECLIENT.skiptogf = GAMECLIENT.GetLastReplayGF();
        pumpUntil([this] { return ci().replayEnded || ci().numErrors || ci().numReplayAsync; }, "replay completion");
        frame();
        BOOST_TEST(ci().replayEnded);
        BOOST_TEST(GAMECLIENT.GetGFNumber() == recordedFinalGF);
        BOOST_TEST((AsyncChecksum::create(*ci().game) == recordedChecksum));
        VisualSettings actual;
        world().GetPlayer(0).FillVisualSettings(actual);
        const MilitarySettings recorded{{1, 2, 3, 4, 5, 6, 7, 8}};
        BOOST_TEST(actual.military_settings == recorded, boost::test_tools::per_element());
        expectHistory(before);
        close();
        openFromMain();
        expectSelection({});
        expectTime(1);
    });
}

BOOST_AUTO_TEST_CASE(ExceptionalCleanupRestoresSettingsWhileTheFixtureStillExists)
{
    {
        MerchandisePadFixture fixture;
        bool reachedProbe = false;
        BOOST_CHECK_THROW(fixture.run([&fixture, &reachedProbe] {
            fixture.enter();
            reachedProbe = true;
            throwCleanupProbe();
        }),
                          std::runtime_error);
        BOOST_TEST(reachedProbe);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(dynamic_cast<dskGameInterface*>(WINDOWMANAGER.GetCurrentDesktop()) == nullptr);
        fixture.expectRestored();
    }
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
}

BOOST_AUTO_TEST_SUITE_END()
