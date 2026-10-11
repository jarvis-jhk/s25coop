// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "LocalGameFixture.h"
#include "MenuPadFixture.h"
#include "PadFixture.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlCheck.h"
#include "controls/ctrlComboBox.h"
#include "controls/ctrlList.h"
#include "controls/ctrlPreviewMinimap.h"
#include "controls/ctrlTable.h"
#include "controls/ctrlText.h"
#include "desktops/dskFrontEndMaps.h"
#include "desktops/dskGameLobby.h"
#include "desktops/dskHome.h"
#include "desktops/dskSelectMap.h"
#include "desktops/dskSinglePlayer.h"
#include "desktops/dskTitle.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "ingameWindows/iwConnecting.h"
#include "ingameWindows/iwMsgbox.h"
#include "ogl/glFont.h"
#include "libsiedler2/ArchivItem_Map.h"
#include "libsiedler2/ArchivItem_Map_Header.h"
#include "libsiedler2/prototypen.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include "s25util/Socket.h"
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <array>
#include <memory>

namespace {
constexpr PadDeviceId pad = 91;
using Page = dskFrontEndMaps;

[[noreturn]] void cleanupProbe()
{
    throw std::runtime_error("deliberate map-browser cleanup probe");
}

void writeMap(const boost::filesystem::path& path, const std::string& name, uint8_t players = 2, bool complete = true)
{
    boost::filesystem::create_directories(path.parent_path());
    auto header = std::make_unique<libsiedler2::ArchivItem_Map_Header>();
    header->setName(name);
    header->setAuthor("Map author");
    header->setNumPlayers(players);
    header->setWidth(32);
    header->setHeight(32);
    boost::nowide::ofstream stream(path.string(), std::ios::binary);
    if(complete)
    {
        libsiedler2::ArchivItem_Map map;
        map.init(std::move(header));
        BOOST_TEST_REQUIRE(map.write(stream) == 0);
    } else
        BOOST_TEST_REQUIRE(header->write(stream) == 0);
}

struct MapsFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder data;
    rttr::test::ConfigOverride userOverride{"USERDATA", data / "user"};
    rttr::test::ConfigOverride gameOverride{"GAME", data / "game"};
    const boost::filesystem::path originalWorld = RTTRCONFIG.ExpandPath(s25::folders::gamedata) / "world";
    rttr::test::ConfigOverride rttrOverride{"RTTR", data / "rttr"};
    const boost::filesystem::path maps = RTTRCONFIG.ExpandPath(s25::folders::mapsOwn);

    MapsFixture()
    {
        const auto world = RTTRCONFIG.ExpandPath(s25::folders::gamedata) / "world";
        boost::filesystem::create_directories(world);
        for(boost::filesystem::directory_iterator it(originalWorld), end; it != end; ++it)
        {
            boost::filesystem::copy_file(it->path(), world / it->path().filename());
        }
    }

    static Page& page()
    {
        auto* result = desktopAs<Page>();
        BOOST_TEST_REQUIRE(result);
        return *result;
    }
    void focus(unsigned id)
    {
        for(unsigned i = 0; i < 16 && focusedId(0) != id; ++i)
            press(pad, focusedId(0) < id ? PadButton::RightShoulder : PadButton::LeftShoulder);
        BOOST_TEST_REQUIRE(focusedId(0) == id);
    }
    void enter()
    {
        dskHome::ForgetLastChoice();
        WINDOWMANAGER.Switch(dskHome::Create());
        frame();
        pickUp(pad);
        focus(dskHome::ID_Maps);
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Page>());
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    }
    void filter(unsigned id, unsigned index)
    {
        focus(id);
        auto* combo = page().GetCtrl<ctrlComboBox>(id);
        press(pad, PadButton::A);
        BOOST_TEST_REQUIRE(combo->IsListOpen());
        const unsigned current = *combo->GetSelection();
        pressN(pad, index < current ? PadButton::DpadUp : PadButton::DpadDown,
               index < current ? current - index : index - current);
        press(pad, PadButton::A);
        BOOST_TEST(!combo->IsListOpen());
        BOOST_TEST((*combo->GetSelection() == index));
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
    void closeError()
    {
        auto* error = dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(error);
        click(*error->GetCtrl<Window>(2));
        frame();
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    }
    // NOLINTNEXTLINE(bugprone-exception-escape)
    ~MapsFixture() override { dskHome::ForgetLastChoice(); }
};

