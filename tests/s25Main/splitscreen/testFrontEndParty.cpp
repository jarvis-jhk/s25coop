// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Game.h"
#include "GameLobby.h"
#include "GlobalGameSettings.h"
#include "JoinPlayerInfo.h"
#include "LocalGameFixture.h"
#include "MenuPadFixture.h"
#include "PadFixture.h"
#include "controls/ctrlLobbyPlayerCard.h"
#include "controls/ctrlText.h"
#include "controls/ctrlTextButton.h"
#include "desktops/dskFrontEndCampaigns.h"
#include "desktops/dskFrontEndMaps.h"
#include "desktops/dskFrontEndMissions.h"
#include "desktops/dskGameLoader.h"
#include "desktops/dskHome.h"
#include "desktops/dskParty.h"
#include "desktops/dskTitle.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "frontend/MenuRoutes.h"
#include "ingameWindows/iwAddons.h"
#include "ingameWindows/iwConnecting.h"
#include "ingameWindows/iwMsgbox.h"
#include "postSystem/PostBox.h"
#include "postSystem/PostManager.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include "rttr/test/random.hpp"
#include "s25util/Socket.h"
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <memory>

// The full-screen party page (F7b) for campaign starts from the new menus, through the real title join,
// mission page, local server and client.

namespace {
constexpr PadDeviceId pad = 71;

[[noreturn]] void cleanupProbe()
{
    throw std::runtime_error("deliberate party-page cleanup probe");
}

struct PartyFixture : rttr::test::LocalGameFixture, rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder data;
    rttr::test::ConfigOverride userOverride{"USERDATA", data / "user"};
    const boost::filesystem::path originalWorld = RTTRCONFIG.ExpandPath(s25::folders::gamedata) / "world";
    rttr::test::ConfigOverride rttrOverride{"RTTR", data / "rttr"};
    const CampaignSettings savedCampaigns = SETTINGS.campaigns;
    const uint16_t savedPort = SETTINGS.server.localPort;
    const frontend::MenuStyle savedStyle = frontend::GetMenuStyle();

    PartyFixture()
    {
        const auto world = RTTRCONFIG.ExpandPath(s25::folders::gamedata) / "world";
        boost::filesystem::create_directories(world);
        for(boost::filesystem::directory_iterator it(originalWorld), end; it != end; ++it)
            boost::filesystem::copy_file(it->path(), world / it->path().filename());
        const auto folder = RTTRCONFIG.ExpandPath(s25::folders::campaignsUser) / "Party";
        boost::filesystem::create_directories(folder);
        boost::filesystem::copy_file(rttr::test::rttrBaseDir / "tests" / "testData" / "maps" / "LuaFunctions.SWD",
                                     folder / "Mission1.SWD");
        boost::nowide::ofstream(folder / "Mission1.lua")
          << "function getRequiredLuaVersion() return 1 end\nfunction onStart(first) rttr:SetMissionGoal(0,'M1') end\n";
        boost::nowide::ofstream(folder / "campaign.lua")
          << "function getRequiredLuaVersion() return 1 end\ncampaign={uid='f7b-party',version=1,author='A',"
             "name='Party',shortDescription='Coop',longDescription='D',maxHumanPlayers=1,difficulty='easy',"
             "chaptersEnabled={0},maps={'Mission1.SWD'}}\n";
        boost::filesystem::create_directories(RTTRCONFIG.ExpandPath(s25::folders::mapsPlayed));
        boost::filesystem::create_directories(RTTRCONFIG.ExpandPath(s25::folders::replays));
    }

