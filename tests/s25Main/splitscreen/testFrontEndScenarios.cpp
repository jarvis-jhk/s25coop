// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "LocalGameFixture.h"
#include "MenuPadFixture.h"
#include "PadFixture.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlComboBox.h"
#include "controls/ctrlMultiline.h"
#include "controls/ctrlPreviewMinimap.h"
#include "controls/ctrlTable.h"
#include "controls/ctrlText.h"
#include "desktops/dskFrontEndMaps.h"
#include "desktops/dskFrontEndMissions.h"
#include "desktops/dskGameLobby.h"
#include "desktops/dskHome.h"
#include "desktops/dskSinglePlayer.h"
#include "desktops/dskTitle.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "ingameWindows/iwConnecting.h"
#include "ingameWindows/iwMsgbox.h"
#include "ogl/glFont.h"
#include "postSystem/PostBox.h"
#include "postSystem/PostManager.h"
#include "libsiedler2/ArchivItem_Map.h"
#include "libsiedler2/ArchivItem_Map_Header.h"
#include "libsiedler2/libsiedler2.h"
#include "libsiedler2/prototypen.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include "s25util/Socket.h"
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <memory>

namespace {
constexpr PadDeviceId pad = 94;
using Browser = dskFrontEndMaps;
using Missions = dskFrontEndMissions;

[[noreturn]] void cleanupProbe()
{
    throw std::runtime_error("deliberate scenario-page cleanup probe");
}

struct ScenarioFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder data;
    rttr::test::ConfigOverride userOverride{"USERDATA", data / "user"};
    const boost::filesystem::path originalWorld = RTTRCONFIG.ExpandPath(s25::folders::gamedata) / "world";
    rttr::test::ConfigOverride rttrOverride{"RTTR", data / "rttr"};
    const CampaignSettings savedCampaigns = SETTINGS.campaigns;
    const uint16_t savedPort = SETTINGS.server.localPort;
    const decltype(SETTINGS.video) savedVideo = SETTINGS.video;
    const VideoMode savedSize = VIDEODRIVER.GetWindowSize();
    const DisplayMode savedMode = VIDEODRIVER.GetDisplayMode();

    ScenarioFixture()
    {
        const auto world = RTTRCONFIG.ExpandPath(s25::folders::gamedata) / "world";
        boost::filesystem::create_directories(world);
        for(boost::filesystem::directory_iterator it(originalWorld), end; it != end; ++it)
            boost::filesystem::copy_file(it->path(), world / it->path().filename());
    }

