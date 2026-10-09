// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "BasePlayerInfo.h"
#include "GamePlayer.h"
#include "LocalGameFixture.h"
#include "MenuPadFixture.h"
#include "PadFixture.h"
#include "Savegame.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlTable.h"
#include "controls/ctrlText.h"
#include "desktops/PlayerView.h"
#include "desktops/dskFrontEndLoad.h"
#include "desktops/dskGameLobby.h"
#include "desktops/dskHome.h"
#include "desktops/dskSinglePlayer.h"
#include "desktops/dskTitle.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "helpers/serializeEnums.h"
#include "ingameWindows/iwConnecting.h"
#include "ingameWindows/iwMsgbox.h"
#include "ingameWindows/iwSave.h"
#include "ogl/glFont.h"
#include "gameTypes/CompressedData.h"
#include "libendian/ConvertEndianess.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include "s25util/BinaryFile.h"
#include "s25util/Socket.h"
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <memory>

namespace {
constexpr PadDeviceId pad = 81;

[[noreturn]] void cleanupProbe()
{
    throw std::runtime_error("deliberate cleanup probe");
}

void makeMetadata(const boost::filesystem::path& path, unsigned index)
{
    Savegame save;
    BasePlayerInfo player;
    player.ps = PlayerState::Occupied;
    player.name = "Player " + std::to_string(index);
    save.AddPlayer(player);
    player.ps = PlayerState::AI;
    player.name = "AI " + std::to_string(index);
    save.AddPlayer(player);
    save.start_gf = 100 + index;
    {
        BinaryFile file;
        BOOST_TEST_REQUIRE(file.Open(path, OpenFileMode::Write));
        save.WriteAllHeaderData(file, "Map " + std::to_string(index));
        save.WritePlayerData(file);
        save.WriteGGS(file);
    }
    boost::nowide::fstream file(path.string(), std::ios::binary | std::ios::in | std::ios::out);
    BOOST_TEST_REQUIRE(file.good());
    file.seekp(16);
    const auto time = libendian::ConvertEndianess<false>::fromNative(100 + static_cast<s25util::time64_t>(index));
    file.write(reinterpret_cast<const char*>(&time), sizeof(time));
    BOOST_TEST_REQUIRE(file.good());
}

struct LoadFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder userData;
    rttr::test::ConfigOverride userDataOverride{"USERDATA", userData};
    rttr::test::ConfigOverride gameOverride{"GAME", userData};
    const boost::filesystem::path saves = RTTRCONFIG.ExpandPath(s25::folders::save);

    LoadFixture() { boost::filesystem::create_directories(saves); }

    static dskFrontEndLoad& page()
    {
        auto* p = desktopAs<dskFrontEndLoad>();
        BOOST_TEST_REQUIRE(p);
        return *p;
    }
    void enter(unsigned count = 3)
    {
        for(unsigned i = 0; i < count; ++i)
            makeMetadata(saves / ("save" + std::to_string(i) + ".sav"), i);
        dskHome::ForgetLastChoice();
        WINDOWMANAGER.Switch(dskHome::Create());
        frame();
        pickUp(pad);
        chooseHome(dskHome::ID_Load);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    }
    void chooseHome(unsigned id)
    {
        for(unsigned n = 0; n < 15 && focusedId(0) != id; ++n)
            press(pad, focusedId(0) < id ? PadButton::RightShoulder : PadButton::LeftShoulder);
        BOOST_TEST_REQUIRE(focusedId(0) == id);
        press(pad, PadButton::A);
        frame();
    }
    void click(const Window& ctrl)
    {
        MouseCoords mouse(ctrl.GetDrawPos() + Position(ctrl.GetSize() / 2u));
        WINDOWMANAGER.Msg_MouseMove(mouse);
        mouse.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mouse);
        mouse.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mouse);
        frame();
    }
    void closeError()
    {
        auto* error = dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(error);
        click(*error->GetCtrl<Window>(2));
        frame();
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    }
};

struct ResumeFixture : rttr::test::LocalGameFixture, rttr::test::MenuPadFixture
{
    const uint16_t savedPort = SETTINGS.server.localPort;
    const boost::filesystem::path saves = RTTRCONFIG.ExpandPath(s25::folders::save);
    boost::filesystem::path savedGame;
    std::vector<BasePlayerInfo> savedPlayers;
    unsigned savedGF = 0;

