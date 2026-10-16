// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GameLobby.h"
#include "JoinPlayerInfo.h"
#include "Loader.h"
#include "LocalGameFixture.h"
#include "MenuPadFixture.h"
#include "RTTR_Version.h"
#include "RttrConfig.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlChat.h"
#include "controls/ctrlCheck.h"
#include "controls/ctrlComboBox.h"
#include "controls/ctrlConnectionProgress.h"
#include "controls/ctrlEdit.h"
#include "controls/ctrlLobbyPlayerCard.h"
#include "controls/ctrlPercent.h"
#include "controls/ctrlTable.h"
#include "controls/ctrlText.h"
#include "controls/ctrlTextButton.h"
#include "controls/ctrlTextDeepening.h"
#include "desktops/dskFrontEndConnect.h"
#include "desktops/dskFrontEndHost.h"
#include "desktops/dskFrontEndInternet.h"
#include "desktops/dskFrontEndLan.h"
#include "desktops/dskFrontEndMaps.h"
#include "desktops/dskFrontEndOnline.h"
#include "desktops/dskGameLobby.h"
#include "desktops/dskHome.h"
#include "desktops/dskMultiPlayer.h"
#include "desktops/dskParty.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "files.h"
#include "ingameWindows/iwControllerKeyboard.h"
#include "ingameWindows/iwDirectIPCreate.h"
#include "ingameWindows/iwLobbyConnect.h"
#include "ingameWindows/iwMsgbox.h"
#include "mygettext/mygettext.h"
#include "network/GameMessage_Chat.h"
#include "network/GameMessages.h"
#include "gameTypes/CompressedData.h"
#include "gameData/LanDiscoveryCfg.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include "s25util/MessageQueue.h"
#include "s25util/Serializer.h"
#include "s25util/Socket.h"
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <stdexcept>

namespace {
using Page = dskFrontEndConnect;
using Online = dskFrontEndOnline;
constexpr PadDeviceId pad = 241;
constexpr PadDeviceId other = 242;

[[noreturn]] void onlineCleanupProbe()
{
    throw std::runtime_error("online cleanup probe");
}

struct OnlineFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder userData;
    rttr::test::ConfigOverride userOverride{"USERDATA", userData};
    rttr::test::ConfigOverride gameOverride{"GAME", userData};
    const decltype(SETTINGS.server) savedServer = SETTINGS.server;
    const decltype(SETTINGS.proxy) savedProxy = SETTINGS.proxy;
    const decltype(SETTINGS.video) savedVideo = SETTINGS.video;
    const VideoMode savedSize = VIDEODRIVER.GetWindowSize();
    const DisplayMode savedMode = VIDEODRIVER.GetDisplayMode();
    const unsigned savedScale = VIDEODRIVER.getGuiScale().percent();
    const unsigned savedReference = VIDEODRIVER.getUiReferenceHeight();
    bool cleaned = false;