    boost::filesystem::path campaign(const std::string& name = "Test", const std::string& uid = "scenario-test",
                                     const bool multiple = false)
    {
        const auto folder = RTTRCONFIG.ExpandPath(s25::folders::campaignsUser) / name;
        boost::filesystem::create_directories(folder);
        {
            constexpr unsigned i = 1;
            const std::string mission = "Mission" + std::to_string(i);
            const auto path = folder / (mission + ".SWD");
            boost::filesystem::remove(path);
            boost::filesystem::copy_file(rttr::test::rttrBaseDir / "tests" / "testData" / "maps" / "LuaFunctions.SWD",
                                         path);
            boost::nowide::ofstream(folder / (mission + ".lua"))
              << "function getRequiredLuaVersion() return 1 end\nfunction onStart(first) rttr:SetMissionGoal(0,'"
              << mission << "') end\n";
        }
        boost::nowide::ofstream lua(folder / "campaign.lua");
        lua << "function getRequiredLuaVersion() return 1 end\ncampaign={uid='" << uid
            << "',version=1,author='Author',name='" << name
            << "',shortDescription='A single mission',longDescription='Scenario description',maxHumanPlayers=1,"
               "difficulty='easy',chaptersEnabled={0},maps={'Mission1.SWD'"
            << (multiple ? ",'Mission1.SWD'" : "") << "}}\n";
        BOOST_TEST_REQUIRE(lua.good());
        return folder;
    }
    void focus(unsigned id)
    {
        for(unsigned i = 0; i < 20 && focusedId(0) != id; ++i)
            press(pad, focusedId(0) < id ? PadButton::RightShoulder : PadButton::LeftShoulder);
        BOOST_TEST_REQUIRE(focusedId(0) == id);
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
    void key(KeyType type)
    {
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(type));
        frame();
    }
    void closeError()
    {
        auto* error = dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(error);
        click(*error->GetCtrl<Window>(2));
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    }
    static Browser& browser()
    {
        auto* result = desktopAs<Browser>();
        BOOST_TEST_REQUIRE(result);
        return *result;
    }
    void scenarios()
    {
        focus(Browser::ID_Category);
        auto* combo = browser().GetCtrl<ctrlComboBox>(Browser::ID_Category);
        press(pad, PadButton::A);
        BOOST_TEST_REQUIRE(combo->IsListOpen());
        pressN(pad, PadButton::DpadDown, Browser::scenarioCategory - *combo->GetSelection());
        press(pad, PadButton::A);
        BOOST_TEST(*combo->GetSelection() == Browser::scenarioCategory);
        if(!browser().GetSelectedPath().empty())
            focus(Browser::ID_Maps);
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
        BOOST_TEST_REQUIRE(desktopAs<Browser>());
        scenarios();
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    }
    void missions()
    {
        enter();
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Missions>());
        BOOST_TEST(focusedId(0) == Missions::ID_Missions);
    }
    void cleanup()
    {
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        frame();
        VIDEODRIVER.ResizeScreen(savedSize, savedMode);
        SETTINGS.video = savedVideo;
        BOOST_TEST((SETTINGS.video.windowedSize == savedVideo.windowedSize));
        SETTINGS.campaigns = savedCampaigns;
        SETTINGS.server.localPort = savedPort;
        dskHome::ForgetLastChoice();
        padInput().Reset();
        video.padEvents_.clear();
        BOOST_TEST((SETTINGS.campaigns.createSaveData() == savedCampaigns.createSaveData()));
        BOOST_TEST(SETTINGS.campaigns.shouldShowVictoryScreen() == savedCampaigns.shouldShowVictoryScreen());
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
};