struct StartFixture : rttr::test::LocalGameFixture, MapsFixture
{
    const uint16_t savedPort = SETTINGS.server.localPort;

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
    void cleanup()
    {
        GAMECLIENT.Stop();
        GAMESERVER.Stop();
        GAMECLIENT.SetInterface(&ci());
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        frame();
        SETTINGS.server.localPort = savedPort;
        BOOST_TEST(SETTINGS.server.localPort == savedPort);
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
    void joinParty(unsigned count)
    {
        boost::filesystem::create_directories(maps);
        boost::filesystem::create_directories(RTTRCONFIG.ExpandPath(s25::folders::mapsPlayed));
        boost::filesystem::create_directories(RTTRCONFIG.ExpandPath(s25::folders::replays));
        boost::filesystem::copy_file(mapPath(), maps / "play.SWD");
        WINDOWMANAGER.Switch(dskTitle::Create());
        frame();
        for(unsigned i = 0; i < count; ++i)
        {
            connect(pad + i);
            press(pad + i, PadButton::A);
        }
        BOOST_TEST_REQUIRE(padInput().GetParty().Members().size() == count);
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<dskHome>());
    }
    void host(unsigned count, unsigned input)
    {
        joinParty(count);
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
        socklen_t addressSize = sizeof(address);
        BOOST_TEST_REQUIRE(getsockname(occupied.GetSocket(), &address.sa, &addressSize) == 0);
        const uint16_t busyPort = ntohs(address.sa_in6.sin6_port);
        bool ready = false;
        for(unsigned attempt = 0; attempt < 10 && !ready; ++attempt)
        {
            // Prove the real failure/retry route instead of excluding an unexecuted fallback.
            SETTINGS.server.localPort =
              attempt == 0 ? busyPort : static_cast<uint16_t>(rttr::test::randomValue(1024, 49151));
            focus(dskHome::ID_Maps);
            press(pad, PadButton::A);
            frame();
            BOOST_TEST_REQUIRE(desktopAs<Page>());
            // A failed host can already have copied this map into Played. Keep its original identity.
            BOOST_TEST_REQUIRE(!page().GetCatalog().entries.empty());
            for(unsigned n = 0; n < page().GetCatalog().entries.size() && page().GetSelectedPath() != maps / "play.SWD";
                ++n)
                press(pad, PadButton::DpadDown);
            BOOST_TEST_REQUIRE(page().GetSelectedPath() == maps / "play.SWD");
            BOOST_TEST(page()
                         .GetCtrl<ctrlTable>(Page::ID_Maps)
                         ->GetItemText(*page().GetCtrl<ctrlTable>(Page::ID_Maps)->GetSelection(), 1)
                       == "3");
            BOOST_TEST((*page().GetCtrl<ctrlComboBox>(Page::ID_PlayerFilter)->GetSelection() == 0u));
            BOOST_TEST(focusedId(0) == Page::ID_Maps);
            if(input == 1)
                click(*page().GetCtrl<Window>(Page::ID_Continue));
            else if(input == 2)
            {
                WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Return));
                frame();
            } else if(input == 3)
            {
                const auto* table = page().GetCtrl<ctrlTable>(Page::ID_Maps);
                BOOST_TEST_REQUIRE(table->GetSelection().has_value());
                const auto pos =
                  table->GetDrawPos()
                  + DrawPoint(20, NormalFont->getHeight() * (static_cast<int>(*table->GetSelection()) + 1)
                                    + NormalFont->getHeight() / 2 + 10);
                clickAt(pos);
                BOOST_TEST_REQUIRE(page().GetSelectedPath() == maps / "play.SWD");
                clickAt(pos);
            } else
            {
                // Several edges in one frame cannot host over the connecting window.
                tap(pad, PadButton::A);
                tap(pad, PadButton::A);
                frame();
            }
            if(attempt == 0)
                BOOST_TEST_REQUIRE(dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow()));
            if(auto* connecting = dynamic_cast<iwConnecting*>(WINDOWMANAGER.GetTopMostWindow()))
            {
                const auto* sameWindow = connecting;
                WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Return));
                click(*page().GetCtrl<Window>(Page::ID_Continue));
                BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == sameWindow);
            }
            ready = until([] {
                        return desktopAs<dskGameLobby>() || dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow());
                    })
                    && desktopAs<dskGameLobby>();
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
        BOOST_TEST(!GAMECLIENT.GetGameLobby()->isSavegame());
        const bool together = count > GAMECLIENT.GetGameLobby()->getNumPlayers();
        if(together)
        {
            // Four controllers can use this three-tribe map; the lobby owns the explicit shared-mode choice.
            closeError();
            constexpr unsigned togetherId = 48 + 2 * MAX_PLAYERS + MAX_VIEWPORTS + 7;
            auto* toggle = desktop()->GetCtrl<ctrlCheck>(togetherId);
            BOOST_TEST_REQUIRE(toggle);
            click(*toggle);
            BOOST_TEST_REQUIRE(until([count] { return GAMECLIENT.GetSharedLocalViews() == count - 1; }));
            BOOST_TEST(GAMECLIENT.GetAdditionalLocalPlayers().empty());
        } else
        {
            BOOST_TEST_REQUIRE(until([count] {
                const auto& ids = GAMECLIENT.GetAdditionalLocalPlayers();
                return ids.size() == count - 1 && std::all_of(ids.begin(), ids.end(), [](const uint8_t id) {
                           const auto& player = GAMECLIENT.GetGameLobby()->getPlayer(id);
                           return player.ps == PlayerState::AI && player.aiInfo.type == AI::Type::Dummy;
                       });
            }));
            BOOST_TEST(GAMECLIENT.GetSharedLocalViews() == 0u);
        }
        for(unsigned i = 0; i < count; ++i)
            BOOST_TEST(router().GetSlot(pad + i) == i);
        // Only original-resource loading is bypassed, as in FrontEndLoadTests.
        GAMECLIENT.SetInterface(&ci());
        press(pad, PadButton::Start);
        BOOST_TEST_REQUIRE(until([] { return GAMECLIENT.GetState() == ClientState::Loading; }));
        GAMECLIENT.GameLoaded();
        BOOST_TEST_REQUIRE(until([] { return GAMECLIENT.GetState() == ClientState::Game; }));
        GAMECLIENT.OnGameStart();
        BOOST_TEST_REQUIRE(ci().game);
        {
            const auto view = std::make_unique<rttr::test::TestableGameInterface>(ci().game, GAMECLIENT.GetNWFInfo(),
                                                                                  GAMECLIENT.GetPlayerId(), false);
            BOOST_TEST_REQUIRE(view->GetNumViews() == count);
            BOOST_TEST(view->GetPlayerView(0).GetPlayerId() == GAMECLIENT.GetPlayerId());
            for(unsigned i = 1; i < count; ++i)
                BOOST_TEST(view->GetPlayerView(i).GetPlayerId()
                           == (together ? GAMECLIENT.GetPlayerId() : GAMECLIENT.GetAdditionalLocalPlayers()[i - 1]));
        }
        GAMECLIENT.SetInterface(&ci());
        const unsigned target = GAMECLIENT.GetGFNumber() + 12;
        BOOST_TEST_REQUIRE(until([target] { return GAMECLIENT.GetGFNumber() >= target; }));
        BOOST_TEST(ci().numErrors == 0u);
        BOOST_TEST(ci().numAsync == 0u);
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(FrontEndMapsTests)