    void frame() override
    {
        video.tickCount_ += frameMs;
        pump();
        WINDOWMANAGER.Draw();
    }
    void cleanup()
    {
        GAMECLIENT.Stop();
        GAMESERVER.Stop();
        GAMECLIENT.SetInterface(&ci());
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        frame();
        SETTINGS.server.localPort = savedPort;
        dskHome::ForgetLastChoice();
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
    template<class F>
    bool until(const F& done)
    {
        for(unsigned n = 0; n < 20000 && !done(); ++n)
        {
            frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return done();
    }
    void saveSolo()
    {
        hostAndEnterLobby();
        const unsigned numPlayers = GAMECLIENT.GetGameLobby()->getNumPlayers();
        for(unsigned i = 1; i < numPlayers; ++i)
            lobby().SetPlayerState(i, i == numPlayers - 1 ? PlayerState::Locked : PlayerState::AI,
                                   AI::Info(AI::Type::Dummy));
        BOOST_TEST_REQUIRE(until([numPlayers] {
            for(unsigned i = 1; i < numPlayers; ++i)
            {
                const PlayerState expected = i == numPlayers - 1 ? PlayerState::Locked : PlayerState::AI;
                if(GAMECLIENT.GetGameLobby()->getPlayer(i).ps != expected)
                    return false;
            }
            return true;
        }));
        GAMECLIENT.Command_SetReady(true);
        lobby().StartCountdown(0);
        BOOST_TEST_REQUIRE(until([] { return GAMECLIENT.GetState() == ClientState::Loading; }));
        GAMECLIENT.GameLoaded();
        BOOST_TEST_REQUIRE(until([] { return GAMECLIENT.GetState() == ClientState::Game; }));
        GAMECLIENT.OnGameStart();
        BOOST_TEST_REQUIRE(GAMECLIENT.GetAdditionalLocalPlayers().empty());
        BOOST_TEST_REQUIRE(GAMECLIENT.GetSharedLocalViews() == 0u);
        const unsigned target = GAMECLIENT.GetGFNumber() + 10;
        BOOST_TEST_REQUIRE(until([target] { return GAMECLIENT.GetGFNumber() >= target; }));
        boost::filesystem::create_directories(saves);
        savedGame = saves / "solo.sav";
        BOOST_TEST_REQUIRE(GAMECLIENT.SaveToFile(savedGame));
        Savegame save;
        {
            BinaryFile file;
            BOOST_TEST_REQUIRE(file.Open(savedGame, OpenFileMode::Read));
            BOOST_TEST_REQUIRE(save.Load(file, SaveGameDataToLoad::HeaderAndSettings));
            // Read the exact saved snapshot without Savegame's Debug rttrGameData.raw side effect.
            BOOST_TEST_REQUIRE(file.ReadUnsignedInt() == 1u);
            const unsigned rawSize = file.ReadUnsignedInt();
            std::vector<char> compressed(file.ReadUnsignedInt());
            file.ReadRawData(compressed.data(), compressed.size());
            const auto snapshot = CompressedData::decompress(compressed, rawSize);
            save.sgd.PushRawData(snapshot.data(), snapshot.size());
        }
        savedGF = save.start_gf;
        for(unsigned i = 0; i < save.GetNumPlayers(); ++i)
            savedPlayers.push_back(save.GetPlayer(i));
        BOOST_TEST_REQUIRE(savedPlayers.size() > 1u);
        BOOST_TEST_REQUIRE(savedPlayers[0].isHuman());
        BOOST_TEST_REQUIRE((savedPlayers[1].ps == PlayerState::AI));
        GAMECLIENT.Stop();
        GAMESERVER.Stop();
        GAMECLIENT.SetInterface(&ci());
        // Convert only the player-info header to upstream 4.0 (no portrait/start-goods fields).
        // The actual solo world is retained; it has no s25coop local seats or shared views.
        const auto legacy = saves / "legacy.sav";
        {
            BinaryFile file;
            BOOST_TEST_REQUIRE(file.Open(legacy, OpenFileMode::Write));
            file.WriteRawData("RTTRSV", 6);
            file.WriteUnsignedChar(4);
            file.WriteUnsignedChar(0);
            save.WriteExtHeader(file, save.GetMapName());
            Serializer players;
            players.PushUnsignedChar(save.GetNumPlayers());
            for(const auto& player : savedPlayers)
            {
                helpers::pushEnum<uint8_t>(players, player.ps);
                if(!player.isUsed())
                    continue;
                if(player.ps == PlayerState::AI)
                    player.aiInfo.serialize(players);
                players.PushLongString(player.name);
                helpers::pushEnum<uint8_t>(players, player.nation);
                players.PushUnsignedInt(player.color);
                helpers::pushEnum<uint8_t>(players, player.team);
            }
            players.WriteToFile(file);
            save.WriteGGS(file);
            // Uncompressed legacy encoding. The production reader supports this exact path.
            file.WriteUnsignedInt(save.sgd.GetLength());
            file.WriteRawData(save.sgd.GetData(), save.sgd.GetLength());
        }
        boost::filesystem::remove(savedGame);
        savedGame = legacy;
        Savegame reread;
        BOOST_TEST_REQUIRE(reread.Load(savedGame, SaveGameDataToLoad::All));
        BOOST_TEST(reread.GetMinorVersion() == 0u);
        BOOST_TEST(reread.start_gf == savedGF);
        BOOST_TEST_REQUIRE(reread.sgd.GetLength() == save.sgd.GetLength());
        BOOST_TEST(std::equal(reread.sgd.GetData(), reread.sgd.GetData() + reread.sgd.GetLength(), save.sgd.GetData()));
    }
    void joinParty(unsigned count)
    {
        dskHome::ForgetLastChoice();
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
    void chooseHome(unsigned id)
    {
        for(unsigned n = 0; n < 15 && focusedId(0) != id; ++n)
            press(pad, focusedId(0) < id ? PadButton::RightShoulder : PadButton::LeftShoulder);
        BOOST_TEST_REQUIRE(focusedId(0) == id);
        press(pad, PadButton::A);
        frame();
    }
    void openAndLoad(unsigned count, bool continueTile)
    {
        saveSolo();
        joinParty(count);
        Socket occupied;
        // Reserve both address families: Socket::Listen enables SO_REUSEADDR, which on Windows
        // can let the game bind the same port and connect to this silent listener instead.
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
        bool loaded = false;
        for(unsigned attempt = 0; attempt < 10 && !loaded; ++attempt)
        {
            // The first bind fails deliberately; subsequent attempts retry the real UI route.
            SETTINGS.server.localPort =
              attempt == 0 ? busyPort : static_cast<uint16_t>(rttr::test::randomValue(1024, 49151));
            chooseHome(continueTile ? dskHome::ID_Continue : dskHome::ID_Load);
            BOOST_TEST_REQUIRE(desktopAs<dskFrontEndLoad>());
            if(!continueTile)
            {
                BOOST_TEST_REQUIRE(focusedId(0) == dskFrontEndLoad::ID_Saves);
                // Two activations before paint must keep the same connecting modal.
                tap(pad, PadButton::A);
                tap(pad, PadButton::Start);
                frame();
            }
            if(attempt == 0)
                BOOST_TEST_REQUIRE(dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
            loaded = until([] {
                         return desktopAs<dskGameLobby>() != nullptr
                                || dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr;
                     })
                     && desktopAs<dskGameLobby>() != nullptr;
            if(!loaded)
            {
                occupied.Close();
                GAMECLIENT.Stop();
                GAMESERVER.Stop();
                GAMECLIENT.SetInterface(&ci());
                WINDOWMANAGER.Switch(dskHome::Create());
                frame();
            }
        }
        BOOST_TEST_REQUIRE(loaded);
        BOOST_TEST_REQUIRE(GAMECLIENT.GetGameLobby()->isSavegame());
        BOOST_TEST(until([count] { return GAMECLIENT.GetSharedLocalViews() == count - 1; }));
        BOOST_TEST(GAMECLIENT.GetAdditionalLocalPlayers().empty());
        const auto lobby = GAMECLIENT.GetGameLobby();
        BOOST_TEST_REQUIRE(lobby->getNumPlayers() == savedPlayers.size());
        for(unsigned i = 1; i < savedPlayers.size(); ++i)
        {
            const auto& actual = lobby->getPlayer(i);
            const auto& expected = savedPlayers[i];
            BOOST_TEST((actual.ps == expected.ps));
            BOOST_TEST((actual.aiInfo.type == expected.aiInfo.type));
            BOOST_TEST((actual.aiInfo.level == expected.aiInfo.level));
            BOOST_TEST(actual.color == expected.color);
            BOOST_TEST((actual.nation == expected.nation));
        }
        for(unsigned i = 0; i < count; ++i)
            BOOST_TEST(router().GetSlot(pad + i) == i);
        // Only the loading screen is bypassed: the mock data cannot draw original S2 resources.
        GAMECLIENT.SetInterface(&ci());
        press(pad, PadButton::Start);
        BOOST_TEST_REQUIRE(until([] { return GAMECLIENT.GetState() == ClientState::Loading; }));
        GAMECLIENT.GameLoaded();
        BOOST_TEST_REQUIRE(until([] { return GAMECLIENT.GetState() == ClientState::Game; }));
        GAMECLIENT.OnGameStart();
        BOOST_TEST_REQUIRE(ci().game);
        BOOST_TEST(world().GetNumPlayers() == savedPlayers.size());
        {
            const auto view = std::make_unique<rttr::test::TestableGameInterface>(ci().game, GAMECLIENT.GetNWFInfo(),
                                                                                  GAMECLIENT.GetPlayerId(), false);
            BOOST_TEST_REQUIRE(view->GetNumViews() == count);
            for(unsigned i = 0; i < count; ++i)
                BOOST_TEST(view->GetPlayerView(i).GetPlayerId() == GAMECLIENT.GetPlayerId());
        }
        GAMECLIENT.SetInterface(&ci());
        BOOST_TEST(GAMECLIENT.GetGFNumber() >= savedGF);
        const unsigned target = GAMECLIENT.GetGFNumber() + 12;
        BOOST_TEST_REQUIRE(until([target] { return GAMECLIENT.GetGFNumber() >= target; }));
        BOOST_TEST(ci().numErrors == 0u);
        BOOST_TEST(ci().numAsync == 0u);
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(FrontEndLoadTests)

BOOST_FIXTURE_TEST_CASE(EmptyBrowserHasNoLoadActionAndOneBackReturnsHome, LoadFixture)
{
    enter(0);
    BOOST_TEST(page().GetCatalog().entries.empty());
    BOOST_TEST(!page().GetCtrl<ctrlButton>(dskFrontEndLoad::ID_Load)->GetEnabled());
    BOOST_TEST(focusedId(0) == dskFrontEndLoad::ID_Refresh);
    BOOST_TEST(page().GetCtrl<ctrlText>(dskFrontEndLoad::ID_Status)->GetText() == "No saved games");
    press(pad, PadButton::A);
    WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Return));
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    press(pad, PadButton::B);
    frame();
    BOOST_TEST_REQUIRE(desktopAs<dskHome>());
    BOOST_TEST(focusedId(0) == dskHome::ID_Load);
}

BOOST_FIXTURE_TEST_CASE(PadBrowsesAllRowsAndDetailsTrackTheSelectedIdentity, LoadFixture)
{
    enter(80);
    BOOST_TEST(focusedId(0) == dskFrontEndLoad::ID_Saves);
    BOOST_TEST(page().GetSelectedPath() == saves / "save79.sav");
    BOOST_TEST(page().GetCtrl<ctrlText>(dskFrontEndLoad::ID_Map)->GetText() == "Map 79");
    BOOST_TEST(page().GetCtrl<ctrlText>(dskFrontEndLoad::ID_Players)->GetText() == "1 human tribes, 1 AI tribes");
    BOOST_TEST(page().GetCtrl<ctrlText>(dskFrontEndLoad::ID_Preview)->GetText() == "No preview stored");
    pressN(pad, PadButton::DpadDown, 79);
    BOOST_TEST(page().GetSelectedPath() == saves / "save0.sav");
    BOOST_TEST(page().GetCtrl<ctrlText>(dskFrontEndLoad::ID_Map)->GetText() == "Map 0");
    BOOST_TEST(page().GetCtrl<ctrlText>(dskFrontEndLoad::ID_Names)->GetText() == "Player 0, AI 0");
    pressN(pad, PadButton::DpadUp, 79);
    BOOST_TEST(page().GetSelectedPath() == saves / "save79.sav");
}

BOOST_FIXTURE_TEST_CASE(RefreshKeepsASelectionWhenNewerFilesAppearAndRecoversAfterRemoval, LoadFixture)
{
    enter();
    press(pad, PadButton::DpadDown);
    BOOST_TEST_REQUIRE(page().GetSelectedPath() == saves / "save1.sav");
    makeMetadata(saves / "newer.sav", 100);
    press(pad, PadButton::RightShoulder);
    press(pad, PadButton::RightShoulder);
    BOOST_TEST_REQUIRE(focusedId(0) == dskFrontEndLoad::ID_Refresh);
    press(pad, PadButton::A);
    BOOST_TEST(page().GetSelectedPath() == saves / "save1.sav");
    boost::filesystem::remove(saves / "save1.sav");
    press(pad, PadButton::A);
    BOOST_TEST(page().GetSelectedPath() == saves / "newer.sav");
    for(const auto& file : boost::filesystem::directory_iterator(saves))
        boost::filesystem::remove(file.path());
    press(pad, PadButton::A);
    BOOST_TEST(page().GetSelectedPath().empty());
    BOOST_TEST(!page().GetCtrl<ctrlButton>(dskFrontEndLoad::ID_Load)->GetEnabled());
    BOOST_TEST(!page().GetCtrl<ctrlText>(dskFrontEndLoad::ID_Map)->IsVisible());
}

BOOST_FIXTURE_TEST_CASE(DeletedAndIncompleteSavesFailOnTheBrowserWithoutSwitchingToMaps, LoadFixture)
{
    enter();
    boost::filesystem::remove(page().GetSelectedPath());
    press(pad, PadButton::A);
    BOOST_TEST_REQUIRE(desktopAs<dskFrontEndLoad>());
    BOOST_TEST(page().GetCatalog().entries.size() == 2u);
    closeError();
    // Metadata can become truncated while this page is still open.
    {
        boost::nowide::ofstream file(page().GetSelectedPath().string(), std::ios::binary);
        file << "broken now";
    }
    press(pad, PadButton::A);
    BOOST_TEST_REQUIRE(desktopAs<dskFrontEndLoad>());
    closeError();
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
}

BOOST_FIXTURE_TEST_CASE(MouseAndKeyboardChooseSavesAndEscapeReturnsToHome, LoadFixture)
{
    enter();
    disconnect(pad);
    frame();
    WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Down));
    BOOST_TEST(page().GetSelectedPath() == saves / "save1.sav");
    {
        const auto* table = page().GetCtrl<ctrlTable>(dskFrontEndLoad::ID_Saves);
        const int rowHeight = NormalFont->getHeight();
        MouseCoords mouse(table->GetDrawPos() + DrawPoint(20, rowHeight + 10 + (2 * rowHeight) + (rowHeight / 2)));
        WINDOWMANAGER.Msg_MouseMove(mouse);
        mouse.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mouse);
        mouse.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mouse);
        frame();
        BOOST_TEST(page().GetSelectedPath() == saves / "save0.sav");
    }
    WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Up));
    WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Up));
    BOOST_TEST(page().GetSelectedPath() == saves / "save2.sav");
    click(*page().GetCtrl<Window>(dskFrontEndLoad::ID_Refresh));
    BOOST_TEST(page().GetSelectedPath() == saves / "save2.sav");
    {
        boost::nowide::ofstream file(page().GetSelectedPath().string(), std::ios::binary);
        file << "broken now";
    }
    WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Return));
    frame();
    closeError();
    {
        boost::nowide::ofstream file(page().GetSelectedPath().string(), std::ios::binary);
        file << "broken now";
    }
    click(*page().GetCtrl<Window>(dskFrontEndLoad::ID_Load));
    closeError();
    WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
    frame();
    BOOST_TEST_REQUIRE(desktopAs<dskHome>());
}

