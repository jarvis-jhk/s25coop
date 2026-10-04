// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GameLobby.h"
#include "ILobbyClient.hpp"
#include "JoinPlayerInfo.h"
#include "LocalGameFixture.h"
#include "MenuPadFixture.h"
#include "controls/ctrlCheck.h"
#include "controls/ctrlGroup.h"
#include "controls/ctrlLobbyPlayerCard.h"
#include "desktops/dskGameLobby.h"
#include "ingameWindows/iwAddons.h"
#include "ingameWindows/iwMsgbox.h"
#include "lua/GameDataLoader.h"
#include "mapGenerator/RandomMap.h"
#include "network/GameClient.h"
#include "network/GameServer.h"
#include "world/ViewportLayout.h"
#include "gameData/MaxPlayers.h"
#include "gameData/WorldDescription.h"
#include "libsiedler2/libsiedler2.h"
#include "rttr/test/TmpFolder.hpp"
#include "s25util/colors.h"
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

namespace {
using Row = LobbyPlayerCardModel::Row;
constexpr unsigned firstPlayerGroup = 48;
constexpr unsigned firstCard = firstPlayerGroup + (MAX_PLAYERS * 2);

struct CardFixture : rttr::test::LocalGameFixture, rttr::test::MenuPadFixture
{
    void advanceNetwork()
    {
        video.tickCount_ += frameMs;
        GAMECLIENT.Run();
        GAMESERVER.Run();
    }

    void frame() override
    {
        advanceNetwork();
        WINDOWMANAGER.Draw();
    }

    void enter()
    {
        hostAndEnterLobby();
        showLobby();
    }

    rttr::test::TmpFolder files;

    void hostMap(const MapDescription& map)
    {
        bool connected = false;
        for(unsigned attempt = 0; attempt < 10 && !connected; ++attempt)
        {
            GAMECLIENT.Stop();
            GAMESERVER.Stop();
            GAMECLIENT.SetInterface(&ci());
            const auto port = static_cast<uint16_t>(rttr::test::randomValue(1024, 49151));
            if(GAMECLIENT.HostGame(CreateServerInfo(ServerType::Local, port, "CardTest"), map))
                connected =
                  pumpWhile([] { return GAMECLIENT.GetState() == ClientState::Config; }, std::chrono::seconds(10));
        }
        BOOST_TEST_REQUIRE(connected);
    }

    void showLobby()
    {
        WINDOWMANAGER.GetPadInput().Reset();
        video.padEvents_.clear();
        WINDOWMANAGER.Switch(std::make_unique<dskGameLobby>(ServerType::Local, GAMECLIENT.GetGameLobby(),
                                                            GAMECLIENT.GetPlayerId(), nullptr));
        frame();
        BOOST_TEST_REQUIRE(desktopAs<dskGameLobby>());
        pickUp(10);
        BOOST_TEST_REQUIRE(focused(0) == &card(0));
    }

    static ctrlLobbyPlayerCard& card(unsigned seat)
    {
        auto* desk = desktopAs<dskGameLobby>();
        BOOST_TEST_REQUIRE(desk);
        auto* ctrl = desk->GetCtrl<ctrlLobbyPlayerCard>(firstCard + seat);
        BOOST_TEST_REQUIRE(ctrl);
        return *ctrl;
    }

    static JoinPlayerInfo& player(unsigned id) { return GAMECLIENT.GetGameLobby()->getPlayer(id); }

    void join(PadDeviceId device)
    {
        pickUp(device);
        press(device, PadButton::A);
        settle();
    }

    void settle()
    {
        for(unsigned i = 0; i < 80; ++i)
            frame();
    }

    template<class Predicate>
    void await(const Predicate& done)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while(!done() && std::chrono::steady_clock::now() < deadline)
        {
            frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        BOOST_TEST_REQUIRE(done());
    }

    template<class Predicate>
    void awaitWithoutPaint(const Predicate& done)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while(!done() && std::chrono::steady_clock::now() < deadline)
        {
            // Client sending is paced by the mock driver's clock, even when testing before paint.
            advanceNetwork();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        BOOST_TEST_REQUIRE(done());
    }

    void select(PadDeviceId device, unsigned seat, Row row)
    {
        pressN(device, PadButton::DpadUp, 4);
        pressN(device, PadButton::DpadDown, static_cast<unsigned>(row));
        BOOST_TEST_REQUIRE((card(seat).GetModel().GetRow() == row));
    }