    void frame() override
    {
        video.tickCount_ += frameMs;
        pump();
        WINDOWMANAGER.Draw();
    }
    template<class F>
    bool until(const F& done)
    {
        for(unsigned i = 0; i < 20000 && !done(); ++i)
        {
            frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return done();
    }
    void focus(unsigned slot, PadDeviceId device, unsigned id)
    {
        for(unsigned i = 0; i < 20 && focusedId(slot) != id; ++i)
            press(device, focusedId(slot) < id ? PadButton::RightShoulder : PadButton::LeftShoulder);
        BOOST_TEST_REQUIRE(focusedId(slot) == id);
    }
    void clickAt(const DrawPoint& pos)
    {
        MouseCoords mouse(pos);
        WINDOWMANAGER.Msg_MouseMove(mouse);
        mouse.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mouse);
        mouse.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mouse);
        frame();
    }
    void click(const Window& control) { clickAt(control.GetDrawPos() + Position(control.GetSize() / 2u)); }
    static iwMsgbox* question() { return dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow()); }
    static dskParty& party()
    {
        auto* page = desktopAs<dskParty>();
        BOOST_TEST_REQUIRE(page);
        return *page;
    }
    static const ctrlLobbyPlayerCard& card(unsigned seat)
    {
        const auto* card = party().GetCtrl<ctrlLobbyPlayerCard>(dskParty::ID_Seat + seat);
        BOOST_TEST_REQUIRE(card);
        return *card;
    }

    /// `count` controllers join on the title page, then the first starts the campaign's mission.
    void open(unsigned count)
    {
        WINDOWMANAGER.Switch(dskTitle::Create());
        frame();
        for(unsigned i = 0; i < count; ++i)
        {
            connect(pad + i);
            press(pad + i, PadButton::A);
        }
        BOOST_TEST_REQUIRE(padInput().GetParty().Members().size() == count);
        press(pad, PadButton::A);
        BOOST_TEST_REQUIRE(desktopAs<dskHome>());
        // The first attempt meets a busy port, so the retry below is exercised and not only a fallback.
        Socket occupied;
        BOOST_TEST_REQUIRE(occupied.Create(AF_INET6));
        const int disabled = 0;
        BOOST_TEST_REQUIRE(occupied.SetSockOpt(SO_REUSEADDR, &disabled, sizeof(disabled), SOL_SOCKET));
        BOOST_TEST_REQUIRE(occupied.SetSockOpt(IPV6_V6ONLY, &disabled, sizeof(disabled), IPPROTO_IPV6));
#ifdef _WIN32
        const int exclusive = 1;
        BOOST_TEST_REQUIRE(occupied.SetSockOpt(SO_EXCLUSIVEADDRUSE, &exclusive, sizeof(exclusive), SOL_SOCKET));
#endif
        BOOST_TEST_REQUIRE(occupied.Bind(0, true));
        BOOST_TEST_REQUIRE(listen(occupied.GetSocket(), 1) == 0);
        address_t address{};
        socklen_t length = sizeof(address);
        BOOST_TEST_REQUIRE(getsockname(occupied.GetSocket(), &address.sa, &length) == 0);
        bool ready = false;
        for(unsigned attempt = 0; attempt < 10 && !ready; ++attempt)
        {
            SETTINGS.server.localPort = attempt == 0 ? ntohs(address.sa_in6.sin6_port) :
                                                       static_cast<uint16_t>(rttr::test::randomValue(1024, 49151));
            focus(0, pad, dskHome::ID_Campaigns);
            press(pad, PadButton::A);
            BOOST_TEST_REQUIRE(desktopAs<dskFrontEndCampaigns>());
            press(pad, PadButton::A);
            BOOST_TEST_REQUIRE(desktopAs<dskFrontEndMissions>());
            tap(pad, PadButton::A);
            tap(pad, PadButton::A);
            frame();
            ready = until([] { return desktopAs<dskParty>() || question(); }) && desktopAs<dskParty>();
            if(attempt == 0)
                BOOST_TEST_REQUIRE(!ready);
            if(!ready)
            {
                occupied.Close();
                GAMECLIENT.Stop();
                GAMESERVER.Stop();
                GAMECLIENT.SetInterface(&ci());
                WINDOWMANAGER.Switch(dskHome::Create());
                frame();
            }
        }
        BOOST_TEST_REQUIRE(ready);
        BOOST_TEST_REQUIRE(until([count] { return GAMECLIENT.GetSharedLocalViews() == count - 1; }));
    }
    /// `count` controllers join on the title page, then the first starts an ordinary three-tribe map through
    /// Maps & scenarios.
    void openMap(unsigned count)
    {
        const auto maps = RTTRCONFIG.ExpandPath(s25::folders::mapsOwn);
        boost::filesystem::create_directories(maps);
        boost::filesystem::copy_file(rttr::test::rttrBaseDir / "tests" / "testData" / "maps" / "LuaFunctions.SWD",
                                     maps / "Three.SWD");
        WINDOWMANAGER.Switch(dskTitle::Create());
        frame();
        for(unsigned i = 0; i < count; ++i)
        {
            connect(pad + i);
            press(pad + i, PadButton::A);
        }
        BOOST_TEST_REQUIRE(padInput().GetParty().Members().size() == count);
        press(pad, PadButton::A);
        BOOST_TEST_REQUIRE(desktopAs<dskHome>());
        bool ready = false;
        for(unsigned attempt = 0; attempt < 10 && !ready; ++attempt)
        {
            SETTINGS.server.localPort = static_cast<uint16_t>(rttr::test::randomValue(1024, 49151));
            focus(0, pad, dskHome::ID_Maps);
            press(pad, PadButton::A);
            auto* maps = desktopAs<dskFrontEndMaps>();
            BOOST_TEST_REQUIRE(maps);
            BOOST_TEST_REQUIRE(maps->GetSelectedPath().filename() == "Three.SWD");
            click(*maps->GetCtrl<Window>(dskFrontEndMaps::ID_Continue));
            ready = until([] { return desktopAs<dskParty>() || question(); }) && desktopAs<dskParty>();
            if(!ready)
            {
                GAMECLIENT.Stop();
                GAMESERVER.Stop();
                GAMECLIENT.SetInterface(&ci());
                WINDOWMANAGER.Switch(dskHome::Create());
                frame();
            }
        }
        BOOST_TEST_REQUIRE(ready);
        BOOST_TEST_REQUIRE(GAMECLIENT.GetGameLobby()->getNumPlayers() == 3u);
    }
    /// The seats on own tribes are confirmed by the server: each guest's slot is its idle placeholder AI.
    static bool ownTribesConfirmed(unsigned guests)
    {
        const auto& ids = GAMECLIENT.GetAdditionalLocalPlayers();
        return ids.size() == guests && std::all_of(ids.begin(), ids.end(), [](const uint8_t id) {
                   const auto& player = GAMECLIENT.GetGameLobby()->getPlayer(id);
                   return player.ps == PlayerState::AI && player.aiInfo.type == AI::Type::Dummy;
               });
    }
    static const GlobalGameSettings& rules() { return party().GetRules(); }
    unsigned ruleFocus() const
    {
        const unsigned id = focusedId(0);
        BOOST_TEST_REQUIRE(id >= dskParty::ID_Rule);
        BOOST_TEST_REQUIRE(id <= dskParty::ID_RuleLast);
        return id - dskParty::ID_Rule;
    }
    /// Walk the drawer with the D-pad to `rule`.
    void focusRule(dskParty::Rule rule)
    {
        const auto target = static_cast<unsigned>(rule);
        for(unsigned i = 0; i < 12 && ruleFocus() != target; ++i)
            press(pad, ruleFocus() < target ? PadButton::DpadDown : PadButton::DpadUp);
        BOOST_TEST_REQUIRE(ruleFocus() == target);
    }
    void openRules()
    {
        focus(0, pad, dskParty::ID_Rules);
        press(pad, PadButton::A);
        BOOST_TEST_REQUIRE(party().IsRulesOpen());
    }
    /// The real start: loading and the game, with only the original-resource loading bypassed.
    void startedWith(unsigned views)
    {
        BOOST_TEST_REQUIRE(until([] { return GAMECLIENT.GetState() == ClientState::Loading; }));
        BOOST_TEST_REQUIRE(ci().game);
        const auto view = std::make_unique<rttr::test::TestableGameInterface>(ci().game, GAMECLIENT.GetNWFInfo(),
                                                                              GAMECLIENT.GetPlayerId(), false);
        GAMECLIENT.GameLoaded();
        BOOST_TEST_REQUIRE(until([] { return GAMECLIENT.GetState() == ClientState::Game; }));
        GAMECLIENT.OnGameStart();
        BOOST_TEST(ci().game->world_.GetPostMgr().GetPostBox(0)->GetCurrentMissionGoal() == "M1");
        BOOST_TEST_REQUIRE(view->GetNumViews() == views);
        for(unsigned i = 0; i < views; ++i)
            BOOST_TEST(view->GetPlayerView(i).GetPlayerId() == GAMECLIENT.GetPlayerId());
        GAMECLIENT.SetInterface(&ci());
        const unsigned gf = GAMECLIENT.GetGFNumber() + 12;
        BOOST_TEST_REQUIRE(until([gf] { return GAMECLIENT.GetGFNumber() >= gf; }));
        BOOST_TEST(ci().numErrors == 0u);
        BOOST_TEST(ci().numAsync == 0u);
    }
    void cleanup()
    {
        GAMECLIENT.Stop();
        GAMESERVER.Stop();
        GAMECLIENT.SetInterface(&ci());
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        frame();
        SETTINGS.campaigns = savedCampaigns;
        SETTINGS.server.localPort = savedPort;
        frontend::SetMenuStyle(savedStyle);
        dskHome::ForgetLastChoice();
        padInput().Reset();
        video.padEvents_.clear();
        BOOST_TEST((SETTINGS.campaigns.createSaveData() == savedCampaigns.createSaveData()));
        BOOST_TEST(SETTINGS.server.localPort == savedPort);
        BOOST_TEST((frontend::GetMenuStyle() == savedStyle));
    }
    template<class F>
    void checked(const F& body)
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
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(FrontEndPartyTests)

