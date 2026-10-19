// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "LocalGameFixture.h"
#include "MenuPadFixture.h"
#include "PadFixture.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlEdit.h"
#include "controls/ctrlMapSelection.h"
#include "controls/ctrlMultiline.h"
#include "controls/ctrlPreviewMinimap.h"
#include "controls/ctrlTable.h"
#include "controls/ctrlText.h"
#include "desktops/dskCampaignSelection.h"
#include "desktops/dskFrontEndCampaigns.h"
#include "desktops/dskFrontEndHost.h"
#include "desktops/dskFrontEndMissions.h"
#include "desktops/dskFrontEndOnline.h"
#include "desktops/dskHome.h"
#include "desktops/dskParty.h"
#include "desktops/dskSinglePlayer.h"
#include "desktops/dskTitle.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "ingameWindows/iwConnecting.h"
#include "ingameWindows/iwMsgbox.h"
#include "ogl/glFont.h"
#include "postSystem/PostBox.h"
#include "postSystem/PostManager.h"
#include "libsiedler2/ArchivItem_Bitmap_Raw.h"
#include "libsiedler2/ArchivItem_Map.h"
#include "libsiedler2/ArchivItem_Map_Header.h"
#include "libsiedler2/PixelBufferBGRA.h"
#include "libsiedler2/libsiedler2.h"
#include "libsiedler2/prototypen.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include "s25util/Socket.h"
#include <boost/algorithm/string/replace.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <iterator>
#include <memory>

namespace {
constexpr PadDeviceId pad = 94;
using Grid = dskFrontEndCampaigns;
using Missions = dskFrontEndMissions;

[[noreturn]] void cleanupProbe()
{
    throw std::runtime_error("deliberate campaign-page cleanup probe");
}

struct CampaignFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder data;
    rttr::test::ConfigOverride userOverride{"USERDATA", data / "user"};
    const boost::filesystem::path originalWorld = RTTRCONFIG.ExpandPath(s25::folders::gamedata) / "world";
    rttr::test::ConfigOverride rttrOverride{"RTTR", data / "rttr"};
    const CampaignSettings savedCampaigns = SETTINGS.campaigns;
    const uint16_t savedPort = SETTINGS.server.localPort;

    CampaignFixture()
    {
        const auto world = RTTRCONFIG.ExpandPath(s25::folders::gamedata) / "world";
        boost::filesystem::create_directories(world);
        for(boost::filesystem::directory_iterator it(originalWorld), end; it != end; ++it)
            boost::filesystem::copy_file(it->path(), world / it->path().filename());
    }