    OnlineFixture()
    {
        SETTINGS.proxy.type = ProxyType::None;
        SETTINGS.server.lastIP = "127.0.0.1";
        SETTINGS.server.localPort = 2105;
        SETTINGS.server.ipv6 = false;
        LOADER.LoadDummyLanguageFiles();
    }
    void cleanup()
    {
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        frame();
        GAMECLIENT.Stop();
        GAMESERVER.Stop();
        SETTINGS.server = savedServer;
        SETTINGS.proxy = savedProxy;
        VIDEODRIVER.setUiReferenceHeight(savedReference);
        VIDEODRIVER.setGuiScalePercent(savedScale);
        VIDEODRIVER.ResizeScreen(savedSize, savedMode);
        SETTINGS.video = savedVideo;
        dskHome::ForgetLastChoice();
        cleaned = true;
    }
    template<class F>
    void checked(F&& body)
    {
        try
        {
            body();
        } catch(...)
        {
            cleanup();
            throw;
        }
        cleanup();
        BOOST_TEST(SETTINGS.server.lastIP == savedServer.lastIP);
        BOOST_TEST(SETTINGS.server.localPort == savedServer.localPort);
        BOOST_TEST(SETTINGS.server.ipv6 == savedServer.ipv6);
        BOOST_TEST((SETTINGS.video.windowedSize == savedVideo.windowedSize));
    }
    static Page& page()
    {
        auto* result = desktopAs<Page>();
        BOOST_TEST_REQUIRE(result);
        return *result;
    }
    void focus(unsigned id)
    {
        for(unsigned i = 0; i < 24 && focusedId(0) != id; ++i)
            press(pad, focusedId(0) < id ? PadButton::RightShoulder : PadButton::LeftShoulder);
        BOOST_TEST_REQUIRE(focusedId(0) == id);
    }
    void choose(unsigned id)
    {
        focus(id);
        press(pad, PadButton::A);
        frame();
    }
    void enter(bool join = true)
    {
        dskHome::ForgetLastChoice();
        WINDOWMANAGER.Switch(dskHome::Create());
        frame();
        pickUp(pad);
        choose(dskHome::ID_Online);
        BOOST_TEST_REQUIRE(desktopAs<Online>());
        if(join)
        {
            choose(Online::ID_Join);
            BOOST_TEST_REQUIRE(desktopAs<Page>());
        }
    }
    void click(Window& control)
    {
        MouseCoords mouse(control.GetDrawPos() + Position(control.GetSize() / 2u));
        WINDOWMANAGER.Msg_MouseMove(mouse);
        mouse.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mouse);
        mouse.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mouse);
        frame();
    }
    void type(unsigned id, const std::u32string& text)
    {
        const auto count = desktop()->GetCtrl<ctrlEdit>(id)->GetText().size();
        click(*desktop()->GetCtrl<ctrlEdit>(id));
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::End));
        for(unsigned i = 0; i < count; ++i)
            WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Backspace));
        for(const auto character : text)
            WINDOWMANAGER.Msg_KeyDown(KeyEvent(character));
    }
    static dskFrontEndLan& lan()
    {
        auto* result = desktopAs<dskFrontEndLan>();
        BOOST_TEST_REQUIRE(result);
        return *result;
    }
    void enterLan()
    {
        enter(false);
        choose(Online::ID_Lan);
        BOOST_TEST_REQUIRE(desktopAs<dskFrontEndLan>());
    }
    void advertise(uint16_t port = 2345, const std::string& name = "Fixture LAN",
                   const std::string& revision = rttr::version::GetRevision(), bool active = true,
                   bool malformed = false, unsigned frames = 10)
    {
        LANDiscoveryBase::Info packet{};
        packet.GetMagic() = LAN_DISCOVERY_CFG.magicResponse;
        packet.GetVersion() = LAN_DISCOVERY_CFG.version;
        packet.GetIsActive() = active ? 1 : 0;
        LanGameInfo info{name, true, "Fixture map", 1, 4, port, false, "Fixture version", revision};
        Serializer ser;
        info.Serialize(ser);
        std::copy_n(ser.GetData(), ser.GetLength(), packet.GetPayload().data());
        if(malformed)
            packet.GetPayload().fill(static_cast<char>(0xff));
        Socket sender;
        BOOST_TEST_REQUIRE(sender.Create(AF_INET, true));
        // BSD sendto requires the IPv4 address length, not PeerAddr's storage-union size.
        sockaddr_in receiver{};
        receiver.sin_family = AF_INET;
        receiver.sin_port = htons(LAN_DISCOVERY_CFG.portResponse);
        receiver.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        BOOST_TEST_REQUIRE(sendto(sender.GetSocket(), packet.data.data(), sizeof(packet), 0,
                                  reinterpret_cast<const sockaddr*>(&receiver), sizeof(receiver))
                           == static_cast<int>(sizeof(packet)));
        for(unsigned i = 0; i < frames; ++i)
        {
            frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    void back()
    {
        press(pad, PadButton::B);
        frame();
    }
};

struct JoinFixture : rttr::test::LocalGameFixture, OnlineFixture
{
    uint16_t port = 0;
    Socket host;
    MessageQueue hostMessages{GameMessage::create_game};
    std::optional<uint8_t> hostSlot;
    bool hostVersion = false;
    bool hostPassword = false;
    bool membersAllowed = false;
    std::vector<std::string> chats;

    void frame() override
    {
        video.tickCount_ += frameMs;
        GAMESERVER.Run();
        GAMECLIENT.Run();
        WINDOWMANAGER.Draw();
    }
    template<class Predicate>
    void await(const Predicate& ready)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while(!ready() && std::chrono::steady_clock::now() < deadline)
        {
            frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        BOOST_TEST_REQUIRE(ready());
    }
    void server(bool ipv6 = false, ServerType type = ServerType::Direct)
    {
        bool started = false;
        for(unsigned i = 0; i < 10 && !started; ++i)
        {
            GAMESERVER.Stop();
            port = static_cast<uint16_t>(rttr::test::randomValue(1024, 49151));
            started = GAMESERVER.Start(CreateServerInfo(type, port, "Online test", "secret", ipv6),
                                       MapDescription(mapPath(), MapType::OldMap), "host-secret");
        }
        BOOST_TEST_REQUIRE(started);
    }
    void receiveHost()
    {
        BOOST_TEST_REQUIRE(hostMessages.recvAll(host) >= 0);
        while(!hostMessages.empty())
        {
            const auto msg = hostMessages.pop();
            if(const auto* id = dynamic_cast<const GameMessage_Player_Id*>(msg.get()))
                hostSlot = id->player;
            else if(const auto* version = dynamic_cast<const GameMessage_Server_TypeOK*>(msg.get()))
                hostVersion = version->err_code == GameMessage_Server_TypeOK::StatusCode::Ok;
            else if(const auto* password = dynamic_cast<const GameMessage_Server_Password*>(msg.get()))
                hostPassword = password->password == "true";
            else if(const auto* members = dynamic_cast<const GameMessage_Coop_Members*>(msg.get()))
                membersAllowed = members->allowed;
            else if(const auto* chat = dynamic_cast<const GameMessage_Chat*>(msg.get()))
                chats.push_back(chat->text);
        }
    }
    void send(const GameMessage& message) { BOOST_TEST_REQUIRE(MessageQueue::sendMessage(host, message)); }
    void joinHost(ServerType type = ServerType::Direct)
    {
        BOOST_TEST_REQUIRE(host.Connect("127.0.0.1", port, false));
        await([&] {
            receiveHost();
            return hostSlot.has_value();
        });
        send(GameMessage_Server_Type(type, rttr::version::GetRevision()));
        await([&] {
            receiveHost();
            return hostVersion;
        });
        send(GameMessage_Server_Password("host-secret"));
        await([&] {
            receiveHost();
            return hostPassword;
        });
        send(GameMessage_Player_Name(0xFF, "Fixture host"));
        CompressedData data;
        unsigned checksum = 0;
        BOOST_TEST_REQUIRE(data.CompressFromFile(mapPath(), &checksum));
        send(GameMessage_Map_Checksum(checksum, 0));
        send(GameMessage_Coop_AllowMembers(true));
        await([&] {
            receiveHost();
            return membersAllowed;
        });
    }
    void fill(const std::u32string& password = U"secret", bool ipv6 = false)
    {
        enter();
        // The existing resolver's localhost path reaches IPv6 loopback even without a global IPv6 interface.
        type(Page::ID_Host, ipv6 ? U"localhost" : U"127.0.0.1");
        const std::string portText = std::to_string(port);
        type(Page::ID_Port, std::u32string(portText.begin(), portText.end()));
        type(Page::ID_Password, password);
        if(ipv6)
            choose(Page::ID_Ipv6);
    }
    template<class F>
    void checkedJoin(F&& body)
    {
        try
        {
            body();
        } catch(...)
        {
            host.Close();
            cleanup();
            throw;
        }
        host.Close();
        cleanup();
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
        BOOST_TEST(SETTINGS.server.lastIP == savedServer.lastIP);
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(FrontEndOnlineTests)

BOOST_FIXTURE_TEST_CASE(FullScreenTrailAndResizeStayInsideTheFrame, OnlineFixture)
{
    checked([&] {
        enter();
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(focusedId(0) == Page::ID_Host);
        for(const VideoMode size :
            {VideoMode(800, 480), VideoMode(800, 600), VideoMode(1024, 640), VideoMode(1280, 800)})
        {
            BOOST_TEST_REQUIRE(VIDEODRIVER.ResizeScreen(size, DisplayMode::Windowed));
            frame();
            for(unsigned id = Page::ID_Host; id <= Page::ID_Status; ++id)
            {
                const auto rect = page().GetCtrl<Window>(id)->GetBoundaryRect();
                const auto& content = page().GetFrame().content;
                BOOST_TEST(rect.left >= content.left);
                BOOST_TEST(rect.right <= content.right);
                BOOST_TEST(rect.top >= content.top);
                BOOST_TEST(rect.bottom <= content.bottom);
            }
        }
        back();
        BOOST_TEST_REQUIRE(desktopAs<Online>());
        BOOST_TEST(focusedId(0) == Online::ID_Join);
        back();
        BOOST_TEST_REQUIRE(desktopAs<dskHome>());
        BOOST_TEST(focusedId(0) == dskHome::ID_Online);
    });
}

BOOST_FIXTURE_TEST_CASE(ControllerFieldsStageCommitCancelAndKeepPasswordsMasked, OnlineFixture)
{
    checked([&] {
        enter();
        pickUp(other);
        for(const unsigned id : {Page::ID_Host, Page::ID_Port, Page::ID_Password})
        {
            focus(id);
            const auto original = page().GetCtrl<ctrlEdit>(id)->GetText();
            press(pad, PadButton::A);
            auto* keyboard = dynamic_cast<iwControllerKeyboard*>(WINDOWMANAGER.GetTopMostWindow());
            BOOST_TEST_REQUIRE(keyboard);
            press(other, PadButton::A);
            press(other, PadButton::Start);
            BOOST_TEST(keyboard->GetCtrl<ctrlEdit>(0)->GetText() == original);
            press(pad, PadButton::A);
            const auto draft = keyboard->GetCtrl<ctrlEdit>(0)->GetText();
            BOOST_TEST(draft == original + "1");
            BOOST_TEST(page().GetCtrl<ctrlEdit>(id)->GetText() == original);
            if(id == Page::ID_Password)
                BOOST_TEST(keyboard->GetCtrl<ctrlEdit>(0)->GetCtrl<ctrlTextDeepening>(0)->GetText() == "*");
            press(pad, PadButton::Start);
            frame();
            BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
            BOOST_TEST(page().GetCtrl<ctrlEdit>(id)->GetText() == draft);
            BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
        }
        focus(Page::ID_Host);
        const auto original = page().GetCtrl<ctrlEdit>(Page::ID_Host)->GetText();
        press(pad, PadButton::A);
        auto* keyboard = dynamic_cast<iwControllerKeyboard*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(keyboard);
        press(pad, PadButton::A);
        click(*keyboard->GetCtrl<Window>(104));
        BOOST_TEST(page().GetCtrl<ctrlEdit>(Page::ID_Host)->GetText() == original);
        press(pad, PadButton::A);
        disconnect(pad);
        frame();
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    });
}

BOOST_FIXTURE_TEST_CASE(PortValidationNumericInputAndFailedConnectAllowRetry, OnlineFixture)
{
    checked([&] {
        enter();
        for(const std::u32string port : {U"", U"0", U"65536"})
        {
            type(Page::ID_Port, port);
            choose(Page::ID_Connect);
            BOOST_TEST(page().GetCtrl<ctrlText>(Page::ID_Status)->GetText()
                       == _("Invalid port. The valid port-range is 1 to 65535!"));
            BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
            BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
            BOOST_TEST(page().GetCtrl<ctrlButton>(Page::ID_Connect)->GetEnabled());
        }
        type(Page::ID_Port, U"1abc");
        BOOST_TEST(page().GetCtrl<ctrlEdit>(Page::ID_Port)->GetText() == "1");
        type(Page::ID_Host, U"invalid host address");
        choose(Page::ID_Connect);
        BOOST_TEST(page().GetCtrl<ctrlText>(Page::ID_Status)->GetText() == "Connection failed!");
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        type(Page::ID_Port, U"65535");
        BOOST_TEST(page().GetCtrl<ctrlText>(Page::ID_Status)->GetText().empty());
        choose(Page::ID_Connect);
        BOOST_TEST(page().GetCtrl<ctrlText>(Page::ID_Status)->GetText() == "Connection failed!");
        BOOST_TEST(SETTINGS.server.lastIP == "127.0.0.1");
    });
}

BOOST_FIXTURE_TEST_CASE(BackAndReentryStartWithoutThePreviousPassword, OnlineFixture)
{
    checked([&] {
        enter();
        type(Page::ID_Password, U"secret");
        BOOST_TEST(page().GetCtrl<ctrlEdit>(Page::ID_Password)->GetCtrl<ctrlTextDeepening>(0)->GetText() == "******");
        const auto rememberedHost = SETTINGS.server.lastIP;
        const auto rememberedPort = SETTINGS.server.localPort;
        back();
        BOOST_TEST_REQUIRE(desktopAs<Online>());
        choose(Online::ID_Join);
        BOOST_TEST(page().GetCtrl<ctrlEdit>(Page::ID_Password)->GetText().empty());
        BOOST_TEST(SETTINGS.server.lastIP == rememberedHost);
        BOOST_TEST(SETTINGS.server.localPort == rememberedPort);
    });
}

BOOST_FIXTURE_TEST_CASE(LegacyNetworkHostAndLobbyStayReachableWithProxyGuard, OnlineFixture)
{
    checked([&] {
        enter(false);
        choose(Online::ID_Host);
        BOOST_TEST_REQUIRE(desktopAs<dskFrontEndHost>());
        back();
        BOOST_TEST_REQUIRE(desktopAs<Online>());
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        SETTINGS.proxy.type = ProxyType::Socks4;
        choose(Online::ID_Host);
        BOOST_TEST_REQUIRE(dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow()));
        back();
        BOOST_TEST_REQUIRE(dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow()));
        press(pad, PadButton::A);
        frame();
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        SETTINGS.proxy.type = ProxyType::None;
        choose(Online::ID_Internet);
        BOOST_TEST_REQUIRE(desktopAs<dskFrontEndInternet>());
        back();
        BOOST_TEST_REQUIRE(desktopAs<Online>());
        choose(Online::ID_Lan);
        BOOST_TEST_REQUIRE(desktopAs<dskFrontEndLan>());
        back();
        BOOST_TEST_REQUIRE(desktopAs<Online>());
        BOOST_TEST(focusedId(0) == Online::ID_Lan);
        back();
        BOOST_TEST_REQUIRE(desktopAs<dskHome>());
    });
}

BOOST_FIXTURE_TEST_CASE(MouseKeyboardEnterAndEscapeUseTheSamePage, OnlineFixture)
{
    checked([&] {
        enter();
        type(Page::ID_Host, U"localhost");
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Return));
        BOOST_TEST(page().GetCtrl<ctrlEdit>(Page::ID_Port)->HasFocus());
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Return));
        BOOST_TEST(page().GetCtrl<ctrlEdit>(Page::ID_Password)->HasFocus());
        type(Page::ID_Port, U"0");
        click(*page().GetCtrl<ctrlEdit>(Page::ID_Password));
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Return));
        BOOST_TEST(page().GetCtrl<ctrlText>(Page::ID_Status)->GetText()
                   == _("Invalid port. The valid port-range is 1 to 65535!"));
        click(*page().GetCtrl<Window>(Page::ID_Ipv6));
        click(*page().GetCtrl<Window>(Page::ID_Coop));
        BOOST_TEST(page().GetCtrl<ctrlCheck>(Page::ID_Ipv6)->isChecked());
        BOOST_TEST(page().GetCtrl<ctrlCheck>(Page::ID_Coop)->isChecked());
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Online>());
        click(*desktop()->GetCtrl<Window>(Online::ID_btBack));
        BOOST_TEST_REQUIRE(desktopAs<dskHome>());
    });
}