BOOST_FIXTURE_TEST_CASE(JoinedControllersSitTogetherAndTheHostStarts, PartyFixture)
{
    checked([&] {
        open(2);
        auto& page = party();
        BOOST_TEST(dskParty::IsFor(ServerType::Local));
        BOOST_TEST(!dskParty::IsFor(ServerType::LAN));
        BOOST_TEST(page.GetSeats().IsTogether());
        BOOST_TEST_REQUIRE(page.GetSeats().size() == 4u);
        BOOST_TEST(page.GetSeats().NumTaken() == 2u);
        BOOST_TEST(GAMECLIENT.GetAdditionalLocalPlayers().empty());
        BOOST_TEST(router().GetSlot(pad) == 0u);
        BOOST_TEST(router().GetSlot(pad + 1) == 1u);
        BOOST_TEST(card(0).IsJoined());
        BOOST_TEST(card(1).IsJoined());
        BOOST_TEST(!card(2).IsJoined());
        BOOST_TEST(card(2).GetText() == "Player 3: press A to join");
        // A guest's card starts on its own seat, and its D-pad never leaves the cards for Start.
        BOOST_TEST(focusedId(1) == dskParty::ID_Seat + 1);
        pressN(pad + 1, PadButton::DpadDown, 6);
        BOOST_TEST(focusedId(1) == dskParty::ID_Seat + 1);
        // The guest cannot start the game for everybody.
        press(pad + 1, PadButton::Start);
        for(unsigned i = 0; i < 40; ++i)
            frame();
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Config));
        BOOST_TEST_REQUIRE(desktopAs<dskParty>() == &page);

        // A third controller first only looks: without a seat, its B neither leaves nor asks to.
        connect(pad + 2);
        press(pad + 2, PadButton::DpadRight);
        BOOST_TEST_REQUIRE(router().GetSlot(pad + 2) == 2u);
        BOOST_TEST(page.GetSeats().NumTaken() == 2u);
        press(pad + 2, PadButton::B);
        BOOST_TEST(question() == nullptr);
        BOOST_TEST_REQUIRE(desktopAs<dskParty>() == &page);
        // It joins on a free seat with A and stands up again with B.
        BOOST_TEST_REQUIRE(focusedId(2) >= dskParty::ID_Seat + 2);
        BOOST_TEST_REQUIRE(focusedId(2) <= dskParty::ID_SeatLast);
        press(pad + 2, PadButton::A);
        BOOST_TEST(page.GetSeats().NumTaken() == 3u);
        BOOST_TEST(GAMECLIENT.GetSharedLocalViews() == 2u);
        BOOST_TEST(card(2).IsJoined());
        BOOST_TEST(card(2).GetCursorColor().has_value());
        press(pad + 2, PadButton::B);
        BOOST_TEST(page.GetSeats().NumTaken() == 2u);
        BOOST_TEST(GAMECLIENT.GetSharedLocalViews() == 1u);
        BOOST_TEST(!card(2).IsJoined());
        BOOST_TEST(question() == nullptr);

        GAMECLIENT.SetInterface(&ci());
        press(pad, PadButton::Start);
        startedWith(2);
    });
}