    void mouse(Window& ctrl)
    {
        MouseCoords mc(ctrl.GetDrawPos() + Position(ctrl.GetSize() / 2u));
        WINDOWMANAGER.Msg_MouseMove(mc);
        mc.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mc);
        mc.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mc);
        settle();
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(MenuPadLobbyCardTests)

BOOST_FIXTURE_TEST_CASE(EachControllerKeepsItsOwnCardRowAndAIsNotStandUp, CardFixture)
{
    // The standard LocalGameFixture map has only three slots. Use a valid four-player map for all four pads.
    WorldDescription descriptions;
    loadGameData(descriptions);
    rttr::mapGenerator::RandomUtility random(0);
    rttr::mapGenerator::MapSettings settings;
    settings.numPlayers = 4;
    settings.size = MapExtent::all(64);
    settings.style = rttr::mapGenerator::MapStyle::Land;
    const auto generated = rttr::mapGenerator::GenerateRandomMap(random, descriptions, settings);
    const auto path = files / "FourPlayers.SWD";
    BOOST_TEST_REQUIRE(libsiedler2::Write(path, generated.CreateArchiv()) == 0);
    hostMap(MapDescription(path, MapType::OldMap));
    BOOST_TEST_REQUIRE(GAMECLIENT.GetGameLobby()->getNumPlayers() == 4u);
    showLobby();
    join(11);
    join(12);
    join(13);
    BOOST_TEST_REQUIRE(GAMECLIENT.GetAdditionalLocalPlayers().size() == 3u);
    select(11, 1, Row::Team);
    select(12, 2, Row::Nation);
    select(13, 3, Row::SharedTribe);
    press(13, PadButton::DpadRight);
    BOOST_TEST(!card(3).GetModel().GetSnapshot().values.sharedTribe);
    BOOST_TEST((card(0).GetModel().GetRow() == Row::Color));
    BOOST_TEST((card(1).GetModel().GetRow() == Row::Team));
    BOOST_TEST((card(2).GetModel().GetRow() == Row::Nation));
    press(11, PadButton::A);
    pressN(11, PadButton::RightShoulder, 20);
    pressN(11, PadButton::LeftShoulder, 20);
    BOOST_TEST(focused(1) == &card(1));
    BOOST_TEST(GAMECLIENT.GetAdditionalLocalPlayers().size() == 3u);
    const Nation remainingNation = player(2).nation;
    tap(11, PadButton::B);
    tap(11, PadButton::DpadRight); // Residual slot-1 edge must not edit the newly compacted controller's card.
    frame();
    settle();
    BOOST_TEST((player(2).nation == remainingNation));
    BOOST_TEST(GAMECLIENT.GetAdditionalLocalPlayers().size() == 2u);
    BOOST_TEST((player(1).aiInfo.type == AI::Type::Default));
    BOOST_TEST(focused(1) == &card(2));
    BOOST_TEST(focused(2) == &card(3));
    // The remaining controllers follow their seats even after the router compacts the view slots.
    press(12, PadButton::DpadDown);
    BOOST_TEST(focused(1) == &card(2));
    BOOST_TEST((card(2).GetModel().GetRow() == Row::Team));
    press(13, PadButton::DpadUp);
    BOOST_TEST(focused(2) == &card(3));
    BOOST_TEST((card(3).GetModel().GetRow() == Row::Team));
}

BOOST_FIXTURE_TEST_CASE(GuestNationAndTeamTakeTheRealServerRoundtrip, CardFixture)
{
    enter();
    join(11);
    const Nation hostNation = player(0).nation;
    const Team hostTeam = player(0).team;
    select(11, 1, Row::Nation);
    const Nation before = Nation::Romans;
    lobby().SetNation(1, Nation::Romans);
    await([] { return player(1).nation == Nation::Romans; });
    tap(11, PadButton::DpadRight);
    // Only the production video/input pump runs: no server or client response yet.
    WINDOWMANAGER.Draw();
    BOOST_TEST((player(1).nation == before));
    await([] { return player(1).nation == Nation::Vikings; });
    BOOST_TEST((card(1).GetModel().GetSnapshot().values.nation == Nation::Vikings));
    select(11, 1, Row::Team);
    lobby().SetTeam(1, Team::None);
    await([] { return player(1).team == Team::None; });
    press(11, PadButton::DpadLeft);
    await([] { return player(1).team == Team::Random1To4; });
    BOOST_TEST((player(0).nation == hostNation));
    BOOST_TEST((player(0).team == hostTeam));
    // An unrelated host broadcast refreshes the snapshot without moving the guest's row.
    const unsigned otherColor = PLAYER_COLORS.back();
    lobby().SetColor(0, otherColor);
    await([&] { return player(0).color == otherColor; });
    BOOST_TEST((card(1).GetModel().GetRow() == Row::Team));
    BOOST_TEST((player(1).nation == Nation::Vikings));
}

BOOST_FIXTURE_TEST_CASE(ColorSkipsOccupiedSlotsInBothDirections, CardFixture)
{
    enter();
    join(11);
    const unsigned oldColor = player(1).color;
    // The initial slots use contiguous palette entries, leaving this known next colour free.
    const unsigned count = GAMECLIENT.GetGameLobby()->getNumPlayers();
    BOOST_TEST_REQUIRE(count < PLAYER_COLORS.size());
    const unsigned expected = PLAYER_COLORS[count];
    press(11, PadButton::DpadRight);
    await([&] { return player(1).color == expected; });
    for(unsigned id = 0; id < GAMECLIENT.GetGameLobby()->getNumPlayers(); ++id)
        if(id != 1 && player(id).isUsed())
            BOOST_TEST(player(1).color != player(id).color);
    press(11, PadButton::DpadLeft);
    await([&] { return player(1).color == oldColor; });
}

BOOST_FIXTURE_TEST_CASE(HostCanEditItsCardAndStillUseMousePlayerControls, CardFixture)
{
    enter();
    select(10, 0, Row::Nation);
    lobby().SetNation(0, Nation::Romans);
    await([] { return player(0).nation == Nation::Romans; });
    press(10, PadButton::DpadLeft);
    await([] { return player(0).nation == Nation::Babylonians; });
    auto* group = desktopAs<dskGameLobby>()->GetCtrl<ctrlGroup>(firstPlayerGroup);
    BOOST_TEST_REQUIRE(group);
    auto* nationButton = group->GetCtrl<Window>(28);
    BOOST_TEST_REQUIRE(nationButton);
    mouse(*nationButton);
    BOOST_TEST((player(0).nation != Nation::Babylonians));
    BOOST_TEST((card(0).GetModel().GetSnapshot().values.nation == player(0).nation));
    BOOST_TEST((card(0).GetModel().GetRow() == Row::Nation));
    press(10, PadButton::LeftShoulder);
    BOOST_TEST(focused(0) != &card(0));
    BOOST_TEST_REQUIRE(focused(0));
}

BOOST_FIXTURE_TEST_CASE(SharedGuestsCannotChangeTheHostsTribeOrSeatMode, CardFixture)
{
    enter();
    select(10, 0, Row::SharedTribe);
    press(10, PadButton::DpadRight);
    BOOST_TEST_REQUIRE(GAMECLIENT.GetSharedLocalViews() == 0u);
    join(11);
    BOOST_TEST_REQUIRE(GAMECLIENT.GetSharedLocalViews() == 1u);
    const auto before = card(0).GetModel().GetSnapshot().values;
    for(unsigned row = 0; row < 4; ++row)
    {
        select(11, 1, static_cast<Row>(row));
        BOOST_TEST(!card(1).GetModel().GetLockReason().empty());
        press(11, PadButton::DpadRight);
        press(11, PadButton::DpadLeft);
    }
    settle();
    BOOST_TEST(player(0).color == before.color);
    BOOST_TEST((player(0).nation == before.nation));
    BOOST_TEST((player(0).team == before.team));
    BOOST_TEST(GAMECLIENT.GetSharedLocalViews() == 1u);
    select(10, 0, Row::SharedTribe);
    press(10, PadButton::DpadLeft);
    BOOST_TEST(GAMECLIENT.GetSharedLocalViews() == 0u);
    BOOST_TEST(!card(0).GetModel().GetSnapshot().values.sharedTribe);
    auto* check = desktopAs<dskGameLobby>()->GetCtrl<ctrlCheck>(firstCard + MAX_VIEWPORTS + 7);
    BOOST_TEST_REQUIRE(check);
    BOOST_TEST(!check->isChecked());
}

BOOST_FIXTURE_TEST_CASE(CampaignModeStaysTogetherAndShowsItsReason, CardFixture)
{
    hostAndEnterLobby();
    GAMECLIENT.SetHostingCampaign(true);
    showLobby();
    select(10, 0, Row::SharedTribe);
    BOOST_TEST(!card(0).GetModel().GetLockReason().empty());
    press(10, PadButton::DpadLeft);
    BOOST_TEST(card(0).GetModel().GetSnapshot().values.sharedTribe);
    join(11);
    BOOST_TEST(GAMECLIENT.GetSharedLocalViews() == 1u);
}

BOOST_FIXTURE_TEST_CASE(DisconnectedAndHostClosedSeatsDoNotKeepEditing, CardFixture)
{
    enter();
    join(11);
    select(11, 1, Row::Nation);
    const Nation before = player(1).nation;
    lobby().CloseSlot(1);
    await([] { return player(1).ps == PlayerState::Locked; });
    BOOST_TEST_REQUIRE(GAMECLIENT.GetAdditionalLocalPlayers().empty());
    press(11, PadButton::DpadRight);
    press(11, PadButton::A);
    settle();
    BOOST_TEST((player(1).nation == before));
    BOOST_TEST((player(1).ps == PlayerState::Locked));
    disconnect(11);
    frame();
    BOOST_TEST(!card(1).IsJoined());
    disconnect(10);
    frame();
    BOOST_TEST(!card(0).IsVisible());
    BOOST_TEST(!card(1).IsVisible());
}

BOOST_FIXTURE_TEST_CASE(CardsStayInsideLobbyAndMouseCardsCannotJoin, CardFixture)
{
    enter();
    pickUp(11);
    for(unsigned seat = 0; seat < router().GetNumSlots(); ++seat)
    {
        const auto& c = card(seat);
        const auto pos = c.GetDrawPos();
        const auto size = c.GetSize();
        BOOST_TEST(pos.x >= 0);
        BOOST_TEST(pos.y >= 0);
        BOOST_TEST(pos.x + static_cast<int>(size.x) <= static_cast<int>(desktop()->GetSize().x));
        BOOST_TEST(pos.y + static_cast<int>(size.y) <= static_cast<int>(desktop()->GetSize().y));
        mouse(card(seat));
    }
    BOOST_TEST(GAMECLIENT.GetAdditionalLocalPlayers().empty());
    press(11, PadButton::A);
    settle();
    BOOST_TEST(GAMECLIENT.GetAdditionalLocalPlayers().size() == 1u);
}

BOOST_FIXTURE_TEST_CASE(MapScriptLocksAreRevalidatedBeforePhysicalInput, CardFixture)
{
    const auto path = files / "Locked.SWD";
    boost::filesystem::copy_file(rttr::test::rttrBaseDir / "tests" / "testData" / "maps" / "LuaFunctions.SWD", path);
    {
        std::ofstream script((files / "Locked.lua").string());
        script << "function getRequiredLuaVersion() return 1 end\n"
                  "function getAllowedChanges() return {ownNation=false, aiTeam=false} end\n";
    }
    hostMap(MapDescription(path, MapType::OldMap));
    BOOST_TEST_REQUIRE(!GAMECLIENT.GetLuaFilePath().empty());
    showLobby();
    join(11);
    select(10, 0, Row::Nation);
    select(11, 1, Row::Team);
    BOOST_TEST(!card(0).GetModel().GetLockReason().empty());
    BOOST_TEST(!card(1).GetModel().GetLockReason().empty());
    const Nation nation = player(0).nation;
    const Team team = player(1).team;
    press(10, PadButton::DpadRight);
    press(11, PadButton::DpadRight);
    settle();
    BOOST_TEST((player(0).nation == nation));
    BOOST_TEST((player(1).team == team));
    // Unlocked rows remain editable on the same scripted map.
    select(11, 1, Row::Nation);
    const auto proposed = card(1).GetModel().ProposeValue(true);
    BOOST_TEST_REQUIRE(proposed.has_value());
    press(11, PadButton::DpadRight);
    await([&] { return player(1).nation == proposed->nation; });
}

BOOST_FIXTURE_TEST_CASE(SavedGameCardsLockSimulationSettingsButStillLetPadsJoin, CardFixture)
{
    enter();
    join(11);
    GAMECLIENT.SetInterface(&ci());
    startGame();
    const auto save = files / "Cards.sav";
    BOOST_TEST_REQUIRE(GAMECLIENT.SaveToFile(save));
    hostMap(MapDescription(save, MapType::Savegame));
    BOOST_TEST_REQUIRE(GAMECLIENT.GetGameLobby()->isSavegame());
    showLobby();
    join(11);
    BOOST_TEST_REQUIRE(GAMECLIENT.GetAdditionalLocalPlayers().size() == 1u);
    const auto before = card(1).GetModel().GetSnapshot().values;
    for(unsigned row = 0; row < 3; ++row)
    {
        select(11, 1, static_cast<Row>(row));
        BOOST_TEST(!card(1).GetModel().GetLockReason().empty());
        press(11, PadButton::DpadRight);
        press(11, PadButton::DpadLeft);
    }
    settle();
    BOOST_TEST(player(1).color == before.color);
    BOOST_TEST((player(1).nation == before.nation));
    BOOST_TEST((player(1).team == before.team));
}

BOOST_FIXTURE_TEST_CASE(GuestCannotOperateHostWindowsAndStartCannotBypassThem, CardFixture)
{
    enter();
    join(11);
    const auto hostRow = card(0).GetModel().GetRow();
    const auto guestRow = card(1).GetModel().GetRow();
    tap(10, PadButton::B);
    tap(10, PadButton::Start); // Both edges in one driver batch must respect the newly opened modal.
    frame();
    frame(); // Root/focus transition completes on the next pump.
    auto* question = dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow());
    BOOST_TEST_REQUIRE(question);
    const Window* guestFocus = focused(1);
    press(11, PadButton::LeftShoulder);
    press(11, PadButton::A);
    press(11, PadButton::B);
    press(11, PadButton::Start);
    press(10, PadButton::Start);
    settle();
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == question);
    BOOST_TEST(focused(1) == guestFocus);
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Config));
    press(10, PadButton::A); // The host's harmless No response.
    settle();
    BOOST_TEST_REQUIRE(!WINDOWMANAGER.GetTopMostWindow());
    BOOST_TEST((card(0).GetModel().GetRow() == hostRow));
    BOOST_TEST((card(1).GetModel().GetRow() == guestRow));
    // Reach the real host Addons button through physical shoulder navigation.
    for(unsigned i = 0; i < 100 && focusedId(0) != 0; ++i)
        press(10, PadButton::LeftShoulder);
    BOOST_TEST_REQUIRE(focusedId(0) == 0u);
    for(unsigned i = 0; i < 20 && focusedId(0) != 6; ++i)
        press(10, PadButton::RightShoulder);
    BOOST_TEST_REQUIRE(focusedId(0) == 6u);
    press(10, PadButton::A);
    frame();
    auto* addons = dynamic_cast<iwAddons*>(WINDOWMANAGER.GetTopMostWindow());
    BOOST_TEST_REQUIRE(addons);
    guestFocus = focused(1);
    pressN(11, PadButton::RightShoulder, 20);
    press(11, PadButton::DpadRight);
    press(11, PadButton::A);
    press(11, PadButton::B);
    press(10, PadButton::Start);
    settle();
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == addons);
    BOOST_TEST(focused(1) == guestFocus);
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Config));
    auto* abort = addons->GetCtrl<Window>(2);
    BOOST_TEST_REQUIRE(abort);
    mouse(*abort);
    BOOST_TEST(!WINDOWMANAGER.GetTopMostWindow());
}

