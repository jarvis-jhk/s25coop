// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Loader.h"
#include "MenuPadFixture.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlEdit.h"
#include "controls/ctrlTable.h"
#include "desktops/dskLobby.h"
#include "desktops/dskMainMenu.h"
#include "desktops/dskMultiPlayer.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "ingameWindows/iwDirectIPCreate.h"
#include "ingameWindows/iwLobbyServerInfo.h"
#include "ingameWindows/iwMsgbox.h"
#include "ogl/glFont.h"
#include "liblobby/LobbyClient.h"
#include "liblobby/LobbyMessages.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include "s25util/Messages.h"
#include "s25util/Serializer.h"
#include <boost/asio.hpp>
#include <boost/endian/arithmetic.hpp>
#include <boost/test/unit_test.hpp>
#include <array>
#include <chrono>
#include <cstring>
#include <memory>
#include <vector>

namespace {
constexpr PadDeviceId pad = 98;
using Tcp = boost::asio::ip::tcp;

// Only the remote peer is a test double: the real singleton authenticates and reads its lists
// through TCP and the production message decoder. The listener binds exclusively to loopback.
class LoopbackLobby : public LobbyMessageInterface
{
    struct Header
    {
        boost::endian::little_uint16_t id;
        boost::endian::little_int32_t length;
    };
    static_assert(sizeof(Header) == 6);

    boost::asio::io_context io;
    Tcp::acceptor listener{io, Tcp::endpoint(boost::asio::ip::address_v4::loopback(), 0)};
    Tcp::socket socket{io};
    std::vector<unsigned char> received;

    void send(const Message& message)
    {
        Serializer body;
        message.Serialize(body);
        // MessageHandler's legacy header is little endian; Serializer's payload is big endian.
        Header header;
        header.id = message.getId();
        header.length = body.GetLength();
        const std::array<boost::asio::const_buffer, 2> buffers{boost::asio::buffer(&header, sizeof(header)),
                                                               boost::asio::buffer(body.GetData(), body.GetLength())};
        boost::asio::write(socket, buffers);
    }

public:
    using LobbyMessageInterface::OnNMSLobbyPlayerList;
    using LobbyMessageInterface::OnNMSLobbyServerInfo;
    using LobbyMessageInterface::OnNMSLobbyServerList;

    bool authenticated = false;
    bool closed = false;
    unsigned deadMessages = 0;
    LobbyServerInfo game;
    LobbyServerList games;
    LobbyPlayerList players;

    LoopbackLobby()
    {
        game.setId(1);
        game.setName("Controller test game");
        game.setHost("127.0.0.1");
        game.setPort(1); // These tests never activate Connect.
        game.setMap("Test map");
        game.setVersion("Test version");
        game.setCurPlayers(1);
        game.setMaxPlayers(2);
        games.push_back(game);
        LobbyPlayerInfo player;
        player.setId(1);
        player.setName("Controller test player");
        player.setVersion("Test version");
        players.push_back(player);
    }

    uint16_t port() const { return listener.local_endpoint().port(); }

    void accept()
    {
        listener.accept(socket); // Login has already connected to this listener.
        socket.non_blocking(true);
        socket.set_option(Tcp::no_delay(true));
        send(LobbyMessage_Id(1));
    }

    void pump()
    {
        if(closed)
            return;
        // Small reads deliberately exercise partial headers and bodies in the peer.
        std::array<unsigned char, 4> buffer{};
        boost::system::error_code error;
        const auto count = socket.read_some(boost::asio::buffer(buffer), error);
        if(error == boost::asio::error::eof || error == boost::asio::error::connection_reset)
            closed = true;
        else
            BOOST_TEST_REQUIRE(
              (!error || error == boost::asio::error::would_block || error == boost::asio::error::try_again));
        received.insert(received.end(), buffer.begin(), buffer.begin() + count);
        while(received.size() >= 6u)
        {
            Header header;
            std::memcpy(&header, received.data(), sizeof(header));
            const uint16_t id = header.id;
            const int32_t length = header.length;
            BOOST_TEST_REQUIRE(length >= 0);
            BOOST_TEST_REQUIRE(length <= 65536);
            const auto packetLength = 6u + static_cast<unsigned>(length);
            if(received.size() < packetLength)
                break;
            std::unique_ptr<Message> message(LobbyMessage::create_lobby(id));
            BOOST_TEST_REQUIRE(message.get() != nullptr);
            Serializer body(received.data() + 6, length);
            message->Deserialize(body);
            BOOST_TEST_REQUIRE(message->run(this, 0));
            received.erase(received.begin(), received.begin() + packetLength);
        }
    }

