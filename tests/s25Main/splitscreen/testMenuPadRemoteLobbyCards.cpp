// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ILobbyClient.hpp"
#include "LocalGameFixture.h"
#include "MenuPadFixture.h"
#include "RTTR_Version.h"
#include "controls/ctrlCheck.h"
#include "controls/ctrlGroup.h"
#include "controls/ctrlLobbyPlayerCard.h"
#include "desktops/dskGameLobby.h"
#include "ingameWindows/iwLobbyPlayerCards.h"
#include "input/LocalViewColor.h"
#include "lua/GameDataLoader.h"
#include "mapGenerator/RandomMap.h"
#include "network/GameMessages.h"
#include "gameTypes/CompressedData.h"
#include "gameData/MaxPlayers.h"
#include "gameData/WorldDescription.h"
#include "libsiedler2/libsiedler2.h"
#include "s25util/MessageQueue.h"
#include "s25util/Socket.h"
#include <algorithm>
#include <optional>

namespace {
constexpr PadDeviceId pad = 220;
constexpr unsigned firstLocalCard = 48 + (2 * MAX_PLAYERS);

struct RosterFixture : rttr::test::LocalGameFixture, rttr::test::MenuPadFixture
{
    Socket peer;
    MessageQueue received{GameMessage::create_game};
    bool connected = false;
    std::optional<uint8_t> peerSlot;
    bool versionOk = false;
    bool passwordOk = false;
    uint16_t port = 0;

    void frame() override
    {
        video.tickCount_ += frameMs;
        GAMESERVER.Run();
        GAMECLIENT.Run();
        if(connected)
        {
            BOOST_TEST_REQUIRE(received.recvAll(peer) >= 0);
            while(!received.empty())
            {
                const auto message = received.pop();
                if(const auto* id = dynamic_cast<const GameMessage_Player_Id*>(message.get()))
                    peerSlot = id->player;
                else if(const auto* type = dynamic_cast<const GameMessage_Server_TypeOK*>(message.get()))
                    versionOk = type->err_code == GameMessage_Server_TypeOK::StatusCode::Ok;
                else if(const auto* password = dynamic_cast<const GameMessage_Server_Password*>(message.get()))
                    passwordOk = password->password == "true";
                else if(dynamic_cast<const GameMessage_Ping*>(message.get()))
                    send(GameMessage_Pong());
            }
        }
        WINDOWMANAGER.Draw();
    }

    void send(const GameMessage& message) { BOOST_TEST_REQUIRE(MessageQueue::sendMessage(peer, message)); }

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

    void enter(const ServerType type = ServerType::Direct, const boost::filesystem::path& map = {})
    {
        bool ready = false;
        for(unsigned attempt = 0; attempt < 10 && !ready; ++attempt)
        {
            GAMECLIENT.Stop();
            GAMESERVER.Stop();
            GAMECLIENT.SetInterface(&ci());
            port = static_cast<uint16_t>(rttr::test::randomValue(1024, 49151));
            if(GAMECLIENT.HostGame(CreateServerInfo(type, port, "Roster", "cards"),
                                   MapDescription(map.empty() ? mapPath() : map, MapType::OldMap)))
                ready =
                  pumpWhile([] { return GAMECLIENT.GetState() == ClientState::Config; }, std::chrono::seconds(10));
        }
        BOOST_TEST_REQUIRE(ready);
        WINDOWMANAGER.Switch(
          std::make_unique<dskGameLobby>(type, GAMECLIENT.GetGameLobby(), GAMECLIENT.GetPlayerId(), nullptr));
        frame();
        pickUp(pad);
    }