BOOST_FIXTURE_TEST_CASE(LiveAuthorityIsCheckedBeforeTheNextPaintAndSticksCannotEscape, CardFixture)
{
    enter();
    join(11);
    select(11, 1, Row::Nation);
    const Nation before = player(1).nation;
    video.padEvents_.push_back(PadEvent::Axis(11, PadAxis::LeftX, 1.f));
    for(unsigned i = 0; i < 100; ++i)
        frame();
    video.padEvents_.push_back(PadEvent::Axis(11, PadAxis::LeftX, 0.f));
    frame();
    BOOST_TEST(focused(1) == &card(1));
    BOOST_TEST((player(1).nation == before));
    lobby().SetPlayerState(1, PlayerState::Free, AI::Info());
    // Local servers normalize Free to the ordinary AI; that also revokes a Dummy seat's edit authority.
    awaitWithoutPaint([] { return player(1).ps == PlayerState::AI && player(1).aiInfo.type == AI::Type::Default; });
    BOOST_TEST_REQUIRE(card(1).IsJoined());
    tap(11, PadButton::DpadRight);
    WINDOWMANAGER.Draw();
    settle();
    BOOST_TEST((player(1).nation == before));
    lobby().CloseSlot(1);
    awaitWithoutPaint([] { return player(1).ps == PlayerState::Locked; });
    BOOST_TEST_REQUIRE(card(1).IsJoined());
    tap(11, PadButton::DpadRight);
    WINDOWMANAGER.Draw();
    settle();
    BOOST_TEST((player(1).nation == before));
    BOOST_TEST(!card(1).IsJoined());
}

BOOST_AUTO_TEST_SUITE_END()