    bool OnNMSLobbyLogin(unsigned, unsigned, const std::string& user, const std::string&, const std::string&) override
    {
        BOOST_TEST(user == "Controller test");
        authenticated = true;
        send(LobbyMessage_Login_Done("player@example.invalid"));
        send(LobbyMessage_ServerList(games));
        send(LobbyMessage_PlayerList(players));
        return true;
    }
    bool OnNMSLobbyServerList(unsigned) override
    {
        send(LobbyMessage_ServerList(games));
        return true;
    }
    bool OnNMSLobbyPlayerList(unsigned) override
    {
        send(LobbyMessage_PlayerList(players));
        return true;
    }
    bool OnNMSLobbyServerInfo(unsigned, const unsigned& id) override
    {
        BOOST_TEST(id == game.getId());
        send(LobbyMessage_ServerInfo(game));
        return true;
    }
    bool OnNMSDead(unsigned) override
    {
        ++deadMessages;
        return true;
    }
};

struct LobbyReturnFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder userData;
    rttr::test::ConfigOverride userDataOverride{"USERDATA", userData};
    LobbyClient& client = LOBBYCLIENT;
    ProxyType& proxyType = SETTINGS.proxy.type;
    const ProxyType oldProxy = proxyType;
    std::unique_ptr<LoopbackLobby> peer;

    LobbyReturnFixture() { proxyType = ProxyType::None; }
    ~LobbyReturnFixture() override { proxyType = oldProxy; }

    void frame() override
    {
        client.Run();
        peer->pump();
        client.Run();
        MenuPadFixture::frame();
        peer->pump();
        client.Run();
    }

    void enter()
    {
        client.Stop(); // Cleanup runs here and explicitly in tests, not in a destructor.
        peer = std::make_unique<LoopbackLobby>();
        BOOST_TEST_REQUIRE(client.Login("127.0.0.1", peer->port(), "Controller test", "test password", false));
        peer->accept();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while(!client.IsLoggedIn() || client.GetServerList().empty() || client.GetPlayerList().empty())
        {
            BOOST_TEST_REQUIRE((std::chrono::steady_clock::now() < deadline));
            client.Run();
            peer->pump();
        }
        BOOST_TEST_REQUIRE(peer->authenticated);
        padInput().Reset();
        WINDOWMANAGER.Switch(std::make_unique<dskLobby>());
        frame();
        pickUp(pad);
        BOOST_TEST_REQUIRE(desktopAs<dskLobby>() != nullptr);
        BOOST_TEST_REQUIRE((router().GetSlot(pad) == 0u));
    }

    void focusUntil(const Window* target)
    {
        for(unsigned i = 0; i < 25 && focused(0) != target; ++i)
            press(pad, PadButton::RightShoulder);
        BOOST_TEST_REQUIRE(focused(0) == target);
    }

    void focusAtY(const int y)
    {
        for(const auto* button : desktop()->GetCtrls<ctrlButton>())
        {
            if(button->GetPos().y == y)
            {
                focusUntil(button);
                return;
            }
        }
        BOOST_FAIL("Expected lobby button missing"); // LCOV_EXCL_LINE
    }

    void expectLobby()
    {
        BOOST_TEST_REQUIRE(desktopAs<dskLobby>() != nullptr);
        BOOST_TEST(client.IsLoggedIn());
        BOOST_TEST(!peer->closed);
        BOOST_TEST(peer->deadMessages == 0u);
    }

    void expectDisconnectedMultiplayer()
    {
        BOOST_TEST_REQUIRE(desktopAs<dskMultiPlayer>() != nullptr);
        BOOST_TEST(!client.IsLoggedIn());
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while(!peer->closed)
        {
            BOOST_TEST_REQUIRE((std::chrono::steady_clock::now() < deadline));
            frame();
        }
        // Stop may reset TCP when unread list responses are still arriving.
        BOOST_TEST(peer->deadMessages <= 1u);
        frame();
        BOOST_TEST(desktopAs<dskMultiPlayer>() != nullptr);
        BOOST_TEST(video.padEvents_.empty());
    }

    void finish() { client.Stop(); }
};
} // namespace

BOOST_AUTO_TEST_SUITE(MenuPadLobbyReturnTests)