BOOST_FIXTURE_TEST_CASE(UnpluggedGuestFreesItsSeat, PartyFixture)
{
    checked([&] {
        open(3);
        BOOST_TEST(party().GetSeats().NumTaken() == 3u);
        disconnect(pad + 2);
        frame();
        frame();
        BOOST_TEST(party().GetSeats().NumTaken() == 2u);
        BOOST_TEST(GAMECLIENT.GetSharedLocalViews() == 1u);
        GAMECLIENT.SetInterface(&ci());
        click(*party().GetCtrl<Window>(dskParty::ID_Start));
        startedWith(2);
    });
}

BOOST_FIXTURE_TEST_CASE(SeatsFreezeOnceStartIsAcceptedAndThePageHandsOverToTheLoader, PartyFixture)
{
    checked([&] {
        open(2);
        auto& page = party();
        connect(pad + 2);
        press(pad + 2, PadButton::DpadRight);
        BOOST_TEST_REQUIRE(router().GetSlot(pad + 2) == 2u);
        // Start and a newcomer's A in the same input batch: the seats Start validated are the ones that load.
        tap(pad, PadButton::Start);
        tap(pad + 2, PadButton::A);
        frame();
        BOOST_TEST(page.GetSeats().NumTaken() == 2u);
        BOOST_TEST(GAMECLIENT.GetSharedLocalViews() == 1u);
        // The page's own client interface switches to the loader (no test interface installed here).
        BOOST_TEST_REQUIRE(until([] { return desktopAs<dskGameLoader>() != nullptr; }));
        BOOST_TEST(GAMECLIENT.GetSharedLocalViews() == 1u);
    });
}