BOOST_FIXTURE_TEST_CASE(RealPasswordHandshakeRetriesThenJoinsTheOnlineLobby, JoinFixture)
{
    checkedJoin([&] {
        server();
        fill(U"wrong");
        choose(Page::ID_Connect);
        BOOST_TEST_REQUIRE(page().GetCtrl<ctrlConnectionProgress>(Page::ID_Connection)->IsConnecting());
        BOOST_TEST(!page().GetCtrl<ctrlButton>(Page::ID_Connect)->GetEnabled());
        await([] { return !page().GetCtrl<ctrlConnectionProgress>(Page::ID_Connection)->IsConnecting(); });
        BOOST_TEST(page().GetCtrl<ctrlText>(Page::ID_Status)->GetText()
                   == ClientErrorToStr(ClientError::WrongPassword));
        BOOST_TEST(page().GetCtrl<ctrlButton>(Page::ID_Connect)->GetEnabled());
        type(Page::ID_Password, U"secret");
        choose(Page::ID_Connect);
        // F7d2: a player who joins from the new menus gets the full-screen party page.
        await([] { return desktopAs<dskParty>() != nullptr; });
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Config));
        BOOST_TEST(!GAMECLIENT.IsHost());
        BOOST_TEST(!GAMECLIENT.IsCoopMember());
        BOOST_TEST(SETTINGS.server.lastIP == "127.0.0.1");
    });
}