struct StartFixture : rttr::test::LocalGameFixture, ScenarioFixture
{
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
        ScenarioFixture::cleanup();
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
    void host(unsigned count, unsigned input)
    {
        const auto folder = campaign();
        boost::filesystem::create_directories(RTTRCONFIG.ExpandPath(s25::folders::mapsPlayed));
        boost::filesystem::create_directories(RTTRCONFIG.ExpandPath(s25::folders::replays));
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
            focus(dskHome::ID_Maps);
            press(pad, PadButton::A);
            frame();
            BOOST_TEST_REQUIRE(desktopAs<Browser>());
            scenarios();
            if(input == 1)
                click(*browser().GetCtrl<Window>(Browser::ID_Continue));
            else if(input == 2)
                key(KeyType::Return);
            else if(input == 3)
            {
                const auto pos = browser().GetCtrl<ctrlTable>(Browser::ID_Maps)->GetDrawPos()
                                 + DrawPoint(20, NormalFont->getHeight() + 10 + NormalFont->getHeight() / 2);
                clickAt(pos);
                clickAt(pos);
            } else
            {
                tap(pad, PadButton::A);
                tap(pad, PadButton::A);
                frame();
            }
            frame();
            auto* page = desktopAs<Missions>();
            BOOST_TEST_REQUIRE(page);
            BOOST_TEST_REQUIRE(page->GetSelectedPath() == folder / "Mission1.SWD");
            if(input == 1)
                click(*page->GetCtrl<Window>(Missions::ID_Start));
            else if(input == 2)
                key(KeyType::Return);
            else if(input == 3)
            {
                const auto* table = page->GetCtrl<ctrlTable>(Missions::ID_Missions);
                const auto pos =
                  table->GetDrawPos() + DrawPoint(20, NormalFont->getHeight() + 10 + NormalFont->getHeight() / 2);
                clickAt(pos);
                BOOST_TEST_REQUIRE(page->GetSelectedPath() == folder / "Mission1.SWD");
                clickAt(pos);
            } else
            {
                tap(pad, PadButton::A);
                tap(pad, PadButton::A);
                frame();
            }
            if(attempt == 0)
                BOOST_TEST_REQUIRE(dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow()));
            if(auto* connecting = dynamic_cast<iwConnecting*>(WINDOWMANAGER.GetTopMostWindow()))
            {
                key(KeyType::Return);
                BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == connecting);
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
        BOOST_TEST(GAMECLIENT.IsHostingCampaign());
        BOOST_TEST(GAMECLIENT.GetMapPath().filename() == "Mission1.SWD");
        BOOST_TEST(GAMECLIENT.GetLuaFilePath().filename() == "Mission1.lua");
        BOOST_TEST_REQUIRE(until([count] { return GAMECLIENT.GetSharedLocalViews() == count - 1; }));
        BOOST_TEST(GAMECLIENT.GetAdditionalLocalPlayers().empty());
        for(unsigned i = 0; i < count; ++i)
            BOOST_TEST(router().GetSlot(pad + i) == i);
        GAMECLIENT.SetInterface(&ci());
        press(pad, PadButton::Start);
        BOOST_TEST_REQUIRE(until([] { return GAMECLIENT.GetState() == ClientState::Loading; }));
        BOOST_TEST_REQUIRE(ci().game);
        // Like dskGameLoader, construct the actual view (including its postbox) before GameLoaded/Start.
        const auto view = std::make_unique<rttr::test::TestableGameInterface>(ci().game, GAMECLIENT.GetNWFInfo(),
                                                                              GAMECLIENT.GetPlayerId(), false);
        GAMECLIENT.GameLoaded();
        BOOST_TEST_REQUIRE(until([] { return GAMECLIENT.GetState() == ClientState::Game; }));
        GAMECLIENT.OnGameStart();
        BOOST_TEST_REQUIRE(ci().game);
        BOOST_TEST(ci().game->IsStarted());
        const auto* box = ci().game->world_.GetPostMgr().GetPostBox(0);
        BOOST_TEST_REQUIRE(box);
        BOOST_TEST(box->GetCurrentMissionGoal() == "Mission1");
        BOOST_TEST_REQUIRE(view->GetNumViews() == count);
        for(unsigned i = 0; i < count; ++i)
            BOOST_TEST(view->GetPlayerView(i).GetPlayerId() == GAMECLIENT.GetPlayerId());
        GAMECLIENT.SetInterface(&ci());
        const unsigned gf = GAMECLIENT.GetGFNumber() + 12;
        BOOST_TEST_REQUIRE(until([gf] { return GAMECLIENT.GetGFNumber() >= gf; }));
        BOOST_TEST(ci().numErrors == 0u);
        BOOST_TEST(ci().numAsync == 0u);
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(FrontEndScenariosTests)

BOOST_FIXTURE_TEST_CASE(OnlyRegisteredSingleMissionsAppearAndBackRetainsBrowserIdentity, ScenarioFixture)
{
    checked([&] {
        const auto folder = campaign("Scenario");
        campaign("Many", "many", true);
        campaign("Zulu", "zulu");
        const auto maps = RTTRCONFIG.ExpandPath(s25::folders::mapsOwn);
        boost::filesystem::create_directories(maps);
        boost::filesystem::copy_file(folder / "Mission1.SWD", maps / "ordinary.SWD");
        boost::nowide::ofstream(maps / "ordinary.lua") << "error('must not execute during discovery')";
        enter();
        BOOST_TEST_REQUIRE(browser().GetCatalog().entries.size() == 2u);
        auto* table = browser().GetCtrl<ctrlTable>(Browser::ID_Maps);
        BOOST_TEST(!table->GetCtrl<ctrlButton>(1)->GetEnabled());
        BOOST_TEST(!table->GetCtrl<ctrlButton>(2)->GetEnabled());
        click(*table->GetCtrl<Window>(1));
        click(*table->GetCtrl<Window>(2));
        BOOST_TEST(table->GetItemText(0, 0) == "Scenario");
        BOOST_TEST(table->GetItemText(1, 0) == "Zulu");
        BOOST_TEST(browser().GetSelectedScenarioFolder() == folder);
        BOOST_TEST(browser().GetCtrl<ctrlText>(Browser::ID_MapName)->GetText() == "Scenario");
        BOOST_TEST(browser().GetCtrl<ctrlText>(Browser::ID_Script)->GetText() == "A single mission");
        BOOST_TEST(browser().GetCtrl<ctrlText>(Browser::ID_MapState)->GetText() == "Available");
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Missions>());
        BOOST_TEST(desktopAs<Missions>()->GetSelectedPath() == folder / "Mission1.SWD");
        press(pad, PadButton::B);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Browser>());
        BOOST_TEST(*browser().GetCtrl<ctrlComboBox>(Browser::ID_Category)->GetSelection() == Browser::scenarioCategory);
        BOOST_TEST(browser().GetSelectedScenarioFolder() == folder);
        BOOST_TEST(focusedId(0) == Browser::ID_Maps);
        press(pad, PadButton::B);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<dskHome>());
        BOOST_TEST(focusedId(0) == dskHome::ID_Maps);
    });
}