    boost::filesystem::path campaign(const std::string& name = "Test", const std::string& image = "",
                                     const bool reordered = false, const bool selectionMap = false)
    {
        const auto folder = RTTRCONFIG.ExpandPath(s25::folders::campaignsUser) / name;
        boost::filesystem::create_directories(folder);
        for(unsigned i = 1; i <= 3; ++i)
        {
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
        lua << "function getRequiredLuaVersion() return 1 end\ncampaign={uid='f4-" << name
            << "',version=1,author='Author',name='" << name
            << "',shortDescription='Coop',longDescription='Campaign description',maxHumanPlayers=1,difficulty='easy',"
               "chaptersEnabled={0,1},maps={"
            << (reordered ? "'Mission2.SWD','Mission1.SWD','Mission3.SWD'" :
                            "'Mission1.SWD','Mission2.SWD','Mission3.SWD'");
        lua << "}";
        if(!image.empty())
            lua << ",image='" << image << "'";
        if(selectionMap)
        {
            lua << ",selectionMap={";
            for(const auto* resource : {"background", "map", "missionMapMask", "marker", "conquered"})
                lua << resource << "={'preview.bmp',0},";
            lua << "backgroundOffset={0,0},disabledColor=0x70000000,missionSelectionInfos="
                   "{{0xff00ff00,1,1},{0xff0000ff,2,2},{0xffff0000,3,3}}}";
        }
        lua << "}\n";
        BOOST_TEST_REQUIRE(lua.good());
        return folder;
    }
    void image(const boost::filesystem::path& path, const uint32_t color)
    {
        auto bitmap = std::make_unique<libsiedler2::ArchivItem_Bitmap_Raw>();
        BOOST_TEST_REQUIRE(bitmap->create(libsiedler2::PixelBufferBGRA(8, 4, libsiedler2::ColorBGRA(color))) == 0);
        libsiedler2::Archiv archive;
        archive.push(std::move(bitmap));
        BOOST_TEST_REQUIRE(libsiedler2::Write(path, archive) == 0);
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
    void enter()
    {
        dskHome::ForgetLastChoice();
        WINDOWMANAGER.Switch(dskHome::Create());
        frame();
        pickUp(pad);
        focus(dskHome::ID_Campaigns);
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Grid>());
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

struct StartFixture : rttr::test::LocalGameFixture, CampaignFixture
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
        CampaignFixture::cleanup();
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
            focus(dskHome::ID_Campaigns);
            press(pad, PadButton::A);
            frame();
            BOOST_TEST_REQUIRE(desktopAs<Grid>());
            press(pad, PadButton::A);
            frame();
            auto* page = desktopAs<Missions>();
            BOOST_TEST_REQUIRE(page);
            press(pad, PadButton::DpadDown);
            BOOST_TEST_REQUIRE(page->GetSelectedPath() == folder / "Mission2.SWD");
            if(input == 1)
                click(*page->GetCtrl<Window>(Missions::ID_Start));
            else if(input == 2)
                key(KeyType::Return);
            else if(input == 3)
            {
                const auto* table = page->GetCtrl<ctrlTable>(Missions::ID_Missions);
                const auto pos =
                  table->GetDrawPos() + DrawPoint(20, NormalFont->getHeight() * 2 + 10 + NormalFont->getHeight() / 2);
                clickAt(pos);
                BOOST_TEST_REQUIRE(page->GetSelectedPath() == folder / "Mission2.SWD");
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
            ready =
              until([] { return desktopAs<dskParty>() || dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow()); })
              && desktopAs<dskParty>();
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
        BOOST_TEST(GAMECLIENT.GetMapPath().filename() == "Mission2.SWD");
        BOOST_TEST(GAMECLIENT.GetLuaFilePath().filename() == "Mission2.lua");
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
        BOOST_TEST(box->GetCurrentMissionGoal() == "Mission2");
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

BOOST_AUTO_TEST_SUITE(FrontEndCampaignsTests)

BOOST_FIXTURE_TEST_CASE(EmptyGridAndBackReturnToTheHomeCampaignTile, CampaignFixture)
{
    checked([&] {
        enter();
        BOOST_TEST(desktopAs<Grid>()->GetCatalog().entries.empty());
        BOOST_TEST(!desktopAs<Grid>()->GetCtrl<Window>(Grid::ID_FirstCampaign)->IsVisible());
        key(KeyType::Return);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        press(pad, PadButton::B);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<dskHome>());
        BOOST_TEST(focusedId(0) == dskHome::ID_Campaigns);
    });
}

BOOST_FIXTURE_TEST_CASE(ArtworkWithEqualStemsIsIndependentAndMissingPicturesAreOptional, CampaignFixture)
{
    checked([&] {
        const auto a = campaign("Alpha", "preview.bmp");
        const auto b = campaign("Beta", "preview.bmp");
        const auto c = campaign("Corrupt", "preview.bmp");
        campaign("Missing", "missing.bmp");
        campaign("Plain");
        image(a / "preview.bmp", 0xff00ff00);
        image(b / "preview.bmp", 0xff0000ff);
        boost::nowide::ofstream(c / "preview.bmp") << "not an image";
        enter();
        auto& page = *desktopAs<Grid>();
        BOOST_TEST_REQUIRE(page.GetCatalog().entries.size() == 5u);
        BOOST_TEST_REQUIRE(page.GetArtwork(0));
        BOOST_TEST_REQUIRE(page.GetArtwork(1));
        BOOST_TEST(page.GetArtwork(0) != page.GetArtwork(1));
        BOOST_TEST(page.GetArtwork(0)->getPixel(0, 0).asValue() == 0xff00ff00u);
        BOOST_TEST(page.GetArtwork(1)->getPixel(0, 0).asValue() == 0xff0000ffu);
        BOOST_TEST(page.GetArtwork(2) == nullptr);
        BOOST_TEST(page.GetArtwork(3) == nullptr);
        BOOST_TEST(page.GetArtwork(4) == nullptr);
        BOOST_TEST(page.GetArtwork(6) == nullptr);
        click(*page.GetCtrl<Window>(Grid::ID_FirstCampaign + 2));
        BOOST_TEST_REQUIRE(desktopAs<Missions>());
        BOOST_TEST(desktopAs<Missions>()->GetCtrl<ctrlButton>(Missions::ID_Start)->GetEnabled());
    });
}

BOOST_FIXTURE_TEST_CASE(PaginationKeyboardAndMissionBackPreserveCampaignIdentity, CampaignFixture)
{
    checked([&] {
        for(unsigned i = 0; i < 9; ++i)
            campaign("Campaign" + std::to_string(i));
        enter();
        focus(Grid::ID_Next);
        press(pad, PadButton::A);
        BOOST_TEST(desktopAs<Grid>()->GetFirstCampaign() == 6u);
        click(*desktopAs<Grid>()->GetCtrl<Window>(Grid::ID_Previous));
        BOOST_TEST(desktopAs<Grid>()->GetFirstCampaign() == 0u);
        for(unsigned i = 0; i < 8; ++i)
            key(KeyType::Right);
        BOOST_TEST(desktopAs<Grid>()->GetFirstCampaign() == 6u);
        key(KeyType::Left);
        key(KeyType::Down);
        key(KeyType::Return);
        BOOST_TEST_REQUIRE(desktopAs<Missions>());
        BOOST_TEST(desktopAs<Missions>()->GetSelectedPath().parent_path().filename() == "Campaign8");
        press(pad, PadButton::B);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Grid>());
        BOOST_TEST(desktopAs<Grid>()->GetFirstCampaign() == 6u);
        BOOST_TEST(focusedId(0) == Grid::ID_FirstCampaign + 2);
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Missions>());
        BOOST_TEST(desktopAs<Missions>()->GetSelectedPath().parent_path().filename() == "Campaign8");
        press(pad, PadButton::B);
        frame();
        click(*desktopAs<Grid>()->GetCtrl<Window>(Grid::ID_Refresh));
        BOOST_TEST(desktopAs<Grid>()->GetFirstCampaign() == 6u);
        key(KeyType::Escape);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<dskHome>());
    });
}

