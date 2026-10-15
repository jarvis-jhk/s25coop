// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Loader.h"
#include "MenuPadFixture.h"
#include "RTTR_Version.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlEdit.h"
#include "controls/ctrlTable.h"
#include "controls/ctrlText.h"
#include "controls/ctrlTextDeepening.h"
#include "desktops/dskFrontEndConnect.h"
#include "desktops/dskFrontEndHost.h"
#include "desktops/dskFrontEndInternet.h"
#include "desktops/dskFrontEndOnline.h"
#include "desktops/dskHome.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "ingameWindows/iwControllerKeyboard.h"
#include "liblobby/LobbyClient.h"
#include "liblobby/LobbyMessages.h"
#include "libsiedler2/Archiv.h"
#include "libsiedler2/ArchivItem_Text.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include "rttr/test/random.hpp"
#include "s25util/SocketSet.h"
#include "s25util/md5.hpp"
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <csignal>
#include <thread>

namespace {
using Page = dskFrontEndInternet;
constexpr PadDeviceId pad = 244;

#ifndef _WIN32
struct LobbySignals
{
    struct sigaction previous
    {};
    LobbySignals()
    {
        // Match the game's no-op SIGPIPE handler: a disconnected send must return EPIPE, not kill the test.
        struct sigaction ignore
        {};
        ignore.sa_handler = SIG_IGN;
        sigemptyset(&ignore.sa_mask);
        BOOST_TEST_REQUIRE(sigaction(SIGPIPE, &ignore, &previous) == 0);
    }
    ~LobbySignals() { BOOST_TEST(sigaction(SIGPIPE, &previous, nullptr) == 0); }
};
#endif

struct InternetFixture : rttr::test::MenuPadFixture, LobbyMessageInterface
{
    rttr::test::TmpFolder userData;
    rttr::test::ConfigOverride userOverride{"USERDATA", userData};
    rttr::test::ConfigOverride gameOverride{"GAME", userData};
    const decltype(SETTINGS.lobby) oldLobby = SETTINGS.lobby;
    const decltype(SETTINGS.server) oldServer = SETTINGS.server;
    const decltype(SETTINGS.proxy) oldProxy = SETTINGS.proxy;
    const VideoMode oldSize = VIDEODRIVER.GetWindowSize();
    const DisplayMode oldMode = VIDEODRIVER.GetDisplayMode();
    const unsigned oldScale = VIDEODRIVER.getGuiScale().percent();
    const unsigned oldReference = VIDEODRIVER.getUiReferenceHeight();
    libsiedler2::Archiv oldClient;
    Socket listener, peer;
    MessageQueue received{LobbyMessage::create_lobby};
    unsigned loginCount = 0;
    bool reject = false;
    std::string loginUser, loginHash, chat;
#ifndef _WIN32
    LobbySignals signals;
#endif