BOOST_FIXTURE_TEST_CASE(ScenarioFilterCountsMapTribesAndKeepsSelectionAcrossRefreshAndResize, ScenarioFixture)
{
    checked([&] {
        campaign("Alpha");
        const auto folder = campaign("Zulu", "zulu");
        enter();
        press(pad, PadButton::DpadDown);
        BOOST_TEST(browser().GetSelectedScenarioFolder() == folder);
        click(*browser().GetCtrl<Window>(Browser::ID_Refresh));
        BOOST_TEST(browser().GetSelectedScenarioFolder() == folder);
        focus(Browser::ID_PlayerFilter);
        press(pad, PadButton::A);
        pressN(pad, PadButton::DpadDown, 3);
        press(pad, PadButton::A);
        BOOST_TEST_REQUIRE(browser().GetCatalog().entries.size() == 2u);
        BOOST_TEST(browser().GetSelectedScenarioFolder() == folder);
        VIDEODRIVER.ResizeScreen(VideoMode(1280, 800), DisplayMode::Windowed);
        frame();
        BOOST_TEST(browser().GetSelectedScenarioFolder() == folder);
        focus(Browser::ID_Maps);
        key(KeyType::Return);
        BOOST_TEST_REQUIRE(desktopAs<Missions>());
        key(KeyType::Escape);
        BOOST_TEST_REQUIRE(desktopAs<Browser>());
        BOOST_TEST(*browser().GetCtrl<ctrlComboBox>(Browser::ID_PlayerFilter)->GetSelection() == 3u);
        BOOST_TEST(browser().GetSelectedScenarioFolder() == folder);
        focus(Browser::ID_PlayerFilter);
        press(pad, PadButton::A);
        press(pad, PadButton::DpadUp);
        press(pad, PadButton::A);
        BOOST_TEST(browser().GetCatalog().entries.empty());
        BOOST_TEST(browser().GetCtrl<ctrlText>(Browser::ID_Status)->GetText() == "No matching scenarios");
        key(KeyType::Return);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    });
}

BOOST_FIXTURE_TEST_CASE(CompletedAndLockedStateComesFromCampaignProgressWithoutWritingItOnBrowse, ScenarioFixture)
{
    checked([&] {
        campaign();
        SETTINGS.campaigns.setChapterCompleted("scenario-test", 0);
        const auto progress = SETTINGS.campaigns.createSaveData();
        enter();
        BOOST_TEST(browser().GetCtrl<ctrlText>(Browser::ID_MapState)->GetText() == "Completed");
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Missions>());
        BOOST_TEST(desktopAs<Missions>()->GetCtrl<ctrlButton>(Missions::ID_Start)->GetEnabled());
        BOOST_TEST((SETTINGS.campaigns.createSaveData() == progress));
    });
}