    void joinPeer()
    {
        BOOST_TEST_REQUIRE(peer.Connect("127.0.0.1", port, false));
        connected = true;
        await([&] { return peerSlot.has_value(); });
        BOOST_TEST_REQUIRE(*peerSlot < GAMECLIENT.GetGameLobby()->getNumPlayers());
        send(GameMessage_Server_Type(ServerType::Direct, rttr::version::GetRevision()));
        await([&] { return versionOk; });
        send(GameMessage_Server_Password("cards"));
        await([&] { return passwordOk; });
        send(GameMessage_Player_Name(0xFF, "Remote player"));
        CompressedData data;
        unsigned checksum = 0;
        BOOST_TEST_REQUIRE(data.CompressFromFile(mapPath(), &checksum));
        send(GameMessage_Map_Checksum(checksum, 0));
        await([&] {
            const auto& player = GAMECLIENT.GetGameLobby()->getPlayer(*peerSlot);
            return player.ps == PlayerState::Occupied && player.name == "Remote player";
        });
    }

    static iwLobbyPlayerCards& roster()
    {
        auto* window = dynamic_cast<iwLobbyPlayerCards*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(window);
        return *window;
    }

    static ctrlLobbyPlayerCard& card(unsigned index)
    {
        auto* result = roster().GetCtrl<ctrlLobbyPlayerCard>(index);
        BOOST_TEST_REQUIRE(result);
        return *result;
    }

    void focus(Window& target)
    {
        for(unsigned i = 0; i < 100 && focused(0) != &target; ++i)
            press(pad, PadButton::LeftShoulder);
        for(unsigned i = 0; i < 100 && focused(0) != &target; ++i)
            press(pad, PadButton::RightShoulder);
        BOOST_TEST_REQUIRE(focused(0) == &target);
    }

    void open()
    {
        auto* desktop = desktopAs<dskGameLobby>();
        BOOST_TEST_REQUIRE(desktop);
        const auto buttons = desktop->GetCtrls<ctrlTextButton>();
        const auto found = std::find_if(buttons.begin(), buttons.end(),
                                        [](const auto* button) { return button->GetText() == "Player cards"; });
        BOOST_TEST_REQUIRE((found != buttons.end()));
        focus(**found);
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(roster().IsModal());
    }

    void mouse(Window& control)
    {
        MouseCoords mc(control.GetDrawPos() + Position(control.GetSize() / 2u));
        WINDOWMANAGER.Msg_MouseMove(mc);
        mc.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mc);
        mc.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mc);
        frame();
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(MenuPadRemoteLobbyCardTests)

BOOST_FIXTURE_TEST_CASE(AuthenticatedRemoteValuesRefreshAndPhysicalBrowsingCannotEdit, RosterFixture)
{
    enter();
    joinPeer();
    open();
    BOOST_TEST_REQUIRE(card(1).GetText() == "Remote player");
    send(GameMessage_Player_Nation(0xFF, Nation::Japanese));
    send(GameMessage_Player_Team(0xFF, Team::Team2));
    send(GameMessage_Player_Color(0xFF, PLAYER_COLORS[5]));
    await([&] {
        const auto& values = card(1).GetModel().GetSnapshot().values;
        return values.nation == Nation::Japanese && values.team == Team::Team2 && values.color == PLAYER_COLORS[5];
    });
    const auto snapshot = card(1).GetModel().GetSnapshot().values;
    focus(card(1));
    BOOST_TEST(card(1).IsReadOnly());
    BOOST_TEST(!card(1).CanActivate());
    BOOST_TEST(!card(1).GetModel().ProposeValue(true).has_value());
    BOOST_TEST(!card(1).GetCursorColor().has_value());
    press(pad, PadButton::A);
    mouse(card(1));
    pressN(pad, PadButton::DpadRight, 5);
    pressN(pad, PadButton::DpadDown, 5);
    const auto& player = GAMECLIENT.GetGameLobby()->getPlayer(*peerSlot);
    BOOST_TEST(player.color == snapshot.color);
    BOOST_TEST((player.nation == snapshot.nation));
    BOOST_TEST((player.team == snapshot.team));
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Config));
    BOOST_TEST(GAMECLIENT.GetAdditionalLocalPlayers().empty());
    press(pad, PadButton::B);
    frame();
    BOOST_TEST(!WINDOWMANAGER.GetTopMostWindow());
    BOOST_TEST_REQUIRE(desktopAs<dskGameLobby>());
    // Mouse editing still uses the original lobby after browsing, including server authority.
    auto* group = desktop()->GetCtrl<ctrlGroup>(48 + GAMECLIENT.GetPlayerId());
    BOOST_TEST_REQUIRE(group);
    auto* nation = group->GetCtrl<ctrlTextButton>(28);
    BOOST_TEST_REQUIRE(nation);
    const auto oldNation = GAMECLIENT.GetGameLobby()->getPlayer(GAMECLIENT.GetPlayerId()).nation;
    mouse(*nation);
    await([&] { return GAMECLIENT.GetGameLobby()->getPlayer(GAMECLIENT.GetPlayerId()).nation != oldNation; });
}