BOOST_FIXTURE_TEST_CASE(HostCardChangesTheSharedTribeOnlyThroughTheServer, PartyFixture)
{
    checked([&] {
        open(2);
        const unsigned before = GAMECLIENT.GetGameLobby()->getPlayer(GAMECLIENT.GetPlayerId()).color;
        BOOST_TEST(focusedId(0) == dskParty::ID_Start);
        focus(0, pad, dskParty::ID_Seat);
        BOOST_TEST(card(0).GetModel().GetLockReason().empty());
        press(pad, PadButton::DpadRight);
        BOOST_TEST_REQUIRE(
          until([before] { return GAMECLIENT.GetGameLobby()->getPlayer(GAMECLIENT.GetPlayerId()).color != before; }));
        frame();
        BOOST_TEST(card(0).GetModel().GetSnapshot().values.color
                   == GAMECLIENT.GetGameLobby()->getPlayer(GAMECLIENT.GetPlayerId()).color);
        // The guest's card shows the same tribe, and its rows are the host's to choose.
        BOOST_TEST(card(1).GetModel().GetSnapshot().values.color == card(0).GetModel().GetSnapshot().values.color);
        BOOST_TEST(card(1).GetModel().GetLockReason() == "Shared tribe: host chooses");
        const unsigned after = card(1).GetModel().GetSnapshot().values.color;
        press(pad + 1, PadButton::DpadRight);
        for(unsigned i = 0; i < 20; ++i)
            frame();
        BOOST_TEST(GAMECLIENT.GetGameLobby()->getPlayer(GAMECLIENT.GetPlayerId()).color == after);
        // The seat mode is fixed: a campaign has one human tribe.
        pressN(pad, PadButton::DpadDown, 3);
        BOOST_TEST((card(0).GetModel().GetRow() == LobbyPlayerCardModel::Row::SharedTribe));
        BOOST_TEST(card(0).GetModel().GetLockReason() == "One tribe required");
        press(pad, PadButton::DpadDown);
        BOOST_TEST(focusedId(0) == dskParty::ID_Start);
    });
}

BOOST_FIXTURE_TEST_CASE(LeavingAsksFirstAndReturnsHome, PartyFixture)
{
    checked([&] {
        open(1);
        BOOST_TEST(party().GetSeats().NumTaken() == 1u);
        // Escape asks; "No" keeps the game.
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
        frame();
        BOOST_TEST_REQUIRE(question());
        press(pad, PadButton::A); // focus starts on "No"
        BOOST_TEST(question() == nullptr);
        BOOST_TEST(desktopAs<dskParty>());
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Config));
        // B asks again. A guest's controller cannot answer the host's question.
        connect(pad + 1);
        press(pad + 1, PadButton::A); // the first press only picks the controller up
        press(pad + 1, PadButton::A);
        BOOST_TEST_REQUIRE(party().GetSeats().NumTaken() == 2u);
        press(pad, PadButton::B);
        auto* box = question();
        BOOST_TEST_REQUIRE(box);
        press(pad + 1, PadButton::A);
        press(pad + 1, PadButton::DpadLeft);
        press(pad + 1, PadButton::A);
        BOOST_TEST(question() == box);
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Config));
        // "Yes" stops the hosted game and goes home.
        click(*box->GetCtrl<Window>(2)); // Yes
        BOOST_TEST_REQUIRE(until([] { return desktopAs<dskHome>() != nullptr; }));
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
    });
}

BOOST_FIXTURE_TEST_CASE(PartyPageIsForEveryLocalHostOfTheNewMenus, PartyFixture)
{
    checked([&] {
        open(1);
        BOOST_TEST(dskParty::IsFor(ServerType::Local));
        BOOST_TEST(!dskParty::IsFor(ServerType::LAN));
        // A campaign has no rules to choose: the mission script owns them.
        BOOST_TEST(!party().HasRules());
        BOOST_TEST(party().GetCtrl<Window>(dskParty::ID_Rules) == nullptr);
        frontend::SetMenuStyle(frontend::MenuStyle::Classic);
        BOOST_TEST(!dskParty::IsFor(ServerType::Local));
        frontend::SetMenuStyle(frontend::MenuStyle::FrontEnd);
        // Ordinary maps (F7c) open it too.
        GAMECLIENT.SetHostingCampaign(false);
        BOOST_TEST(dskParty::IsFor(ServerType::Local));
        GAMECLIENT.SetHostingCampaign(true);
    });
}