BOOST_FIXTURE_TEST_CASE(EmptyBrowserCannotStartAndBackReturnsToHomeTile, MapsFixture)
{
    enter();
    BOOST_TEST(page().GetSelectedPath().empty());
    BOOST_TEST(!page().GetCtrl<ctrlButton>(Page::ID_Continue)->GetEnabled());
    BOOST_TEST(focusedId(0) == Page::ID_Category);
    WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Return));
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    press(pad, PadButton::B);
    frame();
    BOOST_TEST_REQUIRE(desktopAs<dskHome>());
    BOOST_TEST(focusedId(0) == dskHome::ID_Maps);
}

BOOST_FIXTURE_TEST_CASE(LazyPreviewIsolatesIncompleteMapsAndRefreshRepairsThem, MapsFixture)
{
    writeMap(maps / "a.swd", "Alpha");
    writeMap(maps / "b.swd", "Beta", 3, false);
    enter();
    BOOST_TEST_REQUIRE(page().GetCatalog().entries.size() == 2u);
    BOOST_TEST(page().GetSelectedPath() == maps / "a.swd");
    BOOST_TEST(page().GetCtrl<ctrlButton>(Page::ID_Continue)->GetEnabled());
    BOOST_TEST(page().GetCtrl<ctrlPreviewMinimap>(Page::ID_Preview)->GetCurMapSize().x > 0u);
    BOOST_TEST(page().GetCtrl<ctrlText>(Page::ID_MapState)->GetText().empty());
    press(pad, PadButton::DpadDown);
    BOOST_TEST(page().GetSelectedPath() == maps / "b.swd");
    BOOST_TEST(!page().GetCtrl<ctrlButton>(Page::ID_Continue)->GetEnabled());
    BOOST_TEST(page().GetCtrl<ctrlPreviewMinimap>(Page::ID_Preview)->GetCurMapSize().x == 0u);
    BOOST_TEST(!page().GetCtrl<ctrlText>(Page::ID_MapState)->GetText().empty());
    press(pad, PadButton::A);
    closeError();
    writeMap(maps / "b.swd", "Beta repaired", 3);
    click(*page().GetCtrl<Window>(Page::ID_Refresh));
    BOOST_TEST(page().GetSelectedPath() == maps / "b.swd");
    BOOST_TEST(page().GetCtrl<ctrlButton>(Page::ID_Continue)->GetEnabled());
    BOOST_TEST(page().GetCtrl<ctrlText>(Page::ID_MapName)->GetText() == "Beta repaired");
    BOOST_TEST(page().GetCtrl<ctrlText>(Page::ID_Info)->GetText() == "3 tribes — 32 × 32");
    BOOST_TEST(page().GetCtrl<ctrlText>(Page::ID_Author)->GetText() == "Map author");
}