BOOST_FIXTURE_TEST_CASE(ContinentArtworkIsOptionalAndTheMissionListKeepsUnlocks, CampaignFixture)
{
    checked([&] {
        const auto folder = campaign("Map", "", false, true);
        image(folder / "preview.bmp", 0xffff0000);
        enter();
        BOOST_TEST_REQUIRE(desktopAs<Grid>()->GetArtwork(0));
        BOOST_TEST(desktopAs<Grid>()->GetArtwork(0)->getPixel(0, 0).asValue() == 0xffff0000u);
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Missions>());
        pressN(pad, PadButton::DpadDown, 2);
        BOOST_TEST(!desktopAs<Missions>()->GetCtrl<ctrlButton>(Missions::ID_Start)->GetEnabled());
        press(pad, PadButton::B);
        frame();
        boost::filesystem::remove(folder / "preview.bmp");
        click(*desktopAs<Grid>()->GetCtrl<Window>(Grid::ID_Refresh));
        BOOST_TEST_REQUIRE(desktopAs<Grid>()->GetCatalog().entries.size() == 1u);
        BOOST_TEST(desktopAs<Grid>()->GetArtwork(0) == nullptr);
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Missions>());
        BOOST_TEST(desktopAs<Missions>()->GetCtrl<ctrlButton>(Missions::ID_Start)->GetEnabled());
    });
}