BOOST_FIXTURE_TEST_CASE(ClassicLoadStillUsesItsOriginalDialog, LoadFixture)
{
    dskSinglePlayer::PrepareLoadGame();
    frame();
    BOOST_TEST(dynamic_cast<iwLoad*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
    BOOST_TEST(desktopAs<dskFrontEndLoad>() == nullptr);
}

BOOST_FIXTURE_TEST_CASE(SoloWorldWithLegacyPlayerHeaderResumesWithTwoJoinedPlayersAndKeepsAiTribes, ResumeFixture)
{
    checked([&] { openAndLoad(2, false); });
}

BOOST_FIXTURE_TEST_CASE(ContinueResumesSoloWorldWithLegacyPlayerHeaderAndFourPlayers, ResumeFixture)
{
    checked([&] { openAndLoad(4, true); });
}

BOOST_FIXTURE_TEST_CASE(DeletedContinueTargetDoesNotStartAnotherSave, LoadFixture)
{
    makeMetadata(saves / "older.sav", 1);
    makeMetadata(saves / "newest.sav", 2);
    dskHome::ForgetLastChoice();
    WINDOWMANAGER.Switch(dskHome::Create());
    frame();
    pickUp(pad);
    boost::filesystem::remove(saves / "newest.sav");
    chooseHome(dskHome::ID_Continue);
    BOOST_TEST_REQUIRE(desktopAs<dskFrontEndLoad>());
    BOOST_TEST(page().GetSelectedPath() == saves / "older.sav");
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
    closeError();
    // Reactivation after acknowledgement must not repeat Continue's automatic attempt.
    frame();
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    press(pad, PadButton::B);
    frame();
    BOOST_TEST_REQUIRE(desktopAs<dskHome>());
    BOOST_TEST(focusedId(0) == dskHome::ID_Continue);
}

BOOST_FIXTURE_TEST_CASE(LayoutKeepsTheListDetailsAndActionsAboveTheFooter, LoadFixture)
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
        enter(80);
        for(const VideoMode size : {VideoMode(800, 600), VideoMode(1280, 800), VideoMode(800, 480)})
        {
            VIDEODRIVER.setUiReferenceHeight(0);
            VIDEODRIVER.setGuiScalePercent(100);
            VIDEODRIVER.ResizeScreen(size, DisplayMode::Windowed);
            frame();
            const Rect& area = page().GetFrame().content;
            for(unsigned id = dskFrontEndLoad::ID_Saves; id <= dskFrontEndLoad::ID_Coop; ++id)
            {
                const auto* control = page().GetCtrl<Window>(id);
                BOOST_TEST_REQUIRE(control);
                const Rect rect = control->GetBoundaryRect();
                BOOST_TEST((rect.left >= area.left && rect.right <= area.right && rect.top >= area.top
                            && rect.bottom <= area.bottom));
            }
            press(pad, PadButton::DpadDown);
            BOOST_TEST(focusedId(0) == dskFrontEndLoad::ID_Saves);
        }
    });
    bool reachedProbe = false;
    BOOST_CHECK_THROW(checkedLayout([&] {
                          WINDOWMANAGER.Switch(
                            dskFrontEndPage::Create([] { return std::make_unique<dskFrontEndLoad>(); }));
                          frame();
                          VIDEODRIVER.ResizeScreen(VideoMode(1280, 800), DisplayMode::Windowed);
                          BOOST_TEST_REQUIRE(desktopAs<dskFrontEndLoad>());
                          reachedProbe = true;
                          cleanupProbe();
                      }),
                      std::runtime_error);
    BOOST_TEST(reachedProbe);
}

BOOST_FIXTURE_TEST_CASE(ExceptionalCleanupReachesTheConnectingProbeAndRestoresThePort, ResumeFixture)
{
    bool reachedProbe = false;
    BOOST_CHECK_THROW(checked([&] {
                          saveSolo();
                          joinParty(2);
                          SETTINGS.server.localPort = static_cast<uint16_t>(rttr::test::randomValue(1024, 49151));
                          chooseHome(dskHome::ID_Load);
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