BOOST_FIXTURE_TEST_CASE(PadFiltersAllCategoriesAndExactTribesWithoutExecutingScripts, MapsFixture)
{
    const std::array<const char*, 9> sources = {{s25::folders::mapsOld, s25::folders::mapsNew, s25::folders::mapsOwn,
                                                 s25::folders::mapsContinents, s25::folders::mapsCampaign,
                                                 s25::folders::mapsRttr, s25::folders::mapsOther, s25::folders::mapsSea,
                                                 s25::folders::mapsPlayed}};
    for(unsigned i = 0; i < sources.size(); ++i)
        writeMap(RTTRCONFIG.ExpandPath(sources[i]) / "map.swd", "Category " + std::to_string(i), 2);
    writeMap(maps / "extra.WLD", "Extra", 4);
    boost::nowide::ofstream((maps / "map.lua").string()) << "error('must not execute while browsing')";
    enter();
    BOOST_TEST_REQUIRE(page().GetCatalog().entries.size() == 10u);
    for(unsigned category = 1; category <= sources.size(); ++category)
    {
        filter(Page::ID_Category, category);
        BOOST_TEST_REQUIRE(page().GetCatalog().entries.size() == (category == 3 ? 2u : 1u));
        BOOST_TEST(page().GetSelectedPath() == RTTRCONFIG.ExpandPath(sources[category - 1]) / "map.swd");
        if(category == 3)
            BOOST_TEST(page().GetCtrl<ctrlText>(Page::ID_Script)->GetText() == "Map script present");
    }
    filter(Page::ID_Category, 0);
    filter(Page::ID_PlayerFilter, 4);
    BOOST_TEST_REQUIRE(page().GetCatalog().entries.size() == 1u);
    BOOST_TEST(page().GetCatalog().entries.front().players == 4u);
    filter(Page::ID_PlayerFilter, 1);
    BOOST_TEST(page().GetSelectedPath().empty());
    BOOST_TEST(!page().GetCtrl<ctrlButton>(Page::ID_Continue)->GetEnabled());
    filter(Page::ID_PlayerFilter, 2);
    BOOST_TEST(page().GetCatalog().entries.size() == 9u);
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
}

