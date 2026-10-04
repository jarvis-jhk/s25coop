// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AsyncChecksum.h"
#include "PadGameFixture.h"
#include "buildings/noBaseBuilding.h"
#include "buildings/nobBaseWarehouse.h"
#include "controls/ctrlImage.h"
#include "controls/ctrlImageButton.h"
#include "controls/ctrlTextButton.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "factories/BuildingFactory.h"
#include "helpers/EnumRange.h"
#include "ingameWindows/iwBaseWarehouse.h"
#include "ingameWindows/iwBuilding.h"
#include "ingameWindows/iwBuildings.h"
#include "ingameWindows/iwHelp.h"
#include "ingameWindows/iwMainMenu.h"
#include "ingameWindows/iwMilitaryBuilding.h"
#include "ingameWindows/iwPadSystemMenu.h"
#include "ingameWindows/iwTempleBuilding.h"
#include "world/GameWorldView.h"
#include "gameData/BuildingConsts.h"
#include "gameData/const_gui_ids.h"
#include <boost/test/unit_test.hpp>
#include <array>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
constexpr PadDeviceId pad = 114;
constexpr unsigned wine = 1;
constexpr unsigned leather = 2;
constexpr unsigned charburner = 4;

[[noreturn]] void throwCleanupProbe()
{
    throw std::runtime_error("buildings cleanup probe");
}