BOOST_FIXTURE_TEST_CASE(ContinentMapPicksMissionsByClickAndPadAndKeepsLockedContinentsOut, CampaignFixture)
{
    checked([&] {
        const auto folder = campaign("Continents", "", false, true);
        // Own file stems: the loader caches selection-map images by stem, and other cases use preview.bmp.
        std::string lua;
        {
            boost::nowide::ifstream in(folder / "campaign.lua");
            lua.assign(std::istreambuf_iterator<char>(in), {});
        }
        boost::algorithm::replace_all(lua, "missionMapMask={'preview.bmp'", "missionMapMask={'continentmask.bmp'");
        boost::algorithm::replace_all(lua, "'preview.bmp'", "'continents.bmp'");
        boost::nowide::ofstream(folder / "campaign.lua") << lua;
        image(folder / "continents.bmp", 0xff808080);
        // Three continents side by side in the mask, in the order of missionSelectionInfos.
        libsiedler2::PixelBufferBGRA pixels(8, 4);
        for(unsigned y = 0; y < 4; ++y)
            for(unsigned x = 0; x < 8; ++x)
                pixels.set(x, y, libsiedler2::ColorBGRA(x < 3 ? 0xff00ff00 : (x < 6 ? 0xff0000ff : 0xffff0000)));
        auto mask = std::make_unique<libsiedler2::ArchivItem_Bitmap_Raw>();
        BOOST_TEST_REQUIRE(mask->create(pixels) == 0);
        libsiedler2::Archiv archive;
        archive.push(std::move(mask));
        BOOST_TEST_REQUIRE(libsiedler2::Write(folder / "continentmask.bmp", archive) == 0);

        missions();
        auto* page = desktopAs<Missions>();
        auto* map = page->GetCtrl<ctrlMapSelection>(Missions::ID_ContinentMap);
        BOOST_TEST_REQUIRE(map);
        BOOST_TEST(map->IsVisible());
        BOOST_TEST(!page->GetCtrl<Window>(Missions::ID_Preview)->IsVisible());
        BOOST_TEST((map->getSelection() == 0u));

        const auto continent = [&](unsigned column) {
            const auto size = map->GetSize();
            const float scale = std::min(size.x / 8.f, size.y / 4.f);
            const DrawPoint origin =
              map->GetDrawPos()
              + DrawPoint((size - Extent(static_cast<unsigned>(8 * scale), static_cast<unsigned>(4 * scale))) / 2u);
            return origin + DrawPoint(static_cast<int>((column + 0.5f) * scale), static_cast<int>(2 * scale));
        };
        clickAt(continent(4));
        BOOST_TEST(page->GetSelectedPath().filename() == "Mission2.SWD");
        BOOST_TEST((map->getSelection() == 1u));
        BOOST_TEST(page->GetCtrl<ctrlButton>(Missions::ID_Start)->GetEnabled());
        // The third continent is still locked: clicking it changes nothing.
        clickAt(continent(7));
        BOOST_TEST(page->GetSelectedPath().filename() == "Mission2.SWD");

        // The pad walks the playable continents and the list follows.
        focus(Missions::ID_ContinentMap);
        press(pad, PadButton::DpadLeft);
        BOOST_TEST((map->getSelection() == 0u));
        BOOST_TEST(page->GetSelectedPath().filename() == "Mission1.SWD");

        // Choosing a locked mission in the list takes the marker off the map.
        focus(Missions::ID_Missions);
        pressN(pad, PadButton::DpadDown, 2);
        BOOST_TEST(page->GetSelectedPath().filename() == "Mission3.SWD");
        BOOST_TEST(!map->getSelection());

        // Unlocking it on refresh lights the continent up and the click now picks it.
        SETTINGS.campaigns.setChapterCompleted("f4-Continents", 1);
        SETTINGS.campaigns.enableChapter("f4-Continents", 2);
        click(*page->GetCtrl<Window>(Missions::ID_Refresh));
        map = page->GetCtrl<ctrlMapSelection>(Missions::ID_ContinentMap);
        BOOST_TEST_REQUIRE(map);
        BOOST_TEST((map->getSelection() == 2u));
        clickAt(continent(0));
        BOOST_TEST(page->GetSelectedPath().filename() == "Mission1.SWD");
        clickAt(continent(7));
        BOOST_TEST(page->GetSelectedPath().filename() == "Mission3.SWD");
        BOOST_TEST(page->GetCtrl<ctrlButton>(Missions::ID_Start)->GetEnabled());
    });
}

BOOST_FIXTURE_TEST_CASE(IncompleteSelectedLayersAreRejectedAndRefreshRecoversTheSameMission, CampaignFixture)
{
    checked([&] {
        const auto folder = campaign();
        missions();
        press(pad, PadButton::DpadDown);
        auto* page = desktopAs<Missions>();
        const auto path = page->GetSelectedPath();
        libsiedler2::Archiv archive;
        BOOST_TEST_REQUIRE(libsiedler2::loader::LoadMAP(path, archive, true) == 0);
        const auto* map = dynamic_cast<const libsiedler2::ArchivItem_Map*>(archive[0]);
        BOOST_TEST_REQUIRE(map);
        {
            boost::nowide::ofstream stream(path, std::ios::binary);
            BOOST_TEST_REQUIRE(map->getHeader().write(stream) == 0);
        }
        key(KeyType::Return);
        closeError();
        BOOST_TEST(page->GetSelectedPath() == folder / "Mission2.SWD");
        BOOST_TEST(!page->GetCtrl<ctrlButton>(Missions::ID_Start)->GetEnabled());
        BOOST_TEST(page->GetCtrl<ctrlPreviewMinimap>(Missions::ID_Preview)->GetCurMapSize().x == 0u);
        campaign();
        click(*page->GetCtrl<Window>(Missions::ID_Refresh));
        BOOST_TEST(page->GetSelectedPath() == folder / "Mission2.SWD");
        BOOST_TEST(page->GetCtrl<ctrlButton>(Missions::ID_Start)->GetEnabled());
    });
}

