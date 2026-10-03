// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AsyncChecksum.h"
#include "PadGameFixture.h"
#include "buildings/nobBaseWarehouse.h"
#include "controls/ctrlGroup.h"
#include "controls/ctrlImage.h"
#include "controls/ctrlImageButton.h"
#include "controls/ctrlText.h"
#include "controls/ctrlTextButton.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "factories/BuildingFactory.h"
#include "helpers/EnumRange.h"
#include "ingameWindows/iwHelp.h"
#include "ingameWindows/iwInventory.h"
#include "ingameWindows/iwMainMenu.h"
#include "ingameWindows/iwPadSystemMenu.h"
#include "gameData/JobConsts.h"
#include "gameData/ShieldConsts.h"
#include <boost/test/unit_test.hpp>
#include <array>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
constexpr PadDeviceId pad = 114;
constexpr unsigned warePage = 100;
constexpr unsigned peoplePage = 101;
constexpr unsigned wine = 1;
constexpr unsigned leather = 2;
constexpr unsigned charburner = 4;

[[noreturn]] void throwCleanupProbe()
{
    throw std::runtime_error("stock cleanup probe");
}

struct InventoryPadFixture : uiHelper::Fixture, rttr::test::LocalGameFixture
{
    rttr::test::PadFeeder pads{*uiHelper::GetVideoDriver()};
    rttr::test::TestableGameInterface* dsk = nullptr;
    iwInventory* window = nullptr;
    unsigned finalGF = 0;
    AsyncChecksum finalChecksum;
    const bool savedDebugMode = SETTINGS.global.debugMode;
    const decltype(SETTINGS.windows.persistentSettings) savedWindows = SETTINGS.windows.persistentSettings;