struct BuildingsPadFixture : uiHelper::Fixture, rttr::test::LocalGameFixture
{
    rttr::test::PadFeeder pads{*uiHelper::GetVideoDriver()};
    rttr::test::TestableGameInterface* dsk = nullptr;
    iwBuildings* window = nullptr;
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
            if(GAMECLIENT.GetState() == ClientState::Game && !GAMECLIENT.IsReplayModeOn())
                expectNoCommands();
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
          "buildings nation and dummy AI slots");
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
        LOADER.LoadDummyBuildingFiles();
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
                for(unsigned i = 0; i < 50 && focused() != target; ++i)
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
        activate(main->GetCtrl<ctrlImageButton>(5));
        window = dynamic_cast<iwBuildings*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_REQUIRE(window != nullptr);
        press(PadButton::Y);
        BOOST_TEST_REQUIRE(dsk->GetPlayerView(0).GetFocus().GetRoot() == window);
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
    void closeTop()
    {
        auto* top = WINDOWMANAGER.GetTopMostWindow();
        BOOST_REQUIRE(top != nullptr);
        BOOST_REQUIRE(dsk->GetPlayerView(0).GetFocus().GetRoot() != nullptr);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == top);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() != top);
    }
    ctrlImageButton& button(const BuildingType type) const
    {
        for(auto* control : window->GetCtrls<ctrlImageButton>())
            if(control->GetTooltip() == _(BUILDING_NAMES[type]))
                return *control;
        BOOST_FAIL("Building button missing: " << rttr::enum_cast(type)); // LCOV_EXCL_LINE
        throw std::logic_error("missing building button");                // LCOV_EXCL_LINE
    }
    MapPoint seed(const BuildingType type, const bool fromEnd = false)
    {
        auto& gameWorld = world();
        MapPoint pos = MapPoint::Invalid();
        RTTR_FOREACH_PT(MapPoint, gameWorld.GetSize())
        {
            bool clear = gameWorld.GetNode(pt).obj == nullptr;
            for(const auto direction : helpers::EnumRange<Direction>{})
                clear = clear && gameWorld.GetNode(gameWorld.GetNeighbour(pt, direction)).obj == nullptr;
            // Direct factory seeding needs room for the flag and castle extensions too.
            if(clear && gameWorld.GetBQ(pt, 0) == BuildingQuality::Castle)
            {
                pos = pt;
                if(!fromEnd)
                    break;
            }
        }
        BOOST_REQUIRE(pos.isValid());
        auto* building = BuildingFactory::CreateBuilding(world(), type, pos, 0, world().GetPlayer(0).nation);
        BOOST_REQUIRE(building != nullptr);
        BOOST_TEST(building->GetPlayer() == 0u);
        BOOST_TEST((building->GetBuildingType() == type));
        return pos;
    }
    void expectCentered(const MapPoint target)
    {
        const auto& view = dsk->GetPlayerView(0).GetView();
        const Position observed = view.ViewPosToMap(view.GetPos() + view.GetSize() / 2u) + view.GetOffset();
        const Position expected = world().GetNodePos(target);
        const Position mapPixels(world().GetWidth() * TR_W, world().GetHeight() * TR_H);
        BOOST_TEST((observed.x - expected.x) % mapPixels.x == 0);
        BOOST_TEST((observed.y - expected.y) % mapPixels.y == 0);
    }
    template<class Child>
    void select(const BuildingType type, const MapPoint expected)
    {
        activate(&button(type));
        auto* child = dynamic_cast<Child*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_REQUIRE(child != nullptr);
        // The GUI identity includes the map location of the actual selected building.
        BOOST_TEST(child->GetID() == CGI_BUILDING + MapBase::CreateGUIID(expected));
        BOOST_TEST(child->GetTitle() == _(BUILDING_NAMES[type]));
        expectCentered(expected);
        BOOST_TEST(WINDOWMANAGER.FindNonModalWindow(CGI_BUILDINGS, window->GetOwner()) == window);
        closeTop();
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        press(PadButton::Y);
    }
    void expectUnchanged(const AsyncChecksum& checksum, const unsigned gf, const Inventory& stock)
    {
        BOOST_TEST((AsyncChecksum::create(*ci().game) == checksum));
        BOOST_TEST(GAMECLIENT.GetGFNumber() == gf);
        for(const auto good : helpers::EnumRange<GoodType>{})
            BOOST_TEST(world().GetPlayer(0).GetInventory()[good] == stock[good]);
        for(const auto job : helpers::EnumRange<Job>{})
            BOOST_TEST(world().GetPlayer(0).GetInventory()[job] == stock[job]);
        BOOST_TEST(ci().numErrors == 0u);
        BOOST_TEST(ci().numAsync == 0u);
        BOOST_TEST(ci().numReplayAsync == 0u);
    }
    void expectNoCommands()
    {
        pumpUntilGF(GAMECLIENT.GetGFNumber() + 40u);
        const auto replay = RTTRCONFIG.ExpandPath(s25::folders::replays) / GAMECLIENT.GetReplayFilename();
        cleanupDesktop();
        GAMECLIENT.Stop();
        GAMESERVER.Stop();
        BOOST_TEST(rttr::test::numGCsForPlayer(replay, 0) == 0u);
    }
    boost::filesystem::path recordSavedBuildings()
    {
        const auto save = RTTRCONFIG.ExpandPath(s25::folders::save) / "buildings.sav";
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
            if(GAMECLIENT.HostGame(CreateServerInfo(ServerType::Local, port, "Saved buildings"),
                                   MapDescription(save, MapType::Savegame)))
                inLobby =
                  pumpWhile([] { return GAMECLIENT.GetState() == ClientState::Config; }, std::chrono::seconds(10));
        }
        BOOST_TEST_REQUIRE(inLobby);
        ci().numErrors = 0;
        GameLobbyController controller(GAMECLIENT.GetGameLobby(), GAMECLIENT.GetMainPlayer());
        GAMECLIENT.Command_SetReady(true);
        controller.StartCountdown(0);
        pumpUntil([] { return GAMECLIENT.GetState() == ClientState::Loading; }, "saved buildings loading");
        GAMECLIENT.GameLoaded();
        pumpUntil([] { return GAMECLIENT.GetState() == ClientState::Game; }, "saved buildings game");
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
          "recorded buildings command roundtrip");
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

BOOST_AUTO_TEST_SUITE(PadBuildingsTests)