BOOST_FIXTURE_TEST_CASE(MemberConversionNameAndLeaderValuesArriveThroughTheServer, RosterFixture)
{
    enter();
    joinPeer();
    GameLobbyController controller(GAMECLIENT.GetGameLobby(), GAMECLIENT.GetMainPlayer());
    controller.SetCoopMembersAllowed(true);
    await([] { return GAMECLIENT.AreCoopMembersAllowed(); });
    open();
    send(GameMessage_Coop_JoinMember(GAMECLIENT.GetPlayerId()));
    await([] { return GAMECLIENT.GetCoopMembers().size() == 1; });
    frame();
    BOOST_TEST(card(0).GetModel().GetSnapshot().values.sharedTribe);
    BOOST_TEST(card(1).GetModel().GetSnapshot().values.sharedTribe);
    BOOST_TEST(card(1).GetText().find("Remote player") != std::string::npos);
    controller.SetNation(GAMECLIENT.GetPlayerId(), Nation::Africans);
    await([] { return card(1).GetModel().GetSnapshot().values.nation == Nation::Africans; });
    send(GameMessage_Player_Name(0xFF, "Renamed co-player"));
    await([] { return card(1).GetText().find("Renamed co-player") != std::string::npos; });
    peer.Close();
    connected = false;
    await([] { return GAMECLIENT.GetCoopMembers().empty(); });
    frame();
    BOOST_TEST(!card(0).GetModel().GetSnapshot().values.sharedTribe);
    BOOST_TEST(!card(1).IsVisible());
    press(pad, PadButton::B);
    frame();
    BOOST_TEST(!WINDOWMANAGER.GetTopMostWindow());
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Config));
}

BOOST_FIXTURE_TEST_CASE(SharedCursorPreviewUsesCompactedViewOrderAndRestoresTribeColor, RosterFixture)
{
    enter(ServerType::Local);
    const auto localCard = [](unsigned seat) -> ctrlLobbyPlayerCard& {
        auto* result = desktop()->GetCtrl<ctrlLobbyPlayerCard>(firstLocalCard + seat);
        BOOST_TEST_REQUIRE(result);
        return *result;
    };
    pressN(pad, PadButton::DpadDown, 3);
    press(pad, PadButton::DpadRight); // Host changes the shared-tribe row.
    frame();
    BOOST_TEST_REQUIRE(localCard(0).GetModel().GetSnapshot().values.sharedTribe);
    pickUp(221);
    press(221, PadButton::A);
    pickUp(222);
    press(222, PadButton::A);
    frame();
    BOOST_TEST_REQUIRE(localCard(1).IsJoined());
    BOOST_TEST_REQUIRE(localCard(2).IsJoined());
    BOOST_TEST((localCard(0).GetCursorColor() == LocalViewColor(0, 0, true)));
    BOOST_TEST((localCard(1).GetCursorColor() == LocalViewColor(1, 0, true)));
    BOOST_TEST((localCard(2).GetCursorColor() == LocalViewColor(2, 0, true)));
    press(221, PadButton::B);
    frame();
    BOOST_TEST(!localCard(1).GetCursorColor().has_value());
    BOOST_TEST((localCard(2).GetCursorColor() == LocalViewColor(1, 0, true)));
    press(222, PadButton::B);
    frame();
    BOOST_TEST(!localCard(0).GetCursorColor().has_value());
    const auto color = GAMECLIENT.GetGameLobby()->getPlayer(GAMECLIENT.GetPlayerId()).color;
    BOOST_TEST(LocalViewColor(0, color, false) == color);
}