    template<class F>
    void run(F&& body, const Nation nation = Nation::Romans, const unsigned addons = 0)
    {
        try
        {
            beginGame(nation, addons);
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
            BOOST_TEST(it->second.isOpen == entry.second.isOpen);
            BOOST_TEST(it->second.isMinimized == entry.second.isMinimized);
            BOOST_TEST(it->second.isPinned == entry.second.isPinned);
            BOOST_TEST((it->second.lastPos == entry.second.lastPos));
            BOOST_TEST((it->second.restorePos == entry.second.restorePos));
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
        // Open windows retain pointers into persistentSettings until destruction.
        cleanupDesktop();
        SETTINGS.windows.persistentSettings = savedWindows;
        SETTINGS.global.debugMode = savedDebugMode;
    }
    void beginGame(const Nation nation, const unsigned addons)
    {
        SETTINGS.global.debugMode = false;
        hostAndEnterLobby();
        ci().numErrors = 0;
        lobby().SetNation(0, nation);
        lobby().SetPlayerState(1, PlayerState::AI, AI::Info(AI::Type::Dummy));
        lobby().SetPlayerState(2, PlayerState::AI, AI::Info(AI::Type::Dummy));
        auto settings = lobby().GetGGS();
        settings.resetAddons();
        settings.setSelection(AddonId::WINE, (addons & wine) != 0);
        settings.setSelection(AddonId::LEATHER, (addons & leather) != 0);
        settings.setSelection(AddonId::CHARBURNER, (addons & charburner) != 0);
        lobby().ChangeGlobalGameSettings(settings);
        pumpUntil(
          [nation] {
              const auto lobby = GAMECLIENT.GetGameLobby();
              return lobby->getPlayer(0).nation == nation && lobby->getPlayer(1).ps == PlayerState::AI
                     && lobby->getPlayer(2).ps == PlayerState::AI;
          },
          "stock nation and dummy AI slots");
        startGame();
        BOOST_TEST_REQUIRE((world().GetPlayer(0).nation == nation));
        BOOST_TEST(world().GetGGS().isEnabled(AddonId::WINE) == ((addons & wine) != 0));
        BOOST_TEST(world().GetGGS().isEnabled(AddonId::LEATHER) == ((addons & leather) != 0));
        BOOST_TEST(world().GetGGS().isEnabled(AddonId::CHARBURNER) == ((addons & charburner) != 0));
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
        BOOST_REQUIRE(target != nullptr);
        if(dsk->GetPlayerView(0).GetRing().IsOpen())
        {
            for(unsigned i = 0; i < 8 && focused() != target; ++i)
                press(PadButton::DpadRight);
        } else
        {
            for(const auto direction : {PadButton::RightShoulder, PadButton::LeftShoulder})
                for(unsigned i = 0; i < 20 && focused() != target; ++i)
                    press(direction);
        }
        BOOST_TEST_REQUIRE(focused() == target);
    }
    void activate(Window* control)
    {
        focusUntil(control);
        press(PadButton::A);
    }
    void enter()
    {
        press(PadButton::Back);
        auto* system = dynamic_cast<iwPadSystemMenu*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_REQUIRE(system != nullptr);
        focusUntil(system->GetCtrl<ctrlTextButton>(iwPadSystemMenu::ID_MAIN_SELECTION));
        press(PadButton::A);
        openFromMain();
    }
    void openFromMain()
    {
        auto* main = dynamic_cast<iwMainMenu*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_REQUIRE(main != nullptr);
        press(PadButton::Y);
        activate(main->GetCtrl<ctrlImageButton>(6));
        window = dynamic_cast<iwInventory*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_REQUIRE(window != nullptr);
        press(PadButton::Y);
        BOOST_TEST_REQUIRE(dsk->GetPlayerView(0).GetFocus().GetRoot() == window);
        expectPage(warePage);
    }
    ctrlGroup& page(const unsigned id) const
    {
        auto* group = window->GetCtrl<ctrlGroup>(id);
        BOOST_REQUIRE(group != nullptr);
        return *group;
    }
    void expectPage(const unsigned id) const
    {
        BOOST_TEST(page(warePage).IsVisible() == (id == warePage));
        BOOST_TEST(page(peoplePage).IsVisible() == (id == peoplePage));
    }
    void nextPage(const unsigned expected)
    {
        activate(window->GetCtrl<ctrlImageButton>(0));
        expectPage(expected);
    }
    void mouseClick(const Window& control)
    {
        MouseCoords event(control.GetDrawPos() + control.GetSize() / 2u);
        event.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(event);
        event.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(event);
        frame();
    }
    void close()
    {
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        press(PadButton::B);
        BOOST_REQUIRE(dynamic_cast<iwMainMenu*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        window = nullptr;
    }
    nobBaseWarehouse& hq()
    {
        auto* result = world().GetSpecObj<nobBaseWarehouse>(world().GetPlayer(0).GetHQPos());
        BOOST_REQUIRE(result != nullptr);
        return *result;
    }
    void seedStock()
    {
        GoodsAndPeopleCounts added;
        for(const auto good : helpers::EnumRange<GoodType>{})
            if(ConvertShields(good) == good)
                added[good] = 1000u + rttr::enum_cast(good);
        for(const auto job : helpers::EnumRange<Job>{})
            if(job != Job::BoatCarrier)
                added[job] = 2000u + rttr::enum_cast(job);
        for(const auto soldier : helpers::EnumRange<ArmoredSoldier>{})
            added[soldier] = 10u + rttr::enum_cast(soldier);
        // Seed the actual HQ and the player's aggregate together, so a saved replay is valid.
        hq().AddToInventory(added, true);
        frame();
    }
    void expectAmount(const unsigned pageId, const unsigned item, const unsigned amount) const
    {
        const auto* text = page(pageId).GetCtrl<ctrlText>(600u + item);
        BOOST_REQUIRE(text != nullptr);
        BOOST_TEST(text->GetText() == std::to_string(amount));
        BOOST_TEST(text->GetTextColor() == (amount ? COLOR_YELLOW : COLOR_RED));
        const auto* background = page(pageId).GetCtrl<ctrlImage>(100u + item);
        BOOST_REQUIRE(background != nullptr);
        BOOST_TEST(!background->CanFocus());
        BOOST_TEST(page(pageId).GetCtrl<ctrlButton>(100u + item) == nullptr);
        for(const unsigned offset : {400u, 500u, 700u})
        {
            const auto* overlay = page(pageId).GetCtrl<ctrlImage>(offset + item);
            BOOST_REQUIRE(overlay != nullptr);
            BOOST_TEST(!overlay->IsVisible());
        }
    }
    void expectVisibleCounts(const unsigned pageId)
    {
        const auto& inventory = world().GetPlayer(0).GetInventory();
        if(pageId == warePage)
        {
            for(const auto good : helpers::EnumRange<GoodType>{})
                if(page(warePage).GetCtrl<ctrlText>(600u + rttr::enum_cast(good)))
                    expectAmount(warePage, rttr::enum_cast(good), inventory[good]);
        } else
        {
            for(const auto job : helpers::EnumRange<Job>{})
                if(page(peoplePage).GetCtrl<ctrlText>(600u + rttr::enum_cast(job)))
                    expectAmount(peoplePage, rttr::enum_cast(job), inventory[job]);
        }
    }
    void expectStock(const Inventory& expected)
    {
        const auto& actual = world().GetPlayer(0).GetInventory();
        for(const auto good : helpers::EnumRange<GoodType>{})
            BOOST_TEST(actual[good] == expected[good]);
        for(const auto job : helpers::EnumRange<Job>{})
            BOOST_TEST(actual[job] == expected[job]);
        for(const auto soldier : helpers::EnumRange<ArmoredSoldier>{})
            BOOST_TEST(actual[soldier] == expected[soldier]);
        BOOST_TEST(ci().numErrors == 0u);
        BOOST_TEST(ci().numAsync == 0u);
        BOOST_TEST(ci().numReplayAsync == 0u);
    }
    void expectRows(const unsigned addons) const
    {
        const auto wareShown = [this](const GoodType good, const bool expected) {
            BOOST_TEST((page(warePage).GetCtrl<ctrlText>(600u + rttr::enum_cast(good)) != nullptr) == expected);
        };
        const auto jobShown = [this](const Job job, const bool expected) {
            BOOST_TEST((page(peoplePage).GetCtrl<ctrlText>(600u + rttr::enum_cast(job)) != nullptr) == expected);
        };
        wareShown(GoodType::Wood, true);
        wareShown(GoodType::ShieldRomans, true);
        for(const auto good :
            {GoodType::ShieldVikings, GoodType::ShieldAfricans, GoodType::ShieldJapanese, GoodType::WaterEmpty})
            wareShown(good, false);
        for(const auto good : {GoodType::Grapes, GoodType::Wine})
            wareShown(good, (addons & wine) != 0);
        for(const auto good : {GoodType::Skins, GoodType::Leather, GoodType::Armor})
            wareShown(good, (addons & leather) != 0);
        jobShown(Job::Helper, true);
        jobShown(Job::General, true);
        jobShown(Job::BoatCarrier, false);
        jobShown(Job::CharBurner, (addons & charburner) != 0);
        for(const auto job : {Job::Winegrower, Job::Vintner, Job::TempleServant})
            jobShown(job, (addons & wine) != 0);
        for(const auto job : {Job::Skinner, Job::Tanner, Job::LeatherWorker})
            jobShown(job, (addons & leather) != 0);
        BOOST_TEST(page(warePage).GetCtrls<ctrlText>().size()
                   == 31u + ((addons & wine) ? 2u : 0u) + ((addons & leather) ? 3u : 0u));
        BOOST_TEST(page(peoplePage).GetCtrls<ctrlText>().size()
                   == 30u + ((addons & wine) ? 3u : 0u) + ((addons & leather) ? 3u : 0u)
                        + ((addons & charburner) ? 1u : 0u));
    }
    boost::filesystem::path recordSavedStock()
    {
        const auto save = RTTRCONFIG.ExpandPath(s25::folders::save) / "stock.sav";
        boost::filesystem::create_directories(save.parent_path());
        BOOST_TEST_REQUIRE(GAMECLIENT.SaveToFile(save));
        cleanupDesktop();
        bool inLobby = false;
        for(unsigned attempt = 0; attempt < 10 && !inLobby; ++attempt)
        {
            GAMECLIENT.Stop();
            GAMESERVER.Stop();
            GAMECLIENT.SetInterface(&ci());
            const auto port = static_cast<uint16_t>(rttr::test::randomValue(1024, 49151));
            if(GAMECLIENT.HostGame(CreateServerInfo(ServerType::Local, port, "Saved stock"),
                                   MapDescription(save, MapType::Savegame)))
                inLobby =
                  pumpWhile([] { return GAMECLIENT.GetState() == ClientState::Config; }, std::chrono::seconds(10));
        }
        BOOST_TEST_REQUIRE(inLobby);
        ci().numErrors = 0;
        GameLobbyController controller(GAMECLIENT.GetGameLobby(), GAMECLIENT.GetMainPlayer());
        GAMECLIENT.Command_SetReady(true);
        controller.StartCountdown(0);
        pumpUntil([] { return GAMECLIENT.GetState() == ClientState::Loading; }, "saved stock loading");
        GAMECLIENT.GameLoaded();
        pumpUntil([] { return GAMECLIENT.GetState() == ClientState::Game; }, "saved stock game");
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
          "recorded stock command roundtrip");
        pumpUntilGF(GAMECLIENT.GetGFNumber() + 40u);
        finalGF = GAMECLIENT.GetGFNumber();
        finalChecksum = AsyncChecksum::create(*ci().game);
        const auto replay = RTTRCONFIG.ExpandPath(s25::folders::replays) / GAMECLIENT.GetReplayFilename();
        GAMECLIENT.Stop();
        GAMESERVER.Stop();
        BOOST_TEST_REQUIRE(boost::filesystem::is_regular_file(replay));
        return replay;
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(PadInventoryTests)

BOOST_FIXTURE_TEST_CASE(PhysicalPageBrowsingReadsLiveWholeRealmCountsWithoutEditingStock, InventoryPadFixture)
{
    run([this] {
        MapPoint storehousePos = MapPoint::Invalid();
        RTTR_FOREACH_PT(MapPoint, world().GetSize())
        {
            if(world().GetBQ(pt, 0) >= BuildingQuality::House
               && world().GetNO(pt)->GetType() == NodalObjectType::Nothing)
            {
                storehousePos = pt;
                break;
            }
        }
        BOOST_REQUIRE(storehousePos.isValid());
        auto* storehouse = dynamic_cast<nobBaseWarehouse*>(
          BuildingFactory::CreateBuilding(world(), BuildingType::Storehouse, storehousePos, 0, Nation::Romans));
        BOOST_REQUIRE(storehouse != nullptr);
        storehouse->AddToInventory(GoodCounts::make(GoodType::Wood, 137), true);
        storehouse->AddToInventory(PeopleCounts::make(Job::Carpenter, 29), true);
        BOOST_TEST(world().GetPlayer(0).GetInventory()[GoodType::Wood] == hq().GetInventory()[GoodType::Wood] + 137u);
        BOOST_TEST(world().GetPlayer(0).GetInventory()[Job::Carpenter] == hq().GetInventory()[Job::Carpenter] + 29u);
        enter();
        expectVisibleCounts(warePage);
        expectRows(0);
        const auto previous = world().GetPlayer(0).GetInventory();
        seedStock();
        BOOST_TEST_REQUIRE(world().GetPlayer(0).GetInventory()[GoodType::Wood] > previous[GoodType::Wood]);
        expectVisibleCounts(warePage);
        const auto stock = world().GetPlayer(0).GetInventory();
        const auto checksum = AsyncChecksum::create(*ci().game);
        const unsigned gf = GAMECLIENT.GetGFNumber();
        for(unsigned i = 0; i < 3; ++i)
        {
            nextPage(peoplePage);
            expectVisibleCounts(peoplePage);
            nextPage(warePage);
            expectVisibleCounts(warePage);
        }
        close();
        openFromMain();
        expectVisibleCounts(warePage);
        expectStock(stock);
        BOOST_TEST(GAMECLIENT.GetGFNumber() == gf);
        BOOST_TEST((AsyncChecksum::create(*ci().game) == checksum));
        pumpUntilGF(gf + 40u);
        const auto replay = RTTRCONFIG.ExpandPath(s25::folders::replays) / GAMECLIENT.GetReplayFilename();
        cleanupDesktop();
        GAMECLIENT.Stop();
        GAMESERVER.Stop();
        BOOST_TEST(rttr::test::numGCsForPlayer(replay, 0) == 0u);
    });
}

BOOST_AUTO_TEST_CASE(AllFiveNationsUseTheirShieldIconAndOneCanonicalStockCount)
{
    const std::array<GoodType, 5> shieldTypes{GoodType::ShieldAfricans, GoodType::ShieldJapanese,
                                              GoodType::ShieldRomans, GoodType::ShieldVikings,
                                              GoodType::ShieldJapanese};
    for(const auto nation : helpers::EnumRange<Nation>{})
    {
        InventoryPadFixture fixture;
        fixture.run(
          [&fixture, nation, &shieldTypes] {
              fixture.seedStock();
              fixture.enter();
              const auto id = rttr::enum_cast(GoodType::ShieldRomans);
              const auto* image = fixture.page(warePage).GetCtrl<ctrlImage>(300u + id);
              BOOST_REQUIRE(image != nullptr);
              const auto* expected = LOADER.GetWareTex(shieldTypes[rttr::enum_cast(nation)]);
              BOOST_REQUIRE(expected != nullptr);
              BOOST_TEST(image->GetImage() == expected);
              fixture.expectAmount(warePage, id, fixture.world().GetPlayer(0).GetInventory()[GoodType::ShieldRomans]);
              fixture.expectRows(0);
              fixture.nextPage(peoplePage);
              fixture.expectVisibleCounts(peoplePage);
          },
          nation);
    }
}

BOOST_AUTO_TEST_CASE(AddonCombinationsShowOnlySupportedRowsAndLiveArmoredSoldierDetails)
{
    for(unsigned addons = 0; addons < 8; ++addons)
    {
        InventoryPadFixture fixture;
        fixture.run(
          [&fixture, addons] {
              fixture.seedStock();
              fixture.enter();
              fixture.expectRows(addons);
              fixture.expectVisibleCounts(warePage);
              fixture.nextPage(peoplePage);
              fixture.expectVisibleCounts(peoplePage);
              for(const auto job : SOLDIER_JOBS)
              {
                  const auto* image = fixture.page(peoplePage).GetCtrl<ctrlImage>(100u + rttr::enum_cast(job));
                  BOOST_REQUIRE(image != nullptr);
                  std::string expected = _(JOB_NAMES[job]);
                  if(addons & leather)
                  {
                      const auto& stock = fixture.world().GetPlayer(0).GetInventory();
                      expected += " (" + std::to_string(stock[jobEnumToAmoredSoldierEnum(job)]) + "/"
                                  + std::to_string(stock[job]) + " " + _("with armor)");
                  }
                  BOOST_TEST(image->GetTooltip() == expected);
              }
          },
          Nation::Romans, addons);
    }
}

BOOST_FIXTURE_TEST_CASE(MouseHelpKeyboardBackAndControllerReopenPreserveTheWorld, InventoryPadFixture)
{
    run([this] {
        seedStock();
        const auto stock = world().GetPlayer(0).GetInventory();
        const auto checksum = AsyncChecksum::create(*ci().game);
        enter();
        mouseClick(*window->GetCtrl<ctrlImageButton>(0));
        expectPage(peoplePage);
        expectVisibleCounts(peoplePage);
        mouseClick(*window->GetCtrl<ctrlImageButton>(0));
        expectPage(warePage);
        press(PadButton::Y);
        activate(window->GetCtrl<ctrlImageButton>(12));
        BOOST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
        frame();
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        expectPage(warePage);
        press(PadButton::Y);
        nextPage(peoplePage);
        activate(window->GetCtrl<ctrlImageButton>(12));
        BOOST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        press(PadButton::B);
        BOOST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        expectPage(peoplePage);
        press(PadButton::Y);
        close();
        openFromMain();
        expectVisibleCounts(warePage);
        expectStock(stock);
        BOOST_TEST((AsyncChecksum::create(*ci().game) == checksum));
    });
}

BOOST_FIXTURE_TEST_CASE(SavedStockReplaysToExactFinalFrameWhilePhysicalBrowsingRemainsReadonly, InventoryPadFixture)
{
    run(
      [this] {
          seedStock();
          const auto replay = recordSavedStock();
          BOOST_TEST_REQUIRE(rttr::test::numGCsForPlayer(replay, 0) == 1u);
          const auto stock = world().GetPlayer(0).GetInventory();
          GAMECLIENT.SetInterface(&ci());
          BOOST_TEST_REQUIRE(GAMECLIENT.StartReplay(replay));
          GAMECLIENT.GameLoaded();
          makeDesktop();
          expectStock(stock);
          enter();
          expectRows(wine | leather | charburner);
          expectVisibleCounts(warePage);
          nextPage(peoplePage);
          expectVisibleCounts(peoplePage);
          nextPage(warePage);
          GAMECLIENT.SetPause(false);
          GAMECLIENT.skiptogf = GAMECLIENT.GetLastReplayGF();
          pumpUntil([this] { return ci().replayEnded || ci().numErrors || ci().numReplayAsync; }, "stock replay end");
          frame();
          BOOST_TEST(ci().replayEnded);
          BOOST_TEST(GAMECLIENT.GetGFNumber() == finalGF);
          BOOST_TEST((AsyncChecksum::create(*ci().game) == finalChecksum));
          expectStock(stock);
          expectVisibleCounts(warePage);
          nextPage(peoplePage);
          expectVisibleCounts(peoplePage);
          close();
      },
      Nation::Vikings, wine | leather | charburner);
}

BOOST_AUTO_TEST_CASE(ExceptionalCleanupRestoresSettingsBeforeFixtureDestruction)
{
    {
        InventoryPadFixture fixture;
        bool reachedProbe = false;
        BOOST_CHECK_THROW(fixture.run([&fixture, &reachedProbe] {
            fixture.enter();
            fixture.nextPage(peoplePage);
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