BOOST_FIXTURE_TEST_CASE(LockedScenarioCanBeInspectedButCannotStartUntilTheScriptUnlocksIt, ScenarioFixture)
{
    checked([&] {
        const auto folder = campaign();
        const auto path = folder / "campaign.lua";
        boost::nowide::ifstream input(path);
        std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        input.close();
        const auto enabled = contents.find("chaptersEnabled={0}");
        BOOST_TEST_REQUIRE(enabled != std::string::npos);
        contents.replace(enabled, std::string("chaptersEnabled={0}").size(), "chaptersEnabled={}");
        boost::nowide::ofstream(path) << contents;
        enter();
        BOOST_TEST(browser().GetCtrl<ctrlText>(Browser::ID_MapState)->GetText() == "Locked");
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Missions>());
        BOOST_TEST(!desktopAs<Missions>()->GetCtrl<ctrlButton>(Missions::ID_Start)->GetEnabled());
        press(pad, PadButton::A);
        key(KeyType::Return);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST_REQUIRE(desktopAs<Missions>());
        SETTINGS.campaigns.enableChapter("scenario-test", 0);
        click(*desktopAs<Missions>()->GetCtrl<Window>(Missions::ID_Refresh));
        BOOST_TEST(desktopAs<Missions>()->GetCtrl<ctrlButton>(Missions::ID_Start)->GetEnabled());
        key(KeyType::Escape);
        BOOST_TEST_REQUIRE(desktopAs<Browser>());
        BOOST_TEST(browser().GetCtrl<ctrlText>(Browser::ID_MapState)->GetText() == "Available");
    });
}

BOOST_FIXTURE_TEST_CASE(ReplacedRegistrationMissingScriptsAndExpandedCampaignRequireRefresh, ScenarioFixture)
{
    checked([&] {
        const auto folder = campaign();
        enter();
        campaign("Test", "replacement");
        press(pad, PadButton::A);
        closeError();
        BOOST_TEST_REQUIRE(desktopAs<Browser>());
        click(*browser().GetCtrl<Window>(Browser::ID_Refresh));
        campaign("Test", "replacement", true);
        focus(Browser::ID_Maps);
        key(KeyType::Return);
        closeError();
        campaign();
        click(*browser().GetCtrl<Window>(Browser::ID_Refresh));
        boost::filesystem::remove(folder / "Mission1.lua");
        focus(Browser::ID_Maps);
        press(pad, PadButton::A);
        closeError();
        click(*browser().GetCtrl<Window>(Browser::ID_Refresh));
        BOOST_TEST(browser().GetCatalog().entries.empty());
        BOOST_TEST(browser().GetCatalog().failures.size() == 1u);
        campaign();
        click(*browser().GetCtrl<Window>(Browser::ID_Refresh));
        BOOST_TEST(browser().GetSelectedScenarioFolder() == folder);
        focus(Browser::ID_Maps);
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Missions>());
    });
}

BOOST_FIXTURE_TEST_CASE(PageExceptionRestoresTemporaryCampaignState, ScenarioFixture)
{
    bool reached = false;
    BOOST_CHECK_THROW(checked([&] {
                          campaign();
                          missions();
                          BOOST_TEST_REQUIRE(desktopAs<Missions>());
                          SETTINGS.campaigns.setChapterCompleted("scenario-probe", 0);
                          reached = true;
                          cleanupProbe();
                      }),
                      std::runtime_error);
    BOOST_TEST_REQUIRE(reached);
    BOOST_TEST((SETTINGS.campaigns.createSaveData() == savedCampaigns.createSaveData()));
}

BOOST_FIXTURE_TEST_CASE(TwoControllersStartTheScenarioThroughAllFourPhysicalInputRoutes, StartFixture)
{
    for(unsigned input = 0; input < 4; ++input)
        checked([&] { host(2, input); });
}

BOOST_FIXTURE_TEST_CASE(FourControllersShareTheScenarioThroughAllFourPhysicalInputRoutes, StartFixture)
{
    for(unsigned input = 0; input < 4; ++input)
        checked([&] { host(4, input); });
}

BOOST_FIXTURE_TEST_CASE(ExceptionAfterRealStartStopsTheGameRestoresSettingsAndAllowsAnotherStart, StartFixture)
{
    bool reached = false;
    BOOST_CHECK_THROW(checked([&] {
                          host(2, 0);
                          BOOST_TEST_REQUIRE((GAMECLIENT.GetState() == ClientState::Game));
                          SETTINGS.campaigns.setChapterCompleted("scenario-start-probe", 0);
                          reached = true;
                          cleanupProbe();
                      }),
                      std::runtime_error);
    BOOST_TEST_REQUIRE(reached);
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
    BOOST_TEST((SETTINGS.campaigns.createSaveData() == savedCampaigns.createSaveData()));
    BOOST_TEST(SETTINGS.server.localPort == savedPort);
    checked([&] { host(2, 0); });
}

BOOST_AUTO_TEST_SUITE_END()