BOOST_FIXTURE_TEST_CASE(PagesAndLiveSlotRemovalKeepCardsInsideTheWindow, RosterFixture)
{
    WorldDescription descriptions;
    loadGameData(descriptions);
    rttr::mapGenerator::RandomUtility random(0);
    rttr::mapGenerator::MapSettings settings;
    settings.numPlayers = 8;
    settings.size = MapExtent::all(64);
    settings.style = rttr::mapGenerator::MapStyle::Land;
    const auto generated = rttr::mapGenerator::GenerateRandomMap(random, descriptions, settings);
    rttr::test::TmpFolder files;
    const auto map = files / "Roster.SWD";
    BOOST_TEST_REQUIRE(libsiedler2::Write(map, generated.CreateArchiv()) == 0);
    enter(ServerType::Direct, map);
    GameLobbyController controller(GAMECLIENT.GetGameLobby(), GAMECLIENT.GetMainPlayer());
    for(unsigned i = 1; i < 8; ++i)
        controller.SetPlayerState(i, PlayerState::AI, AI::Info(AI::Type::Default, AI::Level::Easy));
    await([] {
        const auto& players = GAMECLIENT.GetGameLobby()->getPlayers();
        return std::all_of(players.begin(), players.end(), [](const auto& player) { return player.isUsed(); });
    });
    open();
    const auto* previous = roster().GetCtrl<ctrlTextButton>(4);
    auto* next = roster().GetCtrl<ctrlTextButton>(5);
    BOOST_TEST_REQUIRE(previous);
    BOOST_TEST_REQUIRE(next);
    BOOST_TEST(!previous->GetEnabled());
    BOOST_TEST(next->GetEnabled());
    focus(*next);
    press(pad, PadButton::A);
    BOOST_TEST(previous->GetEnabled());
    BOOST_TEST(!next->GetEnabled());
    BOOST_TEST(card(0).GetText() == GAMECLIENT.GetGameLobby()->getPlayer(4).name);
    for(unsigned i = 0; i < 4; ++i)
    {
        const Rect bounds = roster().GetDrawRect();
        const Rect child = card(i).GetDrawRect();
        BOOST_TEST(child.left >= bounds.left);
        BOOST_TEST(child.top >= bounds.top);
        BOOST_TEST(child.right <= bounds.right);
        BOOST_TEST(child.bottom <= bounds.bottom);
    }
    mouse(*roster().GetCtrl<ctrlTextButton>(4));
    BOOST_TEST(!previous->GetEnabled());
    mouse(*next);
    BOOST_TEST(previous->GetEnabled());
    for(unsigned i = 1; i < 8; ++i)
        controller.CloseSlot(i);
    await([] {
        const auto& players = GAMECLIENT.GetGameLobby()->getPlayers();
        return std::count_if(players.begin(), players.end(), [](const auto& player) { return player.isUsed(); }) == 1;
    });
    frame();
    BOOST_TEST(!previous->GetEnabled());
    BOOST_TEST(!next->GetEnabled());
    BOOST_TEST(card(0).IsVisible());
    BOOST_TEST(!card(1).IsVisible());
    BOOST_TEST(card(0).GetText() == GAMECLIENT.GetGameLobby()->getPlayer(GAMECLIENT.GetPlayerId()).name);
    mouse(*roster().GetCtrl<ctrlTextButton>(6));
    frame();
    BOOST_TEST(!WINDOWMANAGER.GetTopMostWindow());
}

BOOST_AUTO_TEST_SUITE_END()