BOOST_FIXTURE_TEST_CASE(BDisconnectsFromEveryFocusedLobbyControl, LobbyReturnFixture)
{
    enter();
    std::vector<unsigned> ids;
    for(const auto* button : desktop()->GetCtrls<ctrlButton>())
        ids.push_back(button->GetID());
    for(const auto* table : desktop()->GetCtrls<ctrlTable>())
    {
        BOOST_TEST_REQUIRE(table->GetNumRows() == 1u);
        ids.push_back(table->GetID());
    }
    BOOST_TEST_REQUIRE(ids.size() == 5u);
    for(const auto id : ids)
    {
        enter();
        focusUntil(desktop()->GetCtrl<Window>(id));
        press(pad, PadButton::B);
        expectDisconnectedMultiplayer();
    }
    finish();
}

BOOST_FIXTURE_TEST_CASE(BDoesNotBypassCreateGameOrProxyConfirmation, LobbyReturnFixture)
{
    for(const bool proxy : {false, true})
    {
        enter();
        proxyType = proxy ? ProxyType::Socks5 : ProxyType::None;
        focusAtY(440);
        press(pad, PadButton::A);
        auto* overlay = WINDOWMANAGER.GetTopMostWindow();
        BOOST_TEST_REQUIRE(overlay != nullptr);
        if(proxy)
            BOOST_TEST_REQUIRE(dynamic_cast<iwMsgbox*>(overlay) != nullptr);
        else
            BOOST_TEST_REQUIRE(dynamic_cast<iwDirectIPCreate*>(overlay) != nullptr);
        press(pad, PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == overlay);
        expectLobby();
        if(!proxy)
            focusUntil(overlay->GetCtrl<ctrlButton>(8));
        press(pad, PadButton::A);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        expectLobby();
        press(pad, PadButton::B);
        expectDisconnectedMultiplayer();
    }
    finish();
}

BOOST_FIXTURE_TEST_CASE(ServerInfoClosesBeforeTheLobbyDisconnects, LobbyReturnFixture)
{
    for(const bool keyboard : {false, true})
    {
        enter();
        const auto tables = desktop()->GetCtrls<ctrlTable>();
        const auto* games = tables.front();
        MouseCoords mc(games->GetDrawPos() + Position(25, NormalFont->getHeight() + 10 + NormalFont->getHeight() / 2));
        mc.rdown = true;
        WINDOWMANAGER.Msg_RightDown(mc);
        mc.rdown = false;
        WINDOWMANAGER.Msg_RightUp(mc);
        frame();
        BOOST_TEST_REQUIRE(dynamic_cast<iwLobbyServerInfo*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        if(keyboard)
        {
            WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
            frame();
        } else
            press(pad, PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        expectLobby();
        press(pad, PadButton::B);
        expectDisconnectedMultiplayer();
    }
    finish();
}

BOOST_FIXTURE_TEST_CASE(MouseAndControllerABackAlsoDisconnect, LobbyReturnFixture)
{
    for(const bool mouse : {false, true})
    {
        enter();
        if(mouse)
        {
            MouseCoords mc(Position(650, 540));
            mc.ldown = true;
            WINDOWMANAGER.Msg_LeftDown(mc);
            mc.ldown = false;
            WINDOWMANAGER.Msg_LeftUp(mc);
            frame();
        } else
        {
            focusAtY(530);
            press(pad, PadButton::A);
        }
        expectDisconnectedMultiplayer();
    }
    finish();
}

BOOST_FIXTURE_TEST_CASE(StartAndNavigationRetainTheLobbyAndEditedChat, LobbyReturnFixture)
{
    enter();
    const auto edits = desktop()->GetCtrls<ctrlEdit>();
    BOOST_TEST_REQUIRE(edits.size() == 1u);
    WINDOWMANAGER.Msg_KeyDown(KeyEvent('x'));
    WINDOWMANAGER.Msg_KeyDown(KeyEvent('y'));
    frame();
    BOOST_TEST_REQUIRE(edits.front()->GetText() == "xy");
    for(const auto button : {PadButton::Start, PadButton::RightShoulder, PadButton::DpadDown})
    {
        press(pad, button);
        expectLobby();
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(edits.front()->GetText() == "xy");
    }
    press(pad, PadButton::B);
    expectDisconnectedMultiplayer();
    finish();
}

BOOST_FIXTURE_TEST_CASE(BackBurstStopsAtMultiplayerBeforeANewPress, LobbyReturnFixture)
{
    enter();
    tap(pad, PadButton::B);
    tap(pad, PadButton::B);
    frame();
    expectDisconnectedMultiplayer();
    press(pad, PadButton::B);
    BOOST_TEST_REQUIRE(desktopAs<dskMainMenu>() != nullptr);
    BOOST_TEST(!client.IsLoggedIn());
    // Stop may reset TCP when unread list responses are still arriving.
    BOOST_TEST(peer->deadMessages <= 1u);
    finish();
}

BOOST_AUTO_TEST_SUITE_END()
