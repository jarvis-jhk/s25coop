// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Loader.h"
#include "LocalGameFixture.h"
#include "MenuPadFixture.h"
#include "RTTR_Version.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlCheck.h"
#include "controls/ctrlEdit.h"
#include "controls/ctrlText.h"
#include "controls/ctrlTextDeepening.h"
#include "desktops/dskFrontEndConnect.h"
#include "desktops/dskFrontEndOnline.h"
#include "desktops/dskGameLobby.h"
#include "desktops/dskHome.h"
#include "desktops/dskLAN.h"
#include "desktops/dskMultiPlayer.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "ingameWindows/iwConnecting.h"
#include "ingameWindows/iwControllerKeyboard.h"
#include "ingameWindows/iwDirectIPCreate.h"
#include "ingameWindows/iwLobbyConnect.h"
#include "ingameWindows/iwMsgbox.h"
#include "network/GameMessages.h"
#include "gameTypes/CompressedData.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include "s25util/MessageQueue.h"
#include "s25util/Socket.h"
#include <boost/test/unit_test.hpp>
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
        const auto count = page().GetCtrl<ctrlEdit>(id)->GetText().size();
        click(*page().GetCtrl<ctrlEdit>(id));
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::End));
        for(unsigned i = 0; i < count; ++i)
            WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Backspace));
        for(const auto character : text)
            WINDOWMANAGER.Msg_KeyDown(KeyEvent(character));
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
    void server(bool ipv6 = false)
    {
        bool started = false;
        for(unsigned i = 0; i < 10 && !started; ++i)
        {
            GAMESERVER.Stop();
            port = static_cast<uint16_t>(rttr::test::randomValue(1024, 49151));
            started = GAMESERVER.Start(CreateServerInfo(ServerType::Direct, port, "Online test", "secret", ipv6),
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
        }
    }
    void send(const GameMessage& message) { BOOST_TEST_REQUIRE(MessageQueue::sendMessage(host, message)); }
    void joinHost()
    {
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
                       == "Invalid port. The valid port-range is 1 to 65535!");
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
        BOOST_TEST_REQUIRE(dynamic_cast<iwDirectIPCreate*>(WINDOWMANAGER.GetTopMostWindow()));
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
        BOOST_TEST_REQUIRE(dynamic_cast<iwLobbyConnect*>(WINDOWMANAGER.GetTopMostWindow()));
        back();
        BOOST_TEST_REQUIRE(desktopAs<Online>());
        choose(Online::ID_Lan);
        BOOST_TEST_REQUIRE(desktopAs<dskLAN>());
        back();
        BOOST_TEST_REQUIRE(desktopAs<dskMultiPlayer>());
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
                   == "Invalid port. The valid port-range is 1 to 65535!");
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
        BOOST_TEST_REQUIRE(dynamic_cast<iwConnecting*>(WINDOWMANAGER.GetTopMostWindow()));
        BOOST_TEST(!page().GetCtrl<ctrlButton>(Page::ID_Connect)->GetEnabled());
        await([] { return WINDOWMANAGER.GetTopMostWindow() == nullptr; });
        BOOST_TEST(page().GetCtrl<ctrlText>(Page::ID_Status)->GetText()
                   == ClientErrorToStr(ClientError::WrongPassword));
        BOOST_TEST(page().GetCtrl<ctrlButton>(Page::ID_Connect)->GetEnabled());
        type(Page::ID_Password, U"secret");
        choose(Page::ID_Connect);
        await([] { return desktopAs<dskGameLobby>() != nullptr; });
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
        BOOST_TEST_REQUIRE(dynamic_cast<iwConnecting*>(WINDOWMANAGER.GetTopMostWindow()));
        back();
        BOOST_TEST_REQUIRE(dynamic_cast<iwConnecting*>(WINDOWMANAGER.GetTopMostWindow()));
        // The legacy handshake has custom close behavior: activate its Abort button.
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Page>());
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
        BOOST_TEST(page().GetCtrl<ctrlButton>(Page::ID_Connect)->GetEnabled());
        choose(Page::ID_Connect);
        await([] { return desktopAs<dskGameLobby>() != nullptr; });
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
            BOOST_TEST_REQUIRE(dynamic_cast<iwConnecting*>(WINDOWMANAGER.GetTopMostWindow()));
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

BOOST_AUTO_TEST_SUITE_END()