BOOST_FIXTURE_TEST_CASE(ADeletedCampaignShowsAnUnavailablePageAndClassicKeepsItsChooser, CampaignFixture)
{
    checked([&] {
        const auto folder = campaign();
        enter();
        boost::filesystem::remove_all(folder);
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Missions>());
        BOOST_TEST(desktopAs<Missions>()->GetSelectedPath().empty());
        key(KeyType::Return);
        click(*desktopAs<Missions>()->GetCtrl<Window>(Missions::ID_Refresh));
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        key(KeyType::Escape);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Grid>());
        BOOST_TEST(desktopAs<Grid>()->GetCatalog().entries.empty());
        dskSinglePlayer::OpenCampaigns();
        frame();
        BOOST_TEST_REQUIRE(desktopAs<dskCampaignSelection>());
        BOOST_TEST(desktopAs<Grid>() == nullptr);
    });
}

BOOST_FIXTURE_TEST_CASE(GridAndMissionControlsStayInsideTheContentAfterRealResizes, CampaignFixture)
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
            checked(body);
        } catch(...)
        {
            restore();
            throw;
        }
        restore();
    };
    checkedLayout([&] {
        for(unsigned i = 0; i < 6; ++i)
            campaign("Campaign" + std::to_string(i));
        for(const VideoMode size : {VideoMode(800, 600), VideoMode(1280, 800), VideoMode(800, 480)})
        {
            VIDEODRIVER.setUiReferenceHeight(0);
            VIDEODRIVER.setGuiScalePercent(100);
            VIDEODRIVER.ResizeScreen(size, DisplayMode::Windowed);
            enter();
            const auto check = [](const dskFrontEndPage& page, unsigned first, unsigned last) {
                const auto area = page.GetFrame().content;
                for(unsigned id = first; id <= last; ++id)
                {
                    const auto* control = page.GetCtrl<Window>(id);
                    BOOST_TEST_REQUIRE(control->IsVisible());
                    const auto rect = control->GetBoundaryRect();
                    BOOST_TEST((rect.left >= area.left && rect.right <= area.right && rect.top >= area.top
                                && rect.bottom <= area.bottom));
                }
            };
            check(*desktopAs<Grid>(), Grid::ID_FirstCampaign, Grid::ID_Refresh);
            press(pad, PadButton::A);
            frame();
            BOOST_TEST_REQUIRE(desktopAs<Missions>());
            check(*desktopAs<Missions>(), Missions::ID_Missions, Missions::ID_Preview);
            VIDEODRIVER.ResizeScreen(VideoMode(size.width + 8, size.height + 8), DisplayMode::Windowed);
            frame();
            check(*desktopAs<Missions>(), Missions::ID_Missions, Missions::ID_Preview);
        }
        VIDEODRIVER.ResizeScreen(VideoMode(480, 240), DisplayMode::Windowed);
        enter();
        BOOST_TEST_REQUIRE(desktopAs<Grid>()->GetFrame().content.getSize().x < 480);
        BOOST_TEST(!desktopAs<Grid>()->GetCtrl<Window>(Grid::ID_FirstCampaign)->IsVisible());
        key(KeyType::Return);
        BOOST_TEST_REQUIRE(desktopAs<Grid>());
        VIDEODRIVER.ResizeScreen(VideoMode(808, 608), DisplayMode::Windowed);
        frame();
        BOOST_TEST(desktopAs<Grid>()->GetCtrl<Window>(Grid::ID_FirstCampaign)->IsVisible());
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Missions>());
    });
    bool reached = false;
    BOOST_CHECK_THROW(checkedLayout([&] {
                          missions();
                          VIDEODRIVER.ResizeScreen(VideoMode(1280, 800), DisplayMode::Windowed);
                          BOOST_TEST_REQUIRE(desktopAs<Missions>());
                          reached = true;
                          cleanupProbe();
                      }),
                      std::runtime_error);
    BOOST_TEST_REQUIRE(reached);
}