BOOST_FIXTURE_TEST_CASE(RealIPv6HandshakeAndAbortLeaveTheJoinTrailUsable, JoinFixture)
{
    checkedJoin([&] {
        server(true);
        fill(U"secret", true);
        choose(Page::ID_Connect);
        BOOST_TEST_REQUIRE(page().GetCtrl<ctrlConnectionProgress>(Page::ID_Connection)->IsConnecting());
        back();
        BOOST_TEST(!page().GetCtrl<ctrlConnectionProgress>(Page::ID_Connection)->IsConnecting());
        BOOST_TEST_REQUIRE(desktopAs<Page>());
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
        BOOST_TEST(page().GetCtrl<ctrlButton>(Page::ID_Connect)->GetEnabled());
        choose(Page::ID_Connect);
        await([] { return desktopAs<dskParty>() != nullptr; });
        BOOST_TEST(SETTINGS.server.lastIP == "localhost");
        BOOST_TEST(SETTINGS.server.ipv6);
        BOOST_TEST(!GAMECLIENT.IsCoopMember());
    });
}

BOOST_FIXTURE_TEST_CASE(RealHostTribeMembershipUsesTheSelectedJoinPolicy, JoinFixture)
{
    checkedJoin([&] {
        server();
        joinHost();
        fill();
        choose(Page::ID_Coop);
        BOOST_TEST(page().GetCtrl<ctrlCheck>(Page::ID_Coop)->isChecked());
        choose(Page::ID_Connect);
        await([] { return desktopAs<dskGameLobby>() != nullptr; });
        BOOST_TEST(GAMECLIENT.IsCoopMember());
        BOOST_TEST_REQUIRE(hostSlot.has_value());
        BOOST_TEST(GAMECLIENT.GetPlayerId() == *hostSlot);
        BOOST_TEST(GAMECLIENT.GetCoopMembers().size() == 1u);
    });
}

BOOST_FIXTURE_TEST_CASE(JoinedPlayerGetsThePartyPageWithOwnCardReadyChatAndCoPlay, JoinFixture)
{
    checkedJoin([&] {
        server();
        joinHost(); // a real host on the other end, which allows co-players
        fill();
        choose(Page::ID_Connect);
        await([] { return desktopAs<dskParty>() != nullptr; });
        auto& party = *desktopAs<dskParty>();
        BOOST_TEST_REQUIRE(party.IsOnline());
        BOOST_TEST(!GAMECLIENT.IsHost());
        const auto lobby = GAMECLIENT.GetGameLobby();
        const unsigned me = GAMECLIENT.GetPlayerId();
        BOOST_TEST_REQUIRE(hostSlot.has_value());
        await([&] { return lobby->getPlayer(*hostSlot).name == "Fixture host"; });
        // Who hosts, and the cards: ours editable, the host's read-only.
        BOOST_TEST(party.GetCtrl<ctrlText>(dskParty::ID_Address)->GetText().find("Fixture host") != std::string::npos);
        BOOST_TEST(!party.GetCtrl<ctrlLobbyPlayerCard>(dskParty::ID_Net + me)->IsReadOnly());
        BOOST_TEST(party.GetCtrl<ctrlLobbyPlayerCard>(dskParty::ID_Net + *hostSlot)->IsReadOnly());
        // The rules are the host's: a joined player can only look at them.
        BOOST_TEST(
          !party.GetCtrl<ctrlButton>(dskParty::ID_Rule + static_cast<unsigned>(dskParty::Rule::Goals))->GetEnabled());
        // Our own colour goes through the server.
        focus(dskParty::ID_Net + me);
        const unsigned colorBefore = lobby->getPlayer(me).color;
        press(pad, PadButton::DpadRight);
        await([&] { return lobby->getPlayer(me).color != colorBefore; });
        // Ready instead of Start; pressing it again takes it back.
        auto* start = party.GetCtrl<ctrlTextButton>(dskParty::ID_Start);
        BOOST_TEST(start->GetText() == _("Ready"));
        focus(dskParty::ID_Start);
        press(pad, PadButton::A);
        frame();
        BOOST_TEST(lobby->getPlayer(me).isReady);
        BOOST_TEST(start->GetText() == _("Not Ready"));
        press(pad, PadButton::A);
        frame();
        BOOST_TEST(!lobby->getPlayer(me).isReady);
        // The host's chat line shows up here.
        send(GameMessage_Chat(0xFF, ChatDestination::All, "hi there"));
        await([&] { return party.GetCtrl<ctrlChat>(dskParty::ID_Chat)->GetNumMessages() > 0u; });
        // The host allows co-players: we may play its tribe together with it, which the classic lobby shows.
        await([&] {
            receiveHost();
            return party.GetCtrl<Window>(dskParty::ID_Coop)->IsVisible();
        });
        BOOST_TEST(party.GetCtrl<ctrlTextButton>(dskParty::ID_Coop)->GetText().find("Fixture host")
                   != std::string::npos);
        focus(dskParty::ID_Coop);
        press(pad, PadButton::A);
        await([] { return GAMECLIENT.IsCoopMember() && desktopAs<dskGameLobby>() != nullptr; });
        BOOST_TEST(GAMECLIENT.GetPlayerId() == *hostSlot);
    });
}

BOOST_AUTO_TEST_CASE(ExceptionCleanupRestoresSettingsWithAKeyboardOrHandshakeOpen)
{
    {
        OnlineFixture fixture;
        bool reached = false;
        BOOST_CHECK_THROW(fixture.checked([&] {
            fixture.enter();
            fixture.press(pad, PadButton::A);
            BOOST_TEST_REQUIRE(dynamic_cast<iwControllerKeyboard*>(WINDOWMANAGER.GetTopMostWindow()));
            reached = true;
            onlineCleanupProbe();
        }),
                          std::runtime_error);
        BOOST_TEST(reached);
        BOOST_TEST(fixture.cleaned);
        BOOST_TEST(SETTINGS.server.lastIP == fixture.savedServer.lastIP);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    }
    {
        JoinFixture fixture;
        bool reached = false;
        BOOST_CHECK_THROW(fixture.checkedJoin([&] {
            fixture.server();
            fixture.fill();
            fixture.choose(Page::ID_Connect);
            BOOST_TEST_REQUIRE(fixture.page().GetCtrl<ctrlConnectionProgress>(Page::ID_Connection)->IsConnecting());
            reached = true;
            onlineCleanupProbe();
        }),
                          std::runtime_error);
        BOOST_TEST(reached);
        BOOST_TEST(fixture.cleaned);
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    }
}