    InternetFixture()
    {
        LOBBYCLIENT.Stop();
        SETTINGS.lobby.name = "fixture";
        SETTINGS.lobby.password.clear();
        SETTINGS.lobby.savePassword = false;
        SETTINGS.server.ipv6 = false;
        SETTINGS.proxy.type = ProxyType::None;
        LOADER.LoadDummyLanguageFiles();
        bool started = false;
        uint16_t port = 0;
        for(unsigned i = 0; i < 10 && !started; ++i)
        {
            port = static_cast<uint16_t>(rttr::test::randomValue(1024, 49151));
            started = listener.Listen(port, false, false);
        }
        BOOST_TEST_REQUIRE(started);
        // Replace only the shipped lobby endpoint resource; the product Login and protocol stay real.
        LOADER.GetTextN("client", 0);
        auto& archive = LOADER.GetArchive("client");
        oldClient = std::move(archive);
        archive.alloc(2);
        auto host = std::make_unique<libsiedler2::ArchivItem_Text>();
        host->setText("127.0.0.1");
        archive.set(0, std::move(host));
        auto portText = std::make_unique<libsiedler2::ArchivItem_Text>();
        portText->setText(std::to_string(port));
        archive.set(1, std::move(portText));
    }
    ~InternetFixture() override
    {
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        rttr::test::MenuPadFixture::frame();
        LOBBYCLIENT.Stop();
        LOADER.GetArchive("client") = std::move(oldClient);
        SETTINGS.lobby = oldLobby;
        SETTINGS.server = oldServer;
        SETTINGS.proxy = oldProxy;
        VIDEODRIVER.setUiReferenceHeight(oldReference);
        VIDEODRIVER.setGuiScalePercent(oldScale);
        VIDEODRIVER.ResizeScreen(oldSize, oldMode);
        dskHome::ForgetLastChoice();
    }
    static Page& page()
    {
        auto* result = desktopAs<Page>();
        BOOST_TEST_REQUIRE(result);
        return *result;
    }
    void frame() override
    {
        if(!peer.isValid())
        {
            SocketSet ready;
            ready.Add(listener);
            if(ready.Select(0, 0) > 0)
                peer = listener.Accept();
            if(peer.isValid())
                send(LobbyMessage_Id(1));
        }
        if(peer.isValid())
        {
            received.recvAll(peer);
            while(!received.empty())
                received.pop()->run(this, 0);
        }
        LOBBYCLIENT.Run();
        rttr::test::MenuPadFixture::frame();
    }
    template<class Predicate>
    void until(const Predicate& ready)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while(!ready() && std::chrono::steady_clock::now() < deadline)
        {
            frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        BOOST_TEST_REQUIRE(ready());
    }
    void send(const Message& message) { BOOST_TEST_REQUIRE(MessageQueue::sendMessage(peer, message)); }
    bool OnNMSLobbyLogin(unsigned, unsigned, const std::string& user, const std::string& hash,
                         const std::string&) override
    {
        ++loginCount;
        loginUser = user;
        loginHash = hash;
        if(reject)
            send(LobbyMessage_Login_Error("Wrong login"));
        else
            send(LobbyMessage_Login_Done(""));
        return true;
    }
    bool OnNMSLobbyChat(unsigned, const std::string&, const std::string& text) override
    {
        chat = text;
        send(LobbyMessage_Chat("fixture", text));
        return true;
    }
    void enter()
    {
        WINDOWMANAGER.Switch(dskHome::Create());
        frame();
        pickUp(pad);
        choose(dskHome::ID_Online);
        choose(dskFrontEndOnline::ID_Internet);
        BOOST_TEST_REQUIRE(desktopAs<Page>());
    }
    void focus(unsigned id)
    {
        for(unsigned i = 0; i < 40 && focusedId(0) != id; ++i)
            press(pad, focusedId(0) < id ? PadButton::RightShoulder : PadButton::LeftShoulder);
        BOOST_TEST_REQUIRE(focusedId(0) == id);
    }
    void choose(unsigned id)
    {
        focus(id);
        press(pad, PadButton::A);
        frame();
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
    void type(unsigned id, const std::u32string& value)
    {
        // Every field this fixture types into starts empty; typing only appends.
        BOOST_TEST_REQUIRE(page().GetCtrl<ctrlEdit>(id)->GetText().empty());
        click(*page().GetCtrl<ctrlEdit>(id));
        for(const auto c : value)
            WINDOWMANAGER.Msg_KeyDown(KeyEvent(c));
    }
    void login()
    {
        enter();
        type(Page::ID_Password, U"secret");
        choose(Page::ID_Login);
        until([] { return LOBBYCLIENT.IsLoggedIn(); });
        BOOST_TEST(page().GetCtrl<ctrlEdit>(Page::ID_Password)->GetText().empty());
        BOOST_TEST(page().GetCtrl<Window>(Page::ID_Servers)->IsVisible());
    }
    static LobbyServerInfo server(unsigned id, const std::string& host = "127.0.0.1", unsigned short port = 2105)
    {
        LobbyServerInfo info;
        info.setId(id);
        info.setName("Game " + std::to_string(id));
        info.setHost(host);
        info.setPort(port);
        info.setMap("Fixture map");
        info.setVersion(rttr::version::GetReadableVersion());
        info.setCurPlayers(1);
        info.setMaxPlayers(4);
        return info;
    }
    void list(std::initializer_list<LobbyServerInfo> values)
    {
        LobbyServerList servers;
        for(const auto& value : values)
            servers.push_back(value);
        send(LobbyMessage_ServerList(servers));
        // The list can be unchanged in size, so wait for receipt through the real backend rather than a UI count.
        until([&] {
            const auto& actual = LOBBYCLIENT.GetServerList();
            return actual.size() == servers.size()
                   && std::equal(actual.begin(), actual.end(), servers.begin(), [](const auto& a, const auto& b) {
                          return a.getId() == b.getId() && a.getName() == b.getName() && a.getHost() == b.getHost()
                                 && a.getPort() == b.getPort() && a.getVersion() == b.getVersion();
                      });
        });
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(FrontEndInternetTests)

BOOST_FIXTURE_TEST_CASE(LoginFieldsKeyboardMaskValidationAndBack, InternetFixture)
{
    enter();
    choose(Page::ID_Login);
    BOOST_TEST(page().GetCtrl<ctrlText>(Page::ID_Status)->GetText() == "Username and password are required.");
    focus(Page::ID_Password);
    press(pad, PadButton::A);
    auto* keyboard = dynamic_cast<iwControllerKeyboard*>(WINDOWMANAGER.GetTopMostWindow());
    BOOST_TEST_REQUIRE(keyboard);
    press(pad, PadButton::A);
    BOOST_TEST(keyboard->GetCtrl<ctrlEdit>(0)->GetCtrl<ctrlTextDeepening>(0)->GetText() == "*");
    press(pad, PadButton::Start);
    frame();
    BOOST_TEST(page().GetCtrl<ctrlEdit>(Page::ID_Password)->GetText() == "1");
    BOOST_TEST(page().GetCtrl<ctrlEdit>(Page::ID_Password)->GetCtrl<ctrlTextDeepening>(0)->GetText() == "*");
    choose(Page::ID_Register);
    BOOST_TEST(!page().GetCtrl<ctrlText>(Page::ID_Status)->GetText().empty());
    press(pad, PadButton::B);
    frame();
    BOOST_TEST_REQUIRE(desktopAs<dskFrontEndOnline>());
    choose(dskFrontEndOnline::ID_Internet);
    BOOST_TEST(page().GetCtrl<ctrlEdit>(Page::ID_Password)->GetText().empty());
}

BOOST_FIXTURE_TEST_CASE(RealLoginHashRememberAndChatRoundtrip, InternetFixture)
{
    enter();
    type(Page::ID_Password, U"secret");
    choose(Page::ID_SavePassword);
    choose(Page::ID_Login);
    until([] { return LOBBYCLIENT.IsLoggedIn(); });
    BOOST_TEST(loginCount == 1u);
    BOOST_TEST(loginUser == "fixture");
    BOOST_TEST(loginHash == s25util::md5("secret").toString());
    BOOST_TEST(SETTINGS.lobby.password == "md5:" + loginHash);
    BOOST_TEST(page().GetCtrl<ctrlEdit>(Page::ID_Password)->GetText().empty());
    type(Page::ID_Message, U"hello lobby");
    choose(Page::ID_Send);
    until([&] { return chat == "hello lobby"; });
    BOOST_TEST(page().GetCtrl<ctrlEdit>(Page::ID_Message)->GetText().empty());
    // Leaving the browser keeps a live session; rebuilding it must not expose the saved hash.
    press(pad, PadButton::B);
    frame();
    choose(dskFrontEndOnline::ID_Internet);
    BOOST_TEST(page().GetCtrl<Window>(Page::ID_Servers)->IsVisible());
    choose(Page::ID_Logout);
    BOOST_TEST(!LOBBYCLIENT.IsLoggedIn());
    BOOST_TEST(page().GetCtrl<Window>(Page::ID_User)->IsVisible());
}

BOOST_FIXTURE_TEST_CASE(RejectedLoginCanRetryWithoutLosingInputsOrDoubleHashing, InternetFixture)
{
    SETTINGS.lobby.password = "md5:" + s25util::md5("secret").toString();
    SETTINGS.lobby.savePassword = true;
    enter();
    reject = true;
    choose(Page::ID_SavePassword);
    choose(Page::ID_Login);
    until([&] { return page().GetCtrl<ctrlText>(Page::ID_Status)->GetText() == "Wrong login"; });
    BOOST_TEST(page().GetCtrl<ctrlButton>(Page::ID_Login)->GetEnabled());
    BOOST_TEST(loginHash == s25util::md5("secret").toString());
    BOOST_TEST(!page().GetCtrl<ctrlEdit>(Page::ID_Password)->GetText().empty());
    peer.Close();
    received.clear();
    reject = false;
    choose(Page::ID_Login);
    until([] { return LOBBYCLIENT.IsLoggedIn(); });
    BOOST_TEST(loginCount == 2u);
    BOOST_TEST(SETTINGS.lobby.password.empty());
    BOOST_TEST(loginHash == s25util::md5("secret").toString());
}

BOOST_FIXTURE_TEST_CASE(ServerIdentitySurvivesReorderButNotEndpointReplacementOrRemoval, InternetFixture)
{
    login();
    list({server(1), server(2)});
    focus(Page::ID_Servers);
    press(pad, PadButton::DpadDown);
    auto* table = page().GetCtrl<ctrlTable>(Page::ID_Servers);
    BOOST_TEST_REQUIRE(table->GetSelection().has_value());
    BOOST_TEST(table->GetItemText(*table->GetSelection(), 0) == "Game 2");
    list({server(2), server(1)});
    BOOST_TEST_REQUIRE(table->GetSelection().has_value());
    BOOST_TEST(table->GetItemText(*table->GetSelection(), 0) == "Game 2");
    list({server(2, "127.0.0.1", 2210), server(1)});
    BOOST_TEST(!table->GetSelection());
    BOOST_TEST(!page().GetCtrl<ctrlButton>(Page::ID_Join)->GetEnabled());
    list({});
    BOOST_TEST(table->GetNumRows() == 0u);
    BOOST_TEST(!page().GetCtrl<ctrlButton>(Page::ID_Join)->GetEnabled());
    press(pad, PadButton::B);
    frame();
    choose(dskFrontEndOnline::ID_Internet);
    list({server(3)});
    choose(Page::ID_Join);
    auto* join = desktopAs<dskFrontEndConnect>();
    BOOST_TEST_REQUIRE(join);
    BOOST_TEST(join->GetCtrl<ctrlEdit>(dskFrontEndConnect::ID_Host)->GetText() == "127.0.0.1");
    BOOST_TEST(join->GetCtrl<ctrlEdit>(dskFrontEndConnect::ID_Port)->GetText() == "2105");
    press(pad, PadButton::B);
    frame();
    BOOST_TEST_REQUIRE(desktopAs<Page>());
    auto incompatible = server(3);
    incompatible.setVersion("Other version");
    list({incompatible});
    BOOST_TEST(!page().GetCtrl<ctrlButton>(Page::ID_Join)->GetEnabled());
    choose(Page::ID_Servers);
    BOOST_TEST_REQUIRE(desktopAs<Page>());
    BOOST_TEST(page().GetCtrl<ctrlText>(Page::ID_Status)->GetText() == "You can't join that game with your version!");
}

BOOST_FIXTURE_TEST_CASE(BrowserResizeMouseAndConnectionLossKeepRecoveryVisible, InternetFixture)
{
    login();
    list({server(1)});
    for(const VideoMode size : {VideoMode(800, 480), VideoMode(800, 600), VideoMode(1024, 640), VideoMode(1280, 800)})
    {
        BOOST_TEST_REQUIRE(VIDEODRIVER.ResizeScreen(size, DisplayMode::Windowed));
        frame();
        const auto area = page().GetFrame().content;
        for(const unsigned id : {Page::ID_Servers, Page::ID_Join, Page::ID_Refresh, Page::ID_Host, Page::ID_Logout,
                                 Page::ID_Chat, Page::ID_Message, Page::ID_Send})
        {
            const auto rect = page().GetCtrl<Window>(id)->GetBoundaryRect();
            BOOST_TEST(rect.left >= area.left);
            BOOST_TEST(rect.right <= area.right);
            BOOST_TEST(rect.top >= area.top);
            BOOST_TEST(rect.bottom <= area.bottom);
        }
    }
    click(*page().GetCtrl<Window>(Page::ID_Join));
    BOOST_TEST_REQUIRE(desktopAs<dskFrontEndConnect>());
    WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
    frame();
    BOOST_TEST_REQUIRE(desktopAs<Page>());
    SETTINGS.proxy.type = ProxyType::Socks4;
    choose(Page::ID_Host);
    BOOST_TEST_REQUIRE(desktopAs<Page>());
    BOOST_TEST(!page().GetCtrl<ctrlText>(Page::ID_Status)->GetText().empty());
    SETTINGS.proxy.type = ProxyType::None;
    choose(Page::ID_Host);
    BOOST_TEST_REQUIRE(desktopAs<dskFrontEndHost>());
    press(pad, PadButton::B);
    frame();
    BOOST_TEST_REQUIRE(desktopAs<Page>());
    peer.Close();
    until([] { return !LOBBYCLIENT.IsLoggedIn(); });
    BOOST_TEST(page().GetCtrl<Window>(Page::ID_User)->IsVisible());
    BOOST_TEST(page().GetCtrl<ctrlButton>(Page::ID_Login)->GetEnabled());
    BOOST_TEST(page().GetCtrl<ctrlText>(Page::ID_Status)->GetText() == "Lost connection to lobby!");
}

BOOST_AUTO_TEST_SUITE_END()