BOOST_FIXTURE_TEST_CASE(OrdinaryMapSeatsEveryoneOnOwnTribesAndTheHostSwitchesToOneTribe, PartyFixture)
{
    checked([&] {
        openMap(2);
        auto& page = party();
        BOOST_TEST(page.HasRules());
        BOOST_TEST(!page.GetSeats().IsTogether());
        BOOST_TEST_REQUIRE(page.GetSeats().size() == 3u);
        BOOST_TEST(page.GetSeats().NumTaken() == 2u);
        BOOST_TEST(page.GetCtrl<ctrlText>(dskParty::ID_Mode)->GetText() == "Everyone plays a tribe of their own.");
        BOOST_TEST_REQUIRE(until([] { return ownTribesConfirmed(1); }));
        const unsigned guestTribe = GAMECLIENT.GetAdditionalLocalPlayers().front();
        BOOST_TEST(guestTribe == page.GetSeats()[1].playerId);
        BOOST_TEST(GAMECLIENT.GetSharedLocalViews() == 0u);
        frame();
        // The guest chooses its own tribe's colour; the host's stays.
        const auto* lobby = GAMECLIENT.GetGameLobby().get();
        const unsigned hostColor = lobby->getPlayer(GAMECLIENT.GetPlayerId()).color;
        const unsigned guestColor = lobby->getPlayer(guestTribe).color;
        BOOST_TEST(card(1).GetModel().GetLockReason().empty());
        press(pad + 1, PadButton::DpadRight);
        BOOST_TEST_REQUIRE(until([&] { return lobby->getPlayer(guestTribe).color != guestColor; }));
        BOOST_TEST(lobby->getPlayer(GAMECLIENT.GetPlayerId()).color == hostColor);
        // Only the host chooses the seat mode.
        pressN(pad + 1, PadButton::DpadDown, 3);
        BOOST_TEST(card(1).GetModel().GetLockReason() == "Host chooses seat mode");
        press(pad + 1, PadButton::DpadRight);
        BOOST_TEST(!page.GetSeats().IsTogether());

        // The host's card: Tribes row, D-pad right = one tribe together. The guest keeps its seat.
        focus(0, pad, dskParty::ID_Seat);
        pressN(pad, PadButton::DpadDown, 3);
        BOOST_TEST_REQUIRE((card(0).GetModel().GetRow() == LobbyPlayerCardModel::Row::SharedTribe));
        BOOST_TEST(card(0).GetModel().GetLockReason().empty());
        press(pad, PadButton::DpadRight);
        BOOST_TEST_REQUIRE(desktopAs<dskParty>() == &page);
        BOOST_TEST(page.GetSeats().IsTogether());
        BOOST_TEST(page.GetSeats().size() == 4u);
        BOOST_TEST(page.GetSeats().NumTaken() == 2u);
        BOOST_TEST(GAMECLIENT.GetSharedLocalViews() == 1u);
        BOOST_TEST(GAMECLIENT.GetAdditionalLocalPlayers().empty());
        BOOST_TEST(router().GetSlot(pad + 1) == 1u);
        BOOST_TEST(page.GetCtrl<ctrlText>(dskParty::ID_Mode)->GetText() == "Everyone plays one tribe together.");
        BOOST_TEST((card(0).GetModel().GetRow() == LobbyPlayerCardModel::Row::SharedTribe));
        // The guest's former slot is a computer player again, as before it sat down.
        BOOST_TEST_REQUIRE(until([&] {
            const auto& player = lobby->getPlayer(guestTribe);
            return player.ps == PlayerState::AI && player.aiInfo.type == AI::Type::Default;
        }));
        // And back: own tribes again, the guest on a tribe of its own.
        press(pad, PadButton::DpadLeft);
        BOOST_TEST(!page.GetSeats().IsTogether());
        BOOST_TEST(page.GetSeats().NumTaken() == 2u);
        BOOST_TEST_REQUIRE(until([] { return ownTribesConfirmed(1); }));
        BOOST_TEST(GAMECLIENT.GetSharedLocalViews() == 0u);

        GAMECLIENT.SetInterface(&ci());
        press(pad, PadButton::Start);
        BOOST_TEST_REQUIRE(until([] { return GAMECLIENT.GetState() == ClientState::Loading; }));
        BOOST_TEST_REQUIRE(ci().game);
        const auto view = std::make_unique<rttr::test::TestableGameInterface>(ci().game, GAMECLIENT.GetNWFInfo(),
                                                                              GAMECLIENT.GetPlayerId(), false);
        GAMECLIENT.GameLoaded();
        BOOST_TEST_REQUIRE(until([] { return GAMECLIENT.GetState() == ClientState::Game; }));
        GAMECLIENT.OnGameStart();
        BOOST_TEST_REQUIRE(view->GetNumViews() == 2u);
        BOOST_TEST(view->GetPlayerView(0).GetPlayerId() == GAMECLIENT.GetPlayerId());
        BOOST_TEST(view->GetPlayerView(1).GetPlayerId() == GAMECLIENT.GetAdditionalLocalPlayers().front());
        BOOST_TEST(ci().numErrors == 0u);
    });
}

BOOST_FIXTURE_TEST_CASE(MorePlayersThanTribesPlayOneTribeTogether, PartyFixture)
{
    checked([&] {
        openMap(4);
        auto& page = party();
        BOOST_TEST(page.GetSeats().IsTogether());
        BOOST_TEST(page.GetSeats().NumTaken() == 4u);
        BOOST_TEST(question() == nullptr);
        BOOST_TEST_REQUIRE(until([] { return GAMECLIENT.GetSharedLocalViews() == 3u; }));
        // Own tribes would leave a player out, so the switch is refused with a reason.
        openRules();
        BOOST_TEST(ruleFocus() == static_cast<unsigned>(dskParty::Rule::Tribes));
        press(pad, PadButton::DpadRight);
        BOOST_TEST_REQUIRE(question());
        BOOST_TEST(page.GetSeats().IsTogether());
        BOOST_TEST(page.GetSeats().NumTaken() == 4u);
    });
}