BOOST_FIXTURE_TEST_CASE(LanDiscoveryEmptyRefreshResizeAndBackUseThePageTrail, OnlineFixture)
{
    checked([&] {
        enterLan();
        BOOST_TEST(!lan().GetCtrl<ctrlButton>(dskFrontEndLan::ID_Join)->GetEnabled());
        BOOST_TEST(focusedId(0) == dskFrontEndLan::ID_Refresh);
        advertise();
        auto* table = lan().GetCtrl<ctrlTable>(dskFrontEndLan::ID_Servers);
        BOOST_TEST(table->GetNumRows() == 1u);
        BOOST_TEST(table->GetItemText(0, 0) == "(pwd) Fixture LAN");
        BOOST_TEST(table->GetItemText(0, 1) == "Fixture map");
        BOOST_TEST(table->GetItemText(0, 2) == "1/4");
        for(const VideoMode size :
            {VideoMode(800, 480), VideoMode(800, 600), VideoMode(1024, 640), VideoMode(1280, 800)})
        {
            BOOST_TEST_REQUIRE(VIDEODRIVER.ResizeScreen(size, DisplayMode::Windowed));
            frame();
            for(unsigned id = dskFrontEndLan::ID_Servers; id <= dskFrontEndLan::ID_Host; ++id)
            {
                const auto rect = lan().GetCtrl<Window>(id)->GetBoundaryRect();
                const auto area = lan().GetFrame().content;
                BOOST_TEST(rect.left >= area.left);
                BOOST_TEST(rect.right <= area.right);
                BOOST_TEST(rect.top >= area.top);
                BOOST_TEST(rect.bottom <= area.bottom);
            }
        }
        choose(dskFrontEndLan::ID_Refresh);
        BOOST_TEST(table->GetNumRows() == 0u);
        advertise(2345, "Renamed LAN");
        BOOST_TEST_REQUIRE(table->GetSelection().has_value());
        BOOST_TEST(table->GetItemText(*table->GetSelection(), 0) == "(pwd) Renamed LAN");
        back();
        BOOST_TEST_REQUIRE(desktopAs<Online>());
        BOOST_TEST(focusedId(0) == Online::ID_Lan);
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
        frame();
        BOOST_TEST_REQUIRE(desktopAs<dskHome>());
        BOOST_TEST(focusedId(0) == dskHome::ID_Online);
    });
}

BOOST_FIXTURE_TEST_CASE(LanEndpointReplacementWithdrawalVersionAndMalformedPayloadCannotJoin, OnlineFixture)
{
    checked([&] {
        enterLan();
        advertise();
        choose(dskFrontEndLan::ID_Servers);
        BOOST_TEST_REQUIRE(desktopAs<Page>());
        BOOST_TEST(page().GetCtrl<ctrlEdit>(Page::ID_Host)->GetText() == "127.0.0.1");
        BOOST_TEST(page().GetCtrl<ctrlEdit>(Page::ID_Port)->GetText() == "2345");
        type(Page::ID_Password, U"private");
        back();
        advertise(3456, "Different endpoint");
        auto* table = lan().GetCtrl<ctrlTable>(dskFrontEndLan::ID_Servers);
        BOOST_TEST(!table->GetSelection().has_value());
        BOOST_TEST(!lan().GetCtrl<ctrlButton>(dskFrontEndLan::ID_Join)->GetEnabled());
        focus(dskFrontEndLan::ID_Servers);
        press(pad, PadButton::DpadDown);
        choose(dskFrontEndLan::ID_Join);
        BOOST_TEST_REQUIRE(desktopAs<Page>());
        BOOST_TEST(page().GetCtrl<ctrlEdit>(Page::ID_Port)->GetText() == "3456");
        BOOST_TEST(page().GetCtrl<ctrlEdit>(Page::ID_Password)->GetText().empty());
        back();
        advertise(3456, "Different version", "incompatible");
        BOOST_TEST(!lan().GetCtrl<ctrlButton>(dskFrontEndLan::ID_Join)->GetEnabled());
        choose(dskFrontEndLan::ID_Servers);
        BOOST_TEST_REQUIRE(desktopAs<dskFrontEndLan>());
        advertise(3456, "Offline", rttr::version::GetRevision(), false);
        BOOST_TEST(lan().GetCtrl<ctrlTable>(dskFrontEndLan::ID_Servers)->GetNumRows() == 0u);
        advertise(3456, "Invalid", rttr::version::GetRevision(), true, true);
        BOOST_TEST(lan().GetCtrl<ctrlTable>(dskFrontEndLan::ID_Servers)->GetNumRows() == 0u);
        advertise(3456, "Restored");
        BOOST_TEST(lan().GetCtrl<ctrlButton>(dskFrontEndLan::ID_Join)->GetEnabled());
        // A malformed advertisement from the same address must not displace the valid entry.
        advertise(3456, "Invalid", rttr::version::GetRevision(), true, true);
        BOOST_TEST(lan().GetCtrl<ctrlTable>(dskFrontEndLan::ID_Servers)->GetNumRows() == 1u);
        BOOST_TEST(lan().GetCtrl<ctrlButton>(dskFrontEndLan::ID_Join)->GetEnabled());
        focus(dskFrontEndLan::ID_Servers);
        // Queue the withdrawal without a paint; the next physical A must drain it before dispatch.
        advertise(3456, "Gone", rttr::version::GetRevision(), false, false, 0);
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<dskFrontEndLan>());
        BOOST_TEST(lan().GetCtrl<ctrlTable>(dskFrontEndLan::ID_Servers)->GetNumRows() == 0u);
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
    });
}