BOOST_FIXTURE_TEST_CASE(CancellingAFilterKeepsIdentityBeforeLeavingThePage, MapsFixture)
{
    writeMap(maps / "a.swd", "Alpha", 2);
    writeMap(maps / "b.swd", "Beta", 4);
    enter();
    press(pad, PadButton::DpadDown);
    const auto selected = page().GetSelectedPath();
    focus(Page::ID_PlayerFilter);
    press(pad, PadButton::A);
    WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Return));
    BOOST_TEST(!page().GetCtrl<ctrlComboBox>(Page::ID_PlayerFilter)->IsListOpen());
    press(pad, PadButton::A);
    pressN(pad, PadButton::DpadDown, 2);
    BOOST_TEST(page().GetCatalog().entries.size() == 2u);
    BOOST_TEST(page().GetSelectedPath() == selected);
    press(pad, PadButton::B);
    BOOST_TEST_REQUIRE(desktopAs<Page>());
    BOOST_TEST((*page().GetCtrl<ctrlComboBox>(Page::ID_PlayerFilter)->GetSelection() == 0u));
    BOOST_TEST(page().GetSelectedPath() == selected);
    press(pad, PadButton::B);
    frame();
    BOOST_TEST_REQUIRE(desktopAs<dskHome>());
}

BOOST_FIXTURE_TEST_CASE(LongListsUsePhysicalPadKeyboardMouseAndKeepStablePaths, MapsFixture)
{
    for(unsigned i = 0; i < 80; ++i)
        writeMap(maps / (std::to_string(i) + ".swd"), "Map " + std::to_string(100 + i));
    enter();
    pressN(pad, PadButton::DpadDown, 79);
    BOOST_TEST(page().GetSelectedPath() == maps / "79.swd");
    pressN(pad, PadButton::DpadUp, 79);
    disconnect(pad);
    frame();
    WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Down));
    BOOST_TEST(page().GetSelectedPath() == maps / "1.swd");
    const auto* table = page().GetCtrl<ctrlTable>(Page::ID_Maps);
    const int rowHeight = NormalFont->getHeight();
    clickAt(table->GetDrawPos() + DrawPoint(20, rowHeight + 10 + 2 * rowHeight + rowHeight / 2));
    BOOST_TEST(page().GetSelectedPath() == maps / "2.swd");
    writeMap(maps / "new.swd", "A before all");
    click(*page().GetCtrl<Window>(Page::ID_Refresh));
    BOOST_TEST(page().GetSelectedPath() == maps / "2.swd");
    boost::filesystem::remove(maps / "2.swd");
    click(*page().GetCtrl<Window>(Page::ID_Refresh));
    BOOST_TEST(page().GetSelectedPath() == maps / "new.swd");
    WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
    frame();
    BOOST_TEST_REQUIRE(desktopAs<dskHome>());
}

BOOST_FIXTURE_TEST_CASE(MouseFiltersAndBackButtonWorkWithoutPadFocus, MapsFixture)
{
    writeMap(maps / "a.swd", "Alpha", 2);
    writeMap(maps / "b.swd", "Beta", 4);
    enter();
    disconnect(pad);
    frame();
    auto* combo = page().GetCtrl<ctrlComboBox>(Page::ID_PlayerFilter);
    click(*combo);
    BOOST_TEST_REQUIRE(combo->IsListOpen());
    const auto* list = combo->GetCtrl<ctrlList>(0);
    clickAt(list->GetDrawPos() + DrawPoint(15, 2 + 4 * NormalFont->getHeight() + NormalFont->getHeight() / 2));
    BOOST_TEST((*combo->GetSelection() == 4u));
    BOOST_TEST_REQUIRE(page().GetCatalog().entries.size() == 1u);
    BOOST_TEST(page().GetSelectedPath() == maps / "b.swd");
    click(*combo);
    BOOST_TEST_REQUIRE(combo->IsListOpen());
    WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
    BOOST_TEST_REQUIRE(desktopAs<Page>());
    BOOST_TEST(!combo->IsListOpen());
    click(*page().GetCtrl<Window>(dskFrontEndPage::ID_btBack));
    BOOST_TEST_REQUIRE(desktopAs<dskHome>());
}