BOOST_FIXTURE_TEST_CASE(LockedMissionsCannotStartAndCompletedMissionsRemainPlayable, CampaignFixture)
{
    checked([&] {
        campaign();
        missions();
        auto* page = desktopAs<Missions>();
        auto* table = page->GetCtrl<ctrlTable>(Missions::ID_Missions);
        BOOST_TEST_REQUIRE(table->GetNumRows() == 3u);
        BOOST_TEST(table->GetItemText(2, 1) == "Locked");
        pressN(pad, PadButton::DpadDown, 2);
        BOOST_TEST_REQUIRE(page->GetSelectedPath().filename() == "Mission3.SWD");
        BOOST_TEST(!page->GetCtrl<ctrlButton>(Missions::ID_Start)->GetEnabled());
        press(pad, PadButton::A);
        key(KeyType::Return);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        SETTINGS.campaigns.setChapterCompleted("f4-Test", 1);
        SETTINGS.campaigns.enableChapter("f4-Test", 2);
        click(*page->GetCtrl<Window>(Missions::ID_Refresh));
        BOOST_TEST(page->GetSelectedPath().filename() == "Mission3.SWD");
        BOOST_TEST(table->GetItemText(1, 1) == "Completed");
        BOOST_TEST(table->GetItemText(2, 1) == "Available");
        BOOST_TEST(page->GetCtrl<ctrlButton>(Missions::ID_Start)->GetEnabled());
        // Current progress is checked at activation, even if the displayed row was available.
        SETTINGS.campaigns.readSaveData("f4-Test", "110");
        press(pad, PadButton::A);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(!page->GetCtrl<ctrlButton>(Missions::ID_Start)->GetEnabled());
        key(KeyType::Up);
        BOOST_TEST(page->GetSelectedPath().filename() == "Mission2.SWD");
        BOOST_TEST(page->GetCtrl<ctrlText>(Missions::ID_State)->GetText() == "Available");
    });
}

BOOST_FIXTURE_TEST_CASE(MissingAndChangedCampaignFilesRecoverWithoutStartingAnotherMission, CampaignFixture)
{
    checked([&] {
        const auto folder = campaign();
        missions();
        auto* page = desktopAs<Missions>();
        press(pad, PadButton::DpadDown);
        campaign("Test", "", true);
        key(KeyType::Return);
        closeError();
        BOOST_TEST(page->GetSelectedPath().filename() == "Mission2.SWD");
        click(*page->GetCtrl<Window>(Missions::ID_Refresh));
        BOOST_TEST(page->GetSelectedPath().filename() == "Mission2.SWD");
        boost::filesystem::remove(folder / "Mission2.lua");
        key(KeyType::Return);
        closeError();
        click(*page->GetCtrl<Window>(Missions::ID_Refresh));
        BOOST_TEST(page->GetSelectedPath().empty());
        BOOST_TEST(!page->GetCtrl<ctrlButton>(Missions::ID_Start)->GetEnabled());
        campaign();
        click(*page->GetCtrl<Window>(Missions::ID_Refresh));
        BOOST_TEST(page->GetSelectedPath().filename() == "Mission1.SWD");
        BOOST_TEST(page->GetCtrl<ctrlButton>(Missions::ID_Start)->GetEnabled());
    });
}

BOOST_FIXTURE_TEST_CASE(CleanupAfterAnExceptionRestoresProgressWhileTheMissionPageWasOpen, CampaignFixture)
{
    bool reached = false;
    BOOST_CHECK_THROW(checked([&] {
                          campaign();
                          missions();
                          SETTINGS.campaigns.setChapterCompleted("f4-probe", 2);
                          BOOST_TEST_REQUIRE(desktopAs<Missions>());
                          reached = true;
                          cleanupProbe();
                      }),
                      std::runtime_error);
    BOOST_TEST_REQUIRE(reached);
    BOOST_TEST((SETTINGS.campaigns.createSaveData() == savedCampaigns.createSaveData()));
}

BOOST_FIXTURE_TEST_CASE(TwoControllersStartTheChosenCampaignMissionThroughEveryInput, StartFixture)
{
    for(unsigned input = 0; input < 4; ++input)
        checked([&] { host(2, input); });
}