BOOST_FIXTURE_TEST_CASE(LanMouseKeyboardHostProxyAndSmallWindowRecovery, OnlineFixture)
{
    checked([&] {
        enterLan();
        advertise();
        click(*lan().GetCtrl<Window>(dskFrontEndLan::ID_Join));
        BOOST_TEST_REQUIRE(desktopAs<Page>());
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
        frame();
        advertise();
        SETTINGS.proxy.type = ProxyType::Socks4;
        choose(dskFrontEndLan::ID_Host);
        BOOST_TEST_REQUIRE(dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow()));
        press(pad, PadButton::A);
        frame();
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        SETTINGS.proxy.type = ProxyType::None;
        choose(dskFrontEndLan::ID_Host);
        BOOST_TEST_REQUIRE(desktopAs<dskFrontEndHost>());
        back();
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        VIDEODRIVER.setUiReferenceHeight(0);
        VIDEODRIVER.setGuiScalePercent(100);
        BOOST_TEST_REQUIRE(VIDEODRIVER.ResizeScreen(VideoMode(480, 320), DisplayMode::Windowed));
        frame();
        // Resize notifications clamp to the legacy minimum; a new page reads the actual render size.
        enterLan();
        BOOST_TEST_REQUIRE(lan().GetFrame().content.getSize().x < 480);
        for(unsigned id = dskFrontEndLan::ID_Servers; id <= dskFrontEndLan::ID_Host; ++id)
            BOOST_TEST(!lan().GetCtrl<Window>(id)->IsVisible());
        BOOST_TEST(lan().GetCtrl<ctrlText>(dskFrontEndLan::ID_Status)->GetText()
                   == _("Reduce the UI scale or enlarge the window."));
        BOOST_TEST_REQUIRE(VIDEODRIVER.ResizeScreen(VideoMode(1280, 800), DisplayMode::Windowed));
        frame();
        BOOST_TEST(lan().GetCtrl<Window>(dskFrontEndLan::ID_Servers)->IsVisible());
        advertise();
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Down));
        BOOST_TEST(lan().GetCtrl<ctrlTable>(dskFrontEndLan::ID_Servers)->GetSelection().has_value());
        click(*lan().GetCtrl<Window>(dskFrontEndLan::ID_btBack));
        BOOST_TEST_REQUIRE(desktopAs<Online>());
    });
}

BOOST_FIXTURE_TEST_CASE(LanPasswordRetryAndHostTribeUseTheRealLanHandshake, JoinFixture)
{
    checkedJoin([&] {
        server(false, ServerType::LAN);
        joinHost(ServerType::LAN);
        enterLan();
        advertise(port, "Online test");
        auto* table = lan().GetCtrl<ctrlTable>(dskFrontEndLan::ID_Servers);
        focus(dskFrontEndLan::ID_Servers);
        // Every announcement here uses the same host name, including the real server's broadcast.
        BOOST_TEST_REQUIRE(table->GetSelection().has_value());
        BOOST_TEST_REQUIRE(table->GetItemText(*table->GetSelection(), 0) == "(pwd) Online test");
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Page>());
        type(Page::ID_Password, U"wrong");
        choose(Page::ID_Connect);
        await([] { return !page().GetCtrl<ctrlConnectionProgress>(Page::ID_Connection)->IsConnecting(); });
        BOOST_TEST(page().GetCtrl<ctrlText>(Page::ID_Status)->GetText()
                   == ClientErrorToStr(ClientError::WrongPassword));
        type(Page::ID_Password, U"secret");
        choose(Page::ID_Coop);
        choose(Page::ID_Connect);
        await([] { return desktopAs<dskGameLobby>() != nullptr; });
        BOOST_TEST(GAMECLIENT.IsCoopMember());
        BOOST_TEST(GAMECLIENT.GetPlayerId() == *hostSlot);
    });
}

BOOST_AUTO_TEST_CASE(LanExceptionCleanupReleasesDiscoverySocketForReentry)
{
    OnlineFixture fixture;
    bool reached = false;
    BOOST_CHECK_THROW(fixture.checked([&] {
        fixture.enterLan();
        fixture.advertise();
        BOOST_TEST_REQUIRE(OnlineFixture::lan().GetCtrl<ctrlTable>(dskFrontEndLan::ID_Servers)->GetNumRows() == 1u);
        reached = true;
        onlineCleanupProbe();
    }),
                      std::runtime_error);
    BOOST_TEST(reached);
    BOOST_TEST(fixture.cleaned);
    fixture.checked([&] {
        fixture.enterLan();
        fixture.advertise();
        BOOST_TEST(OnlineFixture::lan().GetCtrl<ctrlTable>(dskFrontEndLan::ID_Servers)->GetNumRows() == 1u);
    });
}

BOOST_FIXTURE_TEST_CASE(NetworkMapHostUsesTheFullScreenBrowserAndValidatedMaskedForm, JoinFixture)
{
    checkedJoin([&] {
        const auto folder = RTTRCONFIG.ExpandPath(s25::folders::mapsOwn);
        boost::filesystem::create_directories(folder);
        const auto map = folder / "Online.SWD";
        boost::filesystem::copy_file(mapPath(), map);
        enter(false);
        choose(Online::ID_Host);
        BOOST_TEST_REQUIRE(desktopAs<dskFrontEndHost>());
        choose(dskFrontEndHost::ID_Maps);
        BOOST_TEST_REQUIRE(desktopAs<dskFrontEndMaps>());
        choose(dskFrontEndMaps::ID_Category);
        pressN(pad, PadButton::DpadDown, 3);
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<dskFrontEndMaps>()->GetSelectedPath() == map);
        choose(dskFrontEndMaps::ID_Continue);
        auto* form = desktopAs<dskFrontEndHost>();
        BOOST_TEST_REQUIRE(form);
        type(dskFrontEndHost::ID_Name, U"");
        choose(dskFrontEndHost::ID_Start);
        BOOST_TEST(form->GetCtrl<ctrlText>(dskFrontEndHost::ID_Status)->GetText()
                   == _("Please enter a name for the game"));
        type(dskFrontEndHost::ID_Name, U"Sofa game");
        type(dskFrontEndHost::ID_Port, U"0");
        choose(dskFrontEndHost::ID_Start);
        BOOST_TEST(form->GetCtrl<ctrlText>(dskFrontEndHost::ID_Status)->GetText()
                   == _("Invalid port. The valid port-range is 1 to 65535!"));
        type(dskFrontEndHost::ID_Password, U"secret");
        BOOST_TEST(form->GetCtrl<ctrlEdit>(dskFrontEndHost::ID_Password)->GetCtrl<ctrlTextDeepening>(0)->GetText()
                   == "******");
        SETTINGS.proxy.type = ProxyType::Socks4;
        choose(dskFrontEndHost::ID_Start);
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
        SETTINGS.proxy.type = ProxyType::None;
        // A file removed while the hosting form is open must stay recoverable here.
        boost::filesystem::remove(map);
        type(dskFrontEndHost::ID_Port, U"2105");
        choose(dskFrontEndHost::ID_Start);
        BOOST_TEST(form->GetCtrl<ctrlText>(dskFrontEndHost::ID_Status)->GetText() == "Hosting of game not possible");
        BOOST_TEST(form->GetCtrl<ctrlButton>(dskFrontEndHost::ID_Start)->GetEnabled());
        boost::filesystem::copy_file(mapPath(), map);
        Socket reservation;
        BOOST_TEST_REQUIRE(reservation.Listen(0, false, false));
        sockaddr_in address{};
        socklen_t length = sizeof(address);
        BOOST_TEST_REQUIRE(getsockname(reservation.GetSocket(), reinterpret_cast<sockaddr*>(&address), &length) == 0);
        const auto portText = std::to_string(ntohs(address.sin_port));
        reservation.Close();
        type(dskFrontEndHost::ID_Port, std::u32string(portText.begin(), portText.end()));
        choose(dskFrontEndHost::ID_Start);
        // F7d: a network game of an ordinary map gets the full-screen party page, not the classic lobby.
        await([] { return desktopAs<dskParty>() != nullptr; });
        BOOST_TEST(desktopAs<dskParty>()->IsOnline());
        BOOST_TEST(GAMECLIENT.IsHost());
        BOOST_TEST(!GAMECLIENT.IsHostingCampaign());
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Config));
    });
}