BOOST_FIXTURE_TEST_CASE(MixedInputCommitsFiltersWithoutStartingTheSelectedMap, MapsFixture)
{
    writeMap(maps / "a.swd", "Alpha", 2);
    writeMap(maps / "b.swd", "Beta", 4);
    enter();
    focus(Page::ID_PlayerFilter);
    auto* combo = page().GetCtrl<ctrlComboBox>(Page::ID_PlayerFilter);
    click(*combo);
    WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Down));
    WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Up));
    for(unsigned i = 0; i < 4; ++i)
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Down));
    BOOST_TEST(page().GetCatalog().entries.size() == 2u);
    WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Return));
    BOOST_TEST(!combo->IsListOpen());
    BOOST_TEST_REQUIRE(page().GetCatalog().entries.size() == 1u);
    BOOST_TEST(page().GetSelectedPath() == maps / "b.swd");
    press(pad, PadButton::A);
    const auto* list = combo->GetCtrl<ctrlList>(0);
    clickAt(list->GetDrawPos() + DrawPoint(15, 2 + NormalFont->getHeight() / 2));
    BOOST_TEST((*combo->GetSelection() == 0u));
    BOOST_TEST(page().GetCatalog().entries.size() == 2u);
    BOOST_TEST(page().GetSelectedPath() == maps / "b.swd");
    click(*combo);
    pressN(pad, PadButton::DpadDown, 2);
    press(pad, PadButton::A);
    BOOST_TEST_REQUIRE(page().GetCatalog().entries.size() == 1u);
    BOOST_TEST(page().GetSelectedPath() == maps / "a.swd");
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
}

BOOST_FIXTURE_TEST_CASE(MouseOpenedFilterBlocksUnderlyingPadActivationAndBCancelsFirst, MapsFixture)
{
    writeMap(maps / "a.swd", "Alpha", 2);
    enter();
    BOOST_TEST_REQUIRE(focusedId(0) == Page::ID_Maps);
    auto* combo = page().GetCtrl<ctrlComboBox>(Page::ID_PlayerFilter);
    click(*combo);
    BOOST_TEST_REQUIRE(combo->IsListOpen());
    press(pad, PadButton::A);
    BOOST_TEST(combo->IsListOpen());
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    press(pad, PadButton::B);
    BOOST_TEST_REQUIRE(desktopAs<Page>());
    BOOST_TEST(!combo->IsListOpen());
    BOOST_TEST((*combo->GetSelection() == 0u));
    press(pad, PadButton::B);
    frame();
    BOOST_TEST_REQUIRE(desktopAs<dskHome>());
}

BOOST_FIXTURE_TEST_CASE(MouseAcceptsTheAlreadyHighlightedFilterPreview, MapsFixture)
{
    writeMap(maps / "a.swd", "Alpha", 2);
    writeMap(maps / "b.swd", "Beta", 4);
    enter();
    for(const bool right : {false, true})
    {
        filter(Page::ID_PlayerFilter, 0);
        focus(Page::ID_PlayerFilter);
        press(pad, PadButton::A);
        pressN(pad, PadButton::DpadDown, 4);
        auto* combo = page().GetCtrl<ctrlComboBox>(Page::ID_PlayerFilter);
        BOOST_TEST_REQUIRE(combo->IsListOpen());
        BOOST_TEST(page().GetCatalog().entries.size() == 2u);
        const auto* list = combo->GetCtrl<ctrlList>(0);
        const auto pos =
          list->GetDrawPos() + DrawPoint(15, 2 + 4 * NormalFont->getHeight() + NormalFont->getHeight() / 2);
        if(right)
        {
            WINDOWMANAGER.Msg_RightDown(MouseCoords(pos));
            frame();
        } else
            clickAt(pos);
        BOOST_TEST(!combo->IsListOpen());
        BOOST_TEST((*combo->GetSelection() == 4u));
        BOOST_TEST(page().GetCatalog().entries.size() == 1u);
        BOOST_TEST(page().GetSelectedPath() == maps / "b.swd");
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
    }
}

BOOST_FIXTURE_TEST_CASE(ChangedOrDeletedPreviewCannotStartAndBadHeadersRemainIsolated, MapsFixture)
{
    writeMap(maps / "a.swd", "Alpha");
    writeMap(maps / "b.swd", "Beta");
    writeMap(maps / "bad.swd", "Bad", 0);
    enter();
    BOOST_TEST_REQUIRE(page().GetCatalog().entries.size() == 2u);
    BOOST_TEST_REQUIRE(page().GetCatalog().failures.size() == 1u);
    BOOST_TEST(page().GetCtrl<ctrlText>(Page::ID_Status)->GetText() == "2 maps; 1 unavailable files");
    boost::filesystem::remove(maps / "a.swd");
    WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Return));
    frame();
    closeError();
    BOOST_TEST(!page().GetCtrl<ctrlButton>(Page::ID_Continue)->GetEnabled());
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
    press(pad, PadButton::DpadDown);
    // A formerly valid preview must not authorize hosting a replaced, unsupported header.
    writeMap(maps / "b.swd", "Replaced", 0);
    tap(pad, PadButton::A);
    tap(pad, PadButton::Start);
    frame();
    closeError();
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
    click(*page().GetCtrl<Window>(Page::ID_Refresh));
    BOOST_TEST(page().GetSelectedPath().empty());
    BOOST_TEST(page().GetCatalog().failures.size() == 2u);
}