BOOST_FIXTURE_TEST_CASE(PhysicalSelectionUsesFirstMatchingBuildingAndCorrectChildType, BuildingsPadFixture)
{
    run([this] {
        const auto guardhouse = seed(BuildingType::Guardhouse);
        const auto barracks = seed(BuildingType::Barracks);
        seed(BuildingType::Barracks);
        const auto warehouse = seed(BuildingType::Storehouse);
        seed(BuildingType::Storehouse);
        const auto sawmill = seed(BuildingType::Sawmill);
        const auto woodcutter = seed(BuildingType::Woodcutter, true);
        const auto laterWoodcutter = seed(BuildingType::Woodcutter);
        BOOST_TEST_REQUIRE(MapBase::CreateGUIID(woodcutter) > MapBase::CreateGUIID(laterWoodcutter));
        const auto stock = world().GetPlayer(0).GetInventory();
        const auto checksum = AsyncChecksum::create(*ci().game);
        const unsigned gf = GAMECLIENT.GetGFNumber();
        enter();
        select<iwBuilding>(BuildingType::Woodcutter, woodcutter);
        select<iwBuilding>(BuildingType::Sawmill, sawmill);
        // Military and warehouse registries contain other types before the requested one.
        select<iwMilitaryBuilding>(BuildingType::Barracks, barracks);
        select<iwMilitaryBuilding>(BuildingType::Guardhouse, guardhouse);
        select<iwBaseWarehouse>(BuildingType::Storehouse, warehouse);
        expectUnchanged(checksum, gf, stock);
    });
}