BOOST_FIXTURE_TEST_CASE(NetworkPartyShowsRemoteCardsReadyChatAndHowToJoin, JoinFixture)
{
    checkedJoin([&] {
        const auto folder = RTTRCONFIG.ExpandPath(s25::folders::mapsOwn);
        boost::filesystem::create_directories(folder);
        boost::filesystem::copy_file(mapPath(), folder / "Online.SWD");
        enter(false);
        choose(Online::ID_Host);
        choose(dskFrontEndHost::ID_Maps);
        choose(dskFrontEndMaps::ID_Category);
        pressN(pad, PadButton::DpadDown, 3);
        press(pad, PadButton::A);
        frame();
        choose(dskFrontEndMaps::ID_Continue);
        BOOST_TEST_REQUIRE(desktopAs<dskFrontEndHost>());
        type(dskFrontEndHost::ID_Name, U"Sofa game");
        type(dskFrontEndHost::ID_Password, U"secret");
        Socket reservation;
        BOOST_TEST_REQUIRE(reservation.Listen(0, false, false));
        sockaddr_in address{};
        socklen_t length = sizeof(address);
        BOOST_TEST_REQUIRE(getsockname(reservation.GetSocket(), reinterpret_cast<sockaddr*>(&address), &length) == 0);
        port = ntohs(address.sin_port);
        reservation.Close();
        const auto portText = std::to_string(port);
        type(dskFrontEndHost::ID_Port, std::u32string(portText.begin(), portText.end()));
        choose(dskFrontEndHost::ID_Start);
        await([] { return desktopAs<dskParty>() != nullptr; });
        auto& party = *desktopAs<dskParty>();
        BOOST_TEST_REQUIRE(party.IsOnline());
        const auto lobby = GAMECLIENT.GetGameLobby();
        const unsigned me = GAMECLIENT.GetPlayerId();
        // How the others get in: the port is always named.
        const std::string joinText = party.GetCtrl<ctrlText>(dskParty::ID_Address)->GetText();
        BOOST_TEST(joinText.find(portText) != std::string::npos);
        // One card per tribe; the open ones wait for players and are not computer players (local F7c would be).
        BOOST_TEST_REQUIRE(lobby->getNumPlayers() >= 2u);
        unsigned remoteSlot = 0;
        for(unsigned id = 0; id < lobby->getNumPlayers(); ++id)
        {
            const auto* card = party.GetCtrl<ctrlLobbyPlayerCard>(dskParty::ID_Net + id);
            BOOST_TEST_REQUIRE(card);
            BOOST_TEST(card->IsReadOnly() == (id != me));
            if(id != me)
            {
                BOOST_TEST((lobby->getPlayer(id).ps == PlayerState::Free));
                BOOST_TEST(card->GetText() == _("Open: waiting for a player"));
                if(!remoteSlot)
                    remoteSlot = id;
            }
        }
        // The host's own card edits its colour through the server.
        BOOST_TEST(focusedId(0) == dskParty::ID_Start);
        focus(dskParty::ID_Net + me);
        const unsigned colorBefore = lobby->getPlayer(me).color;
        press(pad, PadButton::DpadRight);
        frame();
        await([&] { return lobby->getPlayer(me).color != colorBefore; });

        // A remote player joins through the real server.
        BOOST_TEST_REQUIRE(host.Connect("127.0.0.1", port, false));
        await([&] {
            receiveHost();
            return hostSlot.has_value();
        });
        send(GameMessage_Server_Type(ServerType::Direct, rttr::version::GetRevision()));
        await([&] {
            receiveHost();
            return hostVersion;
        });
        send(GameMessage_Server_Password("secret"));
        await([&] {
            receiveHost();
            return hostPassword;
        });
        send(GameMessage_Player_Name(0xFF, "Remote"));
        CompressedData data;
        unsigned checksum = 0;
        BOOST_TEST_REQUIRE(data.CompressFromFile(mapPath(), &checksum));
        send(GameMessage_Map_Checksum(checksum, 0));
        const unsigned remote = *hostSlot;
        await([&] { return lobby->getPlayer(remote).ps == PlayerState::Occupied; });
        await([&] { return lobby->getPlayer(remote).name == "Remote"; });
        const auto* remoteCard = party.GetCtrl<ctrlLobbyPlayerCard>(dskParty::ID_Net + remote);
        BOOST_TEST(remoteCard->IsReadOnly());
        BOOST_TEST(remoteCard->IsJoined());
        BOOST_TEST(remoteCard->GetText().find("Remote") == 0u);
        BOOST_TEST(remoteCard->GetText().find(_("not ready")) != std::string::npos);

        // Start waits for everybody to be ready, and says for whom.
        focus(dskParty::ID_Start);
        press(pad, PadButton::A);
        frame();
        auto* box = dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(box);
        BOOST_TEST(box->GetTitle() == _("Not ready"));
        press(pad, PadButton::A);
        frame();
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);

        // Chat both ways: the remote's line shows up, the host's line reaches the remote.
        send(GameMessage_Chat(0xFF, ChatDestination::All, "hello from afar"));
        await([&] { return party.GetCtrl<ctrlChat>(dskParty::ID_Chat)->GetNumMessages() > 0u; });
        type(dskParty::ID_ChatEdit, U"welcome");
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Return));
        await([&] {
            receiveHost();
            return std::find(chats.begin(), chats.end(), "welcome") != chats.end();
        });
        BOOST_TEST(party.GetCtrl<ctrlEdit>(dskParty::ID_ChatEdit)->GetText().empty());

        // Ready: the card says so and Start counts down; open slots are closed for the start.
        send(GameMessage_Player_Ready(0xFF, true));
        await([&] { return lobby->getPlayer(remote).isReady; });
        BOOST_TEST(remoteCard->GetText().find(_("not ready")) == std::string::npos);
        const auto linesBefore = party.GetCtrl<ctrlChat>(dskParty::ID_Chat)->GetNumMessages();
        focus(dskParty::ID_Start);
        press(pad, PadButton::A);
        frame();
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        await([&] { return party.GetCtrl<ctrlChat>(dskParty::ID_Chat)->GetNumMessages() > linesBefore; });
        for(unsigned id = 0; id < lobby->getNumPlayers(); ++id)
        {
            if(id != me && id != remote)
                await([&] { return lobby->getPlayer(id).ps == PlayerState::Locked; });
        }
        // The host may still stop it; the open slots open again for players who are on their way.
        BOOST_TEST(party.GetCtrl<ctrlTextButton>(dskParty::ID_Start)->GetText() == _("Cancel start"));
        press(pad, PadButton::A);
        frame();
        for(unsigned id = 0; id < lobby->getNumPlayers(); ++id)
        {
            if(id != me && id != remote)
                await([&] { return lobby->getPlayer(id).ps == PlayerState::Free; });
        }
        await([&] { return party.GetCtrl<ctrlTextButton>(dskParty::ID_Start)->GetText() == _("Start game"); });
        // Mouse: a click on a row of the own card changes that row.
        auto* ownCard = party.GetCtrl<ctrlLobbyPlayerCard>(dskParty::ID_Net + me);
        const auto nationBefore = lobby->getPlayer(me).nation;
        MouseCoords nationRow(ownCard->GetDrawPos() + DrawPoint(20, 22 + 16 + 8));
        WINDOWMANAGER.Msg_MouseMove(nationRow);
        nationRow.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(nationRow);
        nationRow.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(nationRow);
        await([&] { return lobby->getPlayer(me).nation != nationBefore; });
        // The host lets co-players in.
        BOOST_TEST(!GAMECLIENT.AreCoopMembersAllowed());
        click(*party.GetCtrl<Window>(dskParty::ID_Coop));
        await([] { return GAMECLIENT.AreCoopMembersAllowed(); });
        BOOST_TEST(party.GetCtrl<ctrlTextButton>(dskParty::ID_Coop)->GetText() == _("Co-players: allowed"));
        // A player who holds the game up can be removed: A on their card, then Yes.
        focus(dskParty::ID_Net + remote);
        press(pad, PadButton::A);
        frame();
        auto* removeBox = dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(removeBox);
        BOOST_TEST(removeBox->GetTitle() == _("Remove player"));
        click(*removeBox->GetCtrl<Window>(2)); // Yes (iwMsgbox ID_BT_0)
        await([&] { return lobby->getPlayer(remote).ps == PlayerState::Free; });
        (void)remoteSlot;
    });
}