BOOST_FIXTURE_TEST_CASE(AReplacedMapMustStillMatchTheCommittedTribeFilter, MapsFixture)
{
    writeMap(maps / "a.swd", "Alpha", 4);
    writeMap(maps / "b.swd", "Beta", 4);
    enter();
    filter(Page::ID_PlayerFilter, 4);
    focus(Page::ID_Maps);
    BOOST_TEST_REQUIRE(page().GetSelectedPath() == maps / "a.swd");
    writeMap(maps / "a.swd", "Alpha changed", 2);
    press(pad, PadButton::A);
    closeError();
    BOOST_TEST(!page().GetCtrl<ctrlButton>(Page::ID_Continue)->GetEnabled());
    BOOST_TEST(page().GetCtrl<ctrlText>(Page::ID_MapState)->GetText()
               == "Map unavailable: Map no longer matches the tribe filter");
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
    click(*page().GetCtrl<Window>(Page::ID_Refresh));
    BOOST_TEST_REQUIRE(page().GetCatalog().entries.size() == 1u);
    BOOST_TEST(page().GetSelectedPath() == maps / "b.swd");
}

BOOST_FIXTURE_TEST_CASE(LayoutAndOpenFilterListsStayAboveTheFooterOnResize, MapsFixture)
{
    const auto savedVideo = SETTINGS.video;
    const auto savedSize = VIDEODRIVER.GetWindowSize();
    const auto savedMode = VIDEODRIVER.GetDisplayMode();
    const auto savedScale = VIDEODRIVER.getGuiScale().percent();
    const auto savedReference = VIDEODRIVER.getUiReferenceHeight();
    const auto restore = [&] {
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        frame();
        VIDEODRIVER.setUiReferenceHeight(savedReference);
        VIDEODRIVER.setGuiScalePercent(savedScale);
        VIDEODRIVER.ResizeScreen(savedSize, savedMode);
        SETTINGS.video = savedVideo;
        BOOST_TEST((SETTINGS.video.windowedSize == savedVideo.windowedSize));
        BOOST_TEST(SETTINGS.video.guiScale == savedVideo.guiScale);
        BOOST_TEST((VIDEODRIVER.GetWindowSize() == savedSize));
    };
    const auto checkedLayout = [&](const auto& body) {
        try
        {
            body();
        } catch(...)
        {
            restore();
            throw;
        }
        restore();
    };
    checkedLayout([&] {
        writeMap(maps / "a.swd", "Alpha");
        enter();
        for(const VideoMode size : {VideoMode(800, 600), VideoMode(1280, 800), VideoMode(800, 480)})
        {
            VIDEODRIVER.setUiReferenceHeight(0);
            VIDEODRIVER.setGuiScalePercent(100);
            VIDEODRIVER.ResizeScreen(size, DisplayMode::Windowed);
            frame();
            const Rect& area = page().GetFrame().content;
            for(unsigned id = Page::ID_Category; id <= Page::ID_MapState; ++id)
            {
                const auto* control = page().GetCtrl<Window>(id);
                BOOST_TEST_REQUIRE(control);
                const Rect rect = control->GetBoundaryRect();
                BOOST_TEST((rect.left >= area.left && rect.right <= area.right && rect.top >= area.top
                            && rect.bottom <= area.bottom));
            }
            for(const unsigned id : {Page::ID_Category, Page::ID_PlayerFilter})
            {
                focus(id);
                press(pad, PadButton::A);
                const auto* combo = page().GetCtrl<ctrlComboBox>(id);
                BOOST_TEST_REQUIRE(combo->IsListOpen());
                BOOST_TEST(combo->GetCtrl<ctrlList>(0)->GetDrawRect().bottom <= area.bottom);
                const auto beforeResize = VIDEODRIVER.GetWindowSize();
                VIDEODRIVER.ResizeScreen(VideoMode(beforeResize.width + 8, beforeResize.height + 8),
                                         DisplayMode::Windowed);
                frame();
                BOOST_TEST(!combo->IsListOpen());
                BOOST_TEST((*combo->GetSelection() == 0u));
                press(pad, PadButton::A);
                BOOST_TEST_REQUIRE(combo->IsListOpen());
                BOOST_TEST(combo->GetCtrl<ctrlList>(0)->GetDrawRect().bottom <= page().GetFrame().content.bottom);
                press(pad, PadButton::B);
            }
        }
        VIDEODRIVER.ResizeScreen(VideoMode(480, 240), DisplayMode::Windowed);
        frame();
        // Resize notifications clamp to the legacy minimum; entering at the actual small render size
        // still exercises the page's unavailable-content guard (e.g. a high GUI scale).
        BOOST_TEST(page().GetSize().x >= VideoDriverWrapper::MinWindowSize.width);
        BOOST_TEST(page().GetSize().y >= VideoDriverWrapper::MinWindowSize.height);
        enter();
        const auto contentSize = page().GetFrame().content.getSize();
        BOOST_TEST_REQUIRE((contentSize.x < 480 || contentSize.y < 240));
        BOOST_TEST(!page().GetCtrl<Window>(Page::ID_Maps)->IsVisible());
        BOOST_TEST(page().GetCtrl<ctrlText>(Page::ID_Status)->GetText()
                   == "Reduce the UI scale or enlarge the window.");
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Return));
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        VIDEODRIVER.ResizeScreen(VideoMode(808, 608), DisplayMode::Windowed);
        frame();
        BOOST_TEST(page().GetCtrl<Window>(Page::ID_Maps)->IsVisible());
        BOOST_TEST(page().GetSelectedPath() == maps / "a.swd");
    });
    bool reachedProbe = false;
    BOOST_CHECK_THROW(checkedLayout([&] {
                          enter();
                          VIDEODRIVER.ResizeScreen(VideoMode(1280, 800), DisplayMode::Windowed);
                          BOOST_TEST_REQUIRE(desktopAs<Page>());
                          reachedProbe = true;
                          cleanupProbe();
                      }),
                      std::runtime_error);
    BOOST_TEST(reachedProbe);
}