BOOST_FIXTURE_TEST_CASE(EmptyCategoriesHelpMouseAndKeyboardReturnPreserveWorldAndCamera, BuildingsPadFixture)
{
    run([this] {
        enter();
        const auto stock = world().GetPlayer(0).GetInventory();
        const auto checksum = AsyncChecksum::create(*ci().game);
        const unsigned gf = GAMECLIENT.GetGFNumber();
        const auto offset = dsk->GetPlayerView(0).GetView().GetOffset();
        for(const auto type :
            {BuildingType::Woodcutter, BuildingType::Barracks, BuildingType::Storehouse, BuildingType::HarborBuilding})
        {
            activate(&button(type));
            BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
            BOOST_TEST((dsk->GetPlayerView(0).GetView().GetOffset() == offset));
        }
        activate(window->GetCtrl<ctrlImageButton>(0));
        BOOST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        closeTop();
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        press(PadButton::Y);
        mouseClick(button(BuildingType::Woodcutter));
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        mouseClick(*window->GetCtrl<ctrlImageButton>(0));
        BOOST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
        frame();
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
        frame();
        BOOST_REQUIRE(dynamic_cast<iwMainMenu*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        openFromMain();
        BOOST_TEST((dsk->GetPlayerView(0).GetView().GetOffset() == offset));
        expectUnchanged(checksum, gf, stock);
    });
}

BOOST_AUTO_TEST_CASE(AllNationsAndAddonCombinationsKeepCorrectIconsAndReachableRows)
{
    for(const auto nation : helpers::EnumRange<Nation>{})
        for(unsigned addons = 0; addons < 8; ++addons)
        {
            BuildingsPadFixture fixture;
            fixture.run(
              [&fixture, nation, addons] {
                  fixture.enter();
                  const auto buttons = fixture.window->GetCtrls<ctrlImageButton>();
                  BOOST_TEST(buttons.size()
                             == 32u + ((addons & wine) ? 3u : 0u) + ((addons & leather) ? 3u : 0u)
                                  + ((addons & charburner) ? 1u : 0u));
                  for(const auto type : {BuildingType::Barracks, BuildingType::Woodcutter, BuildingType::Storehouse})
                  {
                      const auto& control = fixture.button(type);
                      const auto* expected = LOADER.GetNationIcon(nation, type);
                      BOOST_REQUIRE(expected != nullptr);
                      BOOST_TEST(control.GetImage() == expected);
                      LOADER.LoadDummyBuildingFiles();
                      BOOST_TEST(LOADER.GetNationIcon(nation, type) == expected);
                      fixture.focusUntil(&control);
                  }
                  fixture.focusUntil(&fixture.button(BuildingType::Barracks));
                  fixture.press(PadButton::DpadRight);
                  BOOST_TEST_REQUIRE(fixture.focused() == &fixture.button(BuildingType::Guardhouse));
                  fixture.press(PadButton::DpadLeft);
                  BOOST_TEST_REQUIRE(fixture.focused() == &fixture.button(BuildingType::Barracks));
                  fixture.press(PadButton::DpadDown);
                  BOOST_TEST_REQUIRE(fixture.focused() == &fixture.button(BuildingType::GraniteMine));
                  fixture.press(PadButton::DpadUp);
                  BOOST_TEST_REQUIRE(fixture.focused() == &fixture.button(BuildingType::Barracks));
                  if(addons & wine)
                      fixture.focusUntil(&fixture.button(BuildingType::Temple));
                  if(addons & leather)
                      fixture.focusUntil(&fixture.button(BuildingType::LeatherWorks));
                  if(addons & charburner)
                      fixture.focusUntil(&fixture.button(BuildingType::Charburner));
                  fixture.closeTop();
                  fixture.openFromMain();
              },
              nation, addons);
        }
}

BOOST_FIXTURE_TEST_CASE(TempleHasSpecializedChildAndSelectionRefreshesAfterFirstBuildingDisappears, BuildingsPadFixture)
{
    run(
      [this] {
          const auto first = seed(BuildingType::Temple);
          const auto second = seed(BuildingType::Temple);
          enter();
          select<iwTempleBuilding>(BuildingType::Temple, first);
          world().DestroyNO(first);
          frame();
          const auto stock = world().GetPlayer(0).GetInventory();
          const auto checksum = AsyncChecksum::create(*ci().game);
          const unsigned gf = GAMECLIENT.GetGFNumber();
          select<iwTempleBuilding>(BuildingType::Temple, second);
          expectUnchanged(checksum, gf, stock);
          world().DestroyNO(second);
          frame();
          const auto offset = dsk->GetPlayerView(0).GetView().GetOffset();
          activate(&button(BuildingType::Temple));
          BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
          BOOST_TEST((dsk->GetPlayerView(0).GetView().GetOffset() == offset));
      },
      Nation::Babylonians, wine);
}

BOOST_FIXTURE_TEST_CASE(SavedBuildingsReplayToExactFinalFrameAndBrowseTheActualRegistry, BuildingsPadFixture)
{
    run([this] {
        const auto target = seed(BuildingType::Woodcutter);
        seed(BuildingType::Woodcutter);
        const auto replay = recordSavedBuildings();
        BOOST_TEST_REQUIRE(rttr::test::numGCsForPlayer(replay, 0) == 1u);
        GAMECLIENT.SetInterface(&ci());
        BOOST_TEST_REQUIRE(GAMECLIENT.StartReplay(replay));
        GAMECLIENT.GameLoaded();
        makeDesktop();
        enter();
        select<iwBuilding>(BuildingType::Woodcutter, target);
        GAMECLIENT.SetPause(false);
        GAMECLIENT.skiptogf = GAMECLIENT.GetLastReplayGF();
        pumpUntil([this] { return ci().replayEnded || ci().numErrors || ci().numReplayAsync; }, "buildings replay end");
        frame();
        BOOST_TEST(ci().replayEnded);
        BOOST_TEST(GAMECLIENT.GetGFNumber() == finalGF);
        BOOST_TEST((AsyncChecksum::create(*ci().game) == finalChecksum));
        const auto stock = world().GetPlayer(0).GetInventory();
        select<iwBuilding>(BuildingType::Woodcutter, target);
        expectUnchanged(finalChecksum, finalGF, stock);
    });
}

BOOST_AUTO_TEST_CASE(ExceptionalCleanupRestoresSettingsBeforeFixtureDestruction)
{
    {
        BuildingsPadFixture fixture;
        bool reachedProbe = false;
        BOOST_CHECK_THROW(fixture.run([&fixture, &reachedProbe] {
            fixture.enter();
            fixture.activate(fixture.window->GetCtrl<ctrlImageButton>(0));
            BOOST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
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