BOOST_FIXTURE_TEST_CASE(LanHostSelectionBackAndPasswordCancellationDoNotStartAGame, OnlineFixture)
{
    checked([&] {
        enterLan();
        choose(dskFrontEndLan::ID_Host);
        BOOST_TEST_REQUIRE(desktopAs<dskFrontEndHost>());
        choose(dskFrontEndHost::ID_Campaigns);
        back();
        BOOST_TEST_REQUIRE(desktopAs<dskFrontEndHost>());
        BOOST_TEST(focusedId(0) == dskFrontEndHost::ID_Campaigns);
        choose(dskFrontEndHost::ID_Saves);
        back();
        BOOST_TEST_REQUIRE(desktopAs<dskFrontEndHost>());
        back();
        BOOST_TEST_REQUIRE(desktopAs<dskFrontEndLan>());
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
    });
}

BOOST_FIXTURE_TEST_CASE(ConnectionUsesFullPageStatusResizeAndAllAbortPathsWithoutLosingPrivateInputs, JoinFixture)
{
    checkedJoin([&] {
        Socket silent;
        BOOST_TEST_REQUIRE(silent.Listen(0, false, false));
        sockaddr_in address{};
        socklen_t length = sizeof(address);
        BOOST_TEST_REQUIRE(getsockname(silent.GetSocket(), reinterpret_cast<sockaddr*>(&address), &length) == 0);
        port = ntohs(address.sin_port);
        fill();
        for(unsigned input = 0; input < 6; ++input)
        {
            choose(Page::ID_Connect);
            auto* connection = page().GetCtrl<ctrlConnectionProgress>(Page::ID_Connection);
            BOOST_TEST_REQUIRE(connection->IsConnecting());
            BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
            BOOST_TEST((WINDOWMANAGER.GetCursor() == Cursor::Moon));
            BOOST_TEST(!page().GetCtrl<Window>(Page::ID_Password)->IsVisible());
            // Presentation callbacks exercise transfer percentages; the other cases run the real wire handshake.
            ClientInterface& updates = *connection;
            updates.CI_NextConnectState(ConnectState::ReceiveMap);
            updates.CI_MapPartReceived(3, 4);
            BOOST_TEST(connection->GetCtrl<ctrlPercent>(ctrlConnectionProgress::ID_Progress)->getPercentage() == 75u);
            updates.CI_MapPartReceived(UINT32_MAX, 1);
            updates.CI_MapPartReceived(1, 0);
            BOOST_TEST(connection->GetCtrl<ctrlPercent>(ctrlConnectionProgress::ID_Progress)->getPercentage() == 100u);
            for(const VideoMode size :
                {VideoMode(800, 480), VideoMode(800, 600), VideoMode(1024, 640), VideoMode(1280, 800)})
            {
                BOOST_TEST_REQUIRE(VIDEODRIVER.ResizeScreen(size, DisplayMode::Windowed));
                frame();
                BOOST_TEST((connection->GetBoundaryRect() == page().GetFrame().content));
                for(unsigned id = ctrlConnectionProgress::ID_Status; id <= ctrlConnectionProgress::ID_Abort; ++id)
                {
                    const auto rect = connection->GetCtrl<Window>(id)->GetBoundaryRect();
                    BOOST_TEST(rect.left >= 0);
                    BOOST_TEST(rect.right <= static_cast<int>(connection->GetSize().x));
                    BOOST_TEST(rect.top >= 0);
                    BOOST_TEST(rect.bottom <= static_cast<int>(connection->GetSize().y));
                }
            }
            Socket abandoned = silent.Accept();
            BOOST_TEST_REQUIRE(abandoned.isValid());
            if(input == 5)
            {
                WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
                frame();
                BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
                BOOST_TEST((WINDOWMANAGER.GetCursor() == Cursor::Hand));
                continue;
            }
            if(input == 0)
                back();
            else if(input == 1)
            {
                WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
                frame();
            } else if(input == 2)
                click(*page().GetCtrl<Window>(Page::ID_btBack));
            else if(input == 3)
                press(pad, PadButton::A);
            else
                click(*connection->GetCtrl<Window>(ctrlConnectionProgress::ID_Abort));
            BOOST_TEST_REQUIRE(desktopAs<Page>());
            BOOST_TEST(!connection->IsConnecting());
            BOOST_TEST((WINDOWMANAGER.GetCursor() == Cursor::Hand));
            BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
            BOOST_TEST(page().GetCtrl<ctrlEdit>(Page::ID_Password)->GetText() == "secret");
            BOOST_TEST(page().GetCtrl<ctrlEdit>(Page::ID_Password)->IsVisible());
            BOOST_TEST(page().GetCtrl<ctrlButton>(Page::ID_Connect)->GetEnabled());
        }
    });
}

BOOST_AUTO_TEST_SUITE_END()