BOOST_FIXTURE_TEST_CASE(FourControllersShareTheCampaignTribeThroughEveryInput, StartFixture)
{
    for(unsigned input = 0; input < 4; ++input)
        checked([&] { host(4, input); });
}

BOOST_FIXTURE_TEST_CASE(ExceptionAfterCampaignStartStopsTheGameAndRestoresSettings, StartFixture)
{
    bool reached = false;
    BOOST_CHECK_THROW(checked([&] {
                          host(2, 0);
                          BOOST_TEST_REQUIRE((GAMECLIENT.GetState() == ClientState::Game));
                          SETTINGS.campaigns.setChapterCompleted("f4-start-probe", 2);
                          reached = true;
                          cleanupProbe();
                      }),
                      std::runtime_error);
    BOOST_TEST_REQUIRE(reached);
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    BOOST_TEST((SETTINGS.campaigns.createSaveData() == savedCampaigns.createSaveData()));
    BOOST_TEST(SETTINGS.server.localPort == savedPort);
    // A fresh real start also checks that exceptional cleanup released the server and its lobby state.
    checked([&] { host(2, 0); });
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
}

BOOST_FIXTURE_TEST_CASE(NetworkCampaignUsesTheSelectedMissionAndAllowsHostTribeCoPlayers, StartFixture)
{
    checked([&] {
        const auto folder = campaign();
        dskHome::ForgetLastChoice();
        WINDOWMANAGER.Switch(dskHome::Create());
        frame();
        pickUp(pad);
        focus(dskHome::ID_Online);
        press(pad, PadButton::A);
        frame();
        focus(dskFrontEndOnline::ID_Host);
        press(pad, PadButton::A);
        frame();
        focus(dskFrontEndHost::ID_Campaigns);
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Grid>());
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Missions>());
        press(pad, PadButton::DpadDown);
        BOOST_TEST(desktopAs<Missions>()->GetSelectedPath() == folder / "Mission2.SWD");
        press(pad, PadButton::A);
        frame();
        auto* form = desktopAs<dskFrontEndHost>();
        BOOST_TEST_REQUIRE(form);
        // A campaign replaced while its hosting form is open cannot silently start another mission.
        campaign("Test", "", true);
        click(*form->GetCtrl<Window>(dskFrontEndHost::ID_Start));
        BOOST_TEST(form->GetCtrl<ctrlText>(dskFrontEndHost::ID_Status)->GetText()
                   == "Campaign changed. Refresh before choosing a mission.");
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
        campaign();
        Socket reservation;
        BOOST_TEST_REQUIRE(reservation.Listen(0, false, false));
        sockaddr_in address{};
        socklen_t length = sizeof(address);
        BOOST_TEST_REQUIRE(getsockname(reservation.GetSocket(), reinterpret_cast<sockaddr*>(&address), &length) == 0);
        const auto port = std::to_string(ntohs(address.sin_port));
        reservation.Close();
        auto* edit = form->GetCtrl<ctrlEdit>(dskFrontEndHost::ID_Port);
        click(*edit);
        key(KeyType::End);
        const auto count = edit->GetText().size();
        for(unsigned i = 0; i < count; ++i)
            key(KeyType::Backspace);
        for(const char c : port)
            WINDOWMANAGER.Msg_KeyDown(KeyEvent(static_cast<char32_t>(c)));
        click(*form->GetCtrl<Window>(dskFrontEndHost::ID_Start));
        // F7e: a network campaign we host gets the full-screen party page; the mission script owns the rules.
        BOOST_TEST_REQUIRE(until([] { return desktopAs<dskParty>() != nullptr; }));
        BOOST_TEST(desktopAs<dskParty>()->IsOnline());
        BOOST_TEST(!desktopAs<dskParty>()->HasRules());
        BOOST_TEST(desktopAs<dskParty>()->GetCtrl<ctrlText>(dskParty::ID_Mode)->GetText()
                   == "Everyone plays the campaign's tribe together.");
        BOOST_TEST(GAMECLIENT.IsHostingCampaign());
        BOOST_TEST(GAMECLIENT.GetLuaFilePath().filename() == "Mission2.lua");
        BOOST_TEST(GAMECLIENT.GetMapPath().filename() == "Mission2.SWD");
        BOOST_TEST_REQUIRE(until([] { return GAMECLIENT.AreCoopMembersAllowed(); }));
    });
}

BOOST_AUTO_TEST_SUITE_END()