BOOST_FIXTURE_TEST_CASE(RulesDrawerSetsTheGameRulesWithPadAndMouse, PartyFixture)
{
    checked([&] {
        openMap(1);
        auto& page = party();
        BOOST_TEST(!page.IsRulesOpen());
        BOOST_TEST(page.GetCtrl<Window>(dskParty::ID_Rule)->IsVisible() == false);
        openRules();
        BOOST_TEST(ruleFocus() == static_cast<unsigned>(dskParty::Rule::Tribes));
        BOOST_TEST(!page.GetCtrl<Window>(dskParty::ID_Seat)->IsVisible());
        BOOST_TEST(!page.GetCtrl<Window>(dskParty::ID_Start)->IsVisible());
        BOOST_TEST(page.GetCtrl<Window>(dskParty::ID_RulesDone)->IsVisible());
        const auto value = [&](dskParty::Rule rule) {
            return page.GetCtrl<ctrlTextButton>(dskParty::ID_Rule + static_cast<unsigned>(rule))->GetText();
        };

        // D-pad down walks the rules, left/right change the value under the focus.
        const GlobalGameSettings defaults = rules();
        focusRule(dskParty::Rule::Goals);
        press(pad, PadButton::DpadRight);
        BOOST_TEST((rules().objective != defaults.objective));
        press(pad, PadButton::DpadLeft);
        BOOST_TEST((rules().objective == defaults.objective));
        focusRule(dskParty::Rule::Exploration);
        press(pad, PadButton::DpadRight);
        BOOST_TEST((rules().exploration != defaults.exploration));
        // A mouse click steps forward too.
        click(*page.GetCtrl<Window>(dskParty::ID_Rule + static_cast<unsigned>(dskParty::Rule::Speed)));
        BOOST_TEST((rules().speed != defaults.speed));
        click(*page.GetCtrl<Window>(dskParty::ID_Rule + static_cast<unsigned>(dskParty::Rule::LockTeams)));
        BOOST_TEST(rules().lockedTeams != defaults.lockedTeams);
        BOOST_TEST(value(dskParty::Rule::LockTeams) == (defaults.lockedTeams ? "Off" : "On"));
        // Three changes in three frames: the server's echoes of the older ones do not undo the newer, and the
        // lobby's copy (only ever written by the server) ends up with all of them.
        const GlobalGameSettings changed = rules();
        BOOST_TEST_REQUIRE(until([&] {
            const auto& server = GAMECLIENT.GetGameLobby()->getSettings();
            return server.lockedTeams == changed.lockedTeams && server.speed == changed.speed
                   && server.exploration == changed.exploration;
        }));
        BOOST_TEST(rules().lockedTeams == changed.lockedTeams);
        // Back to the defaults, so the next game of this process (the server keeps them) starts as usual.
        focusRule(dskParty::Rule::Exploration);
        press(pad, PadButton::DpadLeft);
        click(*page.GetCtrl<Window>(dskParty::ID_Rule + static_cast<unsigned>(dskParty::Rule::LockTeams)));
        focusRule(dskParty::Rule::Speed);
        press(pad, PadButton::DpadLeft);
        BOOST_TEST_REQUIRE(until([&] {
            const auto& server = GAMECLIENT.GetGameLobby()->getSettings();
            return server.lockedTeams == defaults.lockedTeams && server.speed == defaults.speed
                   && server.exploration == defaults.exploration && server.objective == defaults.objective;
        }));
        // Computer players for the tribes nobody sits on: none.
        focusRule(dskParty::Rule::Opponents);
        press(pad, PadButton::DpadLeft);
        BOOST_TEST((page.GetOpponents() == dskParty::Opponents::None));
        // Game options opens the addon window; closing it keeps the drawer.
        click(*page.GetCtrl<Window>(dskParty::ID_Rule + static_cast<unsigned>(dskParty::Rule::Options)));
        auto* addons = dynamic_cast<iwAddons*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(addons);
        addons->Close();
        frame();
        BOOST_TEST(page.IsRulesOpen());
        // B closes the drawer; the controller lands on Rules again and the cards are back.
        press(pad, PadButton::B);
        BOOST_TEST(!page.IsRulesOpen());
        BOOST_TEST(question() == nullptr);
        BOOST_TEST(focusedId(0) == dskParty::ID_Rules);
        BOOST_TEST(page.GetCtrl<Window>(dskParty::ID_Seat)->IsVisible());
        // Escape in the drawer closes it instead of asking to leave.
        openRules();
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
        frame();
        BOOST_TEST(!page.IsRulesOpen());
        BOOST_TEST(question() == nullptr);
        // So do the header's Back button and Done.
        openRules();
        click(*page.GetCtrl<Window>(dskFrontEndPage::ID_btBack));
        BOOST_TEST(!page.IsRulesOpen());
        BOOST_TEST(question() == nullptr);
        openRules();
        click(*page.GetCtrl<Window>(dskParty::ID_RulesDone));
        BOOST_TEST(!page.IsRulesOpen());
        BOOST_TEST_REQUIRE(desktopAs<dskParty>() == &page);

        // The game starts with the free tribes closed.
        GAMECLIENT.SetInterface(&ci());
        press(pad, PadButton::Start);
        BOOST_TEST_REQUIRE(until([] { return GAMECLIENT.GetState() == ClientState::Loading; }));
        BOOST_TEST_REQUIRE(ci().game);
        BOOST_TEST((ci().game->ggs_.speed == GameSpeed::Normal));
        const auto& world = ci().game->world_;
        BOOST_TEST_REQUIRE(world.GetNumPlayers() == 3u);
        for(unsigned id = 0; id < world.GetNumPlayers(); ++id)
        {
            if(id != GAMECLIENT.GetPlayerId())
                BOOST_TEST((world.GetPlayer(id).ps == PlayerState::Locked));
        }
    });
}