BOOST_FIXTURE_TEST_CASE(ClassicMapsKeepTheOriginalPicker, MapsFixture)
{
    dskSinglePlayer::PrepareSinglePlayerServer();
    frame();
    BOOST_TEST_REQUIRE(desktopAs<dskSelectMap>());
    BOOST_TEST(desktopAs<Page>() == nullptr);
}

BOOST_FIXTURE_TEST_CASE(TwoJoinedControllersReachIndividualTribesThroughPadMapStart, StartFixture)
{
    checked([&] { host(2, 0); });
}

BOOST_FIXTURE_TEST_CASE(FourJoinedControllersChooseOneTribeThroughMouseContinue, StartFixture)
{
    checked([&] { host(4, 1); });
}

BOOST_FIXTURE_TEST_CASE(KeyboardEnterStartsTheSelectedMap, StartFixture)
{
    checked([&] { host(2, 2); });
}

BOOST_FIXTURE_TEST_CASE(DoubleClickStartsTheSelectedMap, StartFixture)
{
    checked([&] { host(2, 3); });
}

BOOST_FIXTURE_TEST_CASE(ExceptionalCleanupReachesARealConnectingWindowAndRestoresPort, StartFixture)
{
    bool reachedProbe = false;
    BOOST_CHECK_THROW(checked([&] {
                          joinParty(2);
                          SETTINGS.server.localPort = static_cast<uint16_t>(rttr::test::randomValue(1024, 49151));
                          focus(dskHome::ID_Maps);
                          press(pad, PadButton::A);
                          frame();
                          press(pad, PadButton::A);
                          BOOST_TEST_REQUIRE(dynamic_cast<iwConnecting*>(WINDOWMANAGER.GetTopMostWindow()));
                          reachedProbe = true;
                          cleanupProbe();
                      }),
                      std::runtime_error);
    BOOST_TEST(reachedProbe);
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
    BOOST_TEST(SETTINGS.server.localPort == savedPort);
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