BOOST_FIXTURE_TEST_CASE(ComputerOpponentsGetTheChosenLevelAtStart, PartyFixture)
{
    checked([&] {
        openMap(2);
        auto& page = party();
        BOOST_TEST_REQUIRE(until([] { return ownTribesConfirmed(1); }));
        openRules();
        focusRule(dskParty::Rule::Opponents);
        pressN(pad, PadButton::DpadRight, 2);
        BOOST_TEST((page.GetOpponents() == dskParty::Opponents::Hard));
        press(pad, PadButton::B);
        GAMECLIENT.SetInterface(&ci());
        press(pad, PadButton::Start);
        BOOST_TEST_REQUIRE(until([] { return GAMECLIENT.GetState() == ClientState::Loading; }));
        BOOST_TEST_REQUIRE(ci().game);
        const auto& world = ci().game->world_;
        const unsigned guest = GAMECLIENT.GetAdditionalLocalPlayers().front();
        BOOST_TEST_REQUIRE(world.GetNumPlayers() == 3u);
        for(unsigned id = 0; id < world.GetNumPlayers(); ++id)
        {
            const auto& player = world.GetPlayer(id);
            if(id == GAMECLIENT.GetPlayerId())
                continue;
            // The guest's seat keeps its placeholder; the free tribe is a hard computer player.
            if(id == guest)
                BOOST_TEST((player.aiInfo.type == AI::Type::Dummy));
            else
                BOOST_TEST((player.ps == PlayerState::AI && player.aiInfo.type == AI::Type::Default
                            && player.aiInfo.level == AI::Level::Hard));
        }
    });
}

BOOST_FIXTURE_TEST_CASE(EconomyModeAsksAboutPeacefulModeBeforeStarting, PartyFixture)
{
    checked([&] {
        openMap(1);
        auto& page = party();
        openRules();
        focusRule(dskParty::Rule::Goals);
        while(rules().objective != GameObjective::EconomyMode)
            press(pad, PadButton::DpadRight);
        press(pad, PadButton::B);
        press(pad, PadButton::Start);
        auto* box = question();
        BOOST_TEST_REQUIRE(box);
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Config));
        // Yes adjusts the rules for a peaceful game and lets the host review them.
        click(*box->GetCtrl<Window>(2));
        BOOST_TEST(question() == nullptr);
        BOOST_TEST(rules().isEnabled(AddonId::PEACEFULMODE));
        for(unsigned i = 0; i < 40; ++i)
            frame();
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Config));
        BOOST_TEST_REQUIRE(desktopAs<dskParty>() == &page);
        // With the goal back to none there is nothing to ask: Start starts. (The goal goes back anyway: the server
        // keeps it for the next game of this process.)
        openRules();
        focusRule(dskParty::Rule::Goals);
        press(pad, PadButton::DpadRight);
        BOOST_TEST((rules().objective == GameObjective::None));
        press(pad, PadButton::B);
        BOOST_TEST_REQUIRE(
          until([] { return GAMECLIENT.GetGameLobby()->getSettings().objective == GameObjective::None; }));
        GAMECLIENT.SetInterface(&ci());
        press(pad, PadButton::Start);
        BOOST_TEST(question() == nullptr);
        BOOST_TEST_REQUIRE(until([] { return GAMECLIENT.GetState() == ClientState::Loading; }));
    });
}

BOOST_FIXTURE_TEST_CASE(ExceptionAfterOpeningStopsTheGameAndAllowsAnotherStart, PartyFixture)
{
    bool reached = false;
    BOOST_CHECK_THROW(checked([&] {
                          open(2);
                          reached = true;
                          cleanupProbe();
                      }),
                      std::runtime_error);
    BOOST_TEST_REQUIRE(reached);
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
    BOOST_TEST(SETTINGS.server.localPort == savedPort);
    checked([&] { open(2); });
}

BOOST_AUTO_TEST_SUITE_END()
