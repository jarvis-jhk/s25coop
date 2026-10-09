// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MenuPadFixture.h"
#include "PointOutput.h"
#include "WindowManager.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlPreviewMinimap.h"
#include "controls/ctrlTable.h"
#include "controls/ctrlText.h"
#include "desktops/dskDirectIP.h"
#include "desktops/dskLAN.h"
#include "desktops/dskSelectMap.h"
#include "desktops/dskSinglePlayer.h"
#include "driver/MouseCoords.h"
#include "helpers/OptionalIO.h"
#include "ingameWindows/IngameWindow.h"
#include "libsiedler2/ArchivItem_Map.h"
#include "libsiedler2/ArchivItem_Map_Header.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <memory>

namespace {
constexpr PadDeviceId pad = 91;

struct MapReturnFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder userData;
    rttr::test::ConfigOverride userDataOverride{"USERDATA", userData};

    void enter(const ServerType type)
    {
        padInput().Reset();
        WINDOWMANAGER.Switch(std::make_unique<dskSelectMap>(CreateServerInfo(type, 12345, "Controller return")));
        frame();
        pickUp(pad);
        BOOST_TEST_REQUIRE(desktopAs<dskSelectMap>() != nullptr);
        BOOST_TEST_REQUIRE(router().GetSlot(pad) == 0u);
    }

    void click(const DrawPoint& position)
    {
        MouseCoords mc(position);
        WINDOWMANAGER.Msg_MouseMove(mc);
        mc.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mc);
        mc.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mc);
        frame();
    }

    void writeMap(const std::string& filename, const std::string& name, const uint16_t width = 32) const
    {
        const auto folder = userData / "MAPS";
        boost::filesystem::create_directories(folder);
        auto header = std::make_unique<libsiedler2::ArchivItem_Map_Header>();
        header->setName(name);
        header->setAuthor("Catalog author");
        header->setNumPlayers(2);
        header->setWidth(width);
        header->setHeight(32);
        libsiedler2::ArchivItem_Map map;
        map.init(std::move(header));
        for(const auto layer : {libsiedler2::MapLayer::Terrain1, libsiedler2::MapLayer::Terrain2})
            std::fill(map.getLayer(layer).begin(), map.getLayer(layer).end(), 5);
        boost::nowide::ofstream stream((folder / filename).string(), std::ios::binary);
        BOOST_TEST_REQUIRE(map.write(stream) == 0);
    }

    void showPlayedMaps()
    {
        enter(ServerType::Local);
        click(DrawPoint(50, 245)); // Played: the isolated USERDATA/MAPS folder.
        BOOST_TEST_REQUIRE(desktopAs<dskSelectMap>());
    }

    bool isExpectedParent(const ServerType type) const
    {
        if(type == ServerType::Local)
            return desktopAs<dskSinglePlayer>() != nullptr;
        if(type == ServerType::LAN)
            return desktopAs<dskLAN>() != nullptr;
        // The fixture is not logged into the public lobby: its existing fallback is Direct-IP.
        return desktopAs<dskDirectIP>() != nullptr;
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(MenuPadMapReturnTests)

BOOST_FIXTURE_TEST_CASE(BReturnsToTheOriginalMenu, MapReturnFixture)
{
    for(const auto type : {ServerType::Local, ServerType::Direct, ServerType::LAN, ServerType::Lobby})
    {
        enter(type);
        press(pad, PadButton::B);
        BOOST_TEST_REQUIRE(isExpectedParent(type));
        BOOST_TEST(video.padEvents_.empty());
    }
}

BOOST_FIXTURE_TEST_CASE(MouseBackRetainsTheSameDestinations, MapReturnFixture)
{
    for(const auto type : {ServerType::Local, ServerType::Direct, ServerType::LAN, ServerType::Lobby})
    {
        enter(type);
        MouseCoords mc(Position(450, 570));
        WINDOWMANAGER.Msg_MouseMove(mc);
        mc.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mc);
        mc.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mc);
        frame();
        BOOST_TEST_REQUIRE(isExpectedParent(type));
    }
}

BOOST_FIXTURE_TEST_CASE(BClosesTheLoadDialogBeforeLeavingCreateGame, MapReturnFixture)
{
    enter(ServerType::Local);
    for(unsigned i = 0; i < 20 && focusedId(0) != 4u; ++i)
        press(pad, PadButton::RightShoulder);
    BOOST_TEST_REQUIRE(focusedId(0) == 4u); // Load game
    press(pad, PadButton::A);
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() != nullptr);
    press(pad, PadButton::B);
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    BOOST_TEST_REQUIRE(desktopAs<dskSelectMap>() != nullptr);
    press(pad, PadButton::B);
    BOOST_TEST(desktopAs<dskSinglePlayer>() != nullptr);
}

BOOST_FIXTURE_TEST_CASE(StartAndNavigationDoNotLeaveCreateGame, MapReturnFixture)
{
    enter(ServerType::Local);
    for(const auto button : {PadButton::Start, PadButton::DpadLeft, PadButton::RightShoulder})
    {
        press(pad, button);
        BOOST_TEST_REQUIRE(desktopAs<dskSelectMap>() != nullptr);
        BOOST_TEST(video.padEvents_.empty());
    }
}

BOOST_FIXTURE_TEST_CASE(BDoesNotDismissCustomMapSettingsOrLeaveCreateGame, MapReturnFixture)
{
    enter(ServerType::Local);
    for(unsigned i = 0; i < 20 && focusedId(0) != 7u; ++i)
        press(pad, PadButton::RightShoulder);
    BOOST_TEST_REQUIRE(focusedId(0) == 7u); // Random-map settings require explicit confirmation.
    press(pad, PadButton::A);
    const auto* settingsWindow = WINDOWMANAGER.GetTopMostWindow();
    BOOST_TEST_REQUIRE(settingsWindow != nullptr);
    press(pad, PadButton::B);
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == settingsWindow);
    BOOST_TEST(desktopAs<dskSelectMap>() != nullptr);
}

BOOST_FIXTURE_TEST_CASE(CatalogMetadataRetainsClassicMouseAndPadPreview, MapReturnFixture)
{
    writeMap("one.SwD", "Alpha");
    writeMap("two.WLD", "Zulu", 64);
    boost::nowide::ofstream((userData / "MAPS/one.lua").string()) << "error('not run by preview')";
    showPlayedMaps();
    auto* page = desktopAs<dskSelectMap>();
    const auto* table = page->GetCtrl<ctrlTable>(1);
    BOOST_TEST_REQUIRE(table->GetNumRows() == 2u);
    BOOST_TEST(table->GetItemText(0, 0) == "Alpha (*)");
    BOOST_TEST(table->GetItemText(0, 1) == "Catalog author");
    BOOST_TEST(table->GetItemText(0, 2) == "2 Player");
    BOOST_TEST(table->GetItemText(0, 4) == "32x32");
    BOOST_TEST(boost::filesystem::path(table->GetItemText(0, 5)) == userData / "MAPS" / "one.SwD");
    click(DrawPoint(200, 70));
    BOOST_TEST_REQUIRE(table->GetSelection());
    const auto firstPreview = page->GetCtrl<ctrlPreviewMinimap>(11)->GetCurMapSize();
    BOOST_TEST(firstPreview.x > 0u);
    BOOST_TEST(firstPreview.y > 0u);
    BOOST_TEST(page->GetCtrl<ctrlText>(12)->GetText() == "Alpha");
    BOOST_TEST(page->GetCtrl<ctrlButton>(5)->GetEnabled());
    BOOST_TEST_REQUIRE(focusedId(0) == 1u);
    press(pad, PadButton::RightShoulder);
    BOOST_TEST_REQUIRE(focusedId(0) != 1u);
    press(pad, PadButton::LeftShoulder);
    BOOST_TEST_REQUIRE(focusedId(0) == 1u);
    press(pad, PadButton::DpadDown);
    BOOST_TEST_REQUIRE(table->GetSelection());
    BOOST_TEST(*table->GetSelection() == 1u);
    BOOST_TEST(table->GetItemText(*table->GetSelection(), 0) == "Zulu");
    BOOST_TEST(page->GetCtrl<ctrlText>(12)->GetText() == "Zulu");
    BOOST_TEST(page->GetCtrl<ctrlPreviewMinimap>(11)->GetCurMapSize().y < firstPreview.y);
    press(pad, PadButton::B);
    BOOST_TEST(desktopAs<dskSinglePlayer>() != nullptr);
}

BOOST_FIXTURE_TEST_CASE(HeaderOnlyMapFailsAtPreviewWithoutHidingOtherMaps, MapReturnFixture)
{
    writeMap("broken.swd", "Alpha");
    writeMap("good.wld", "Zulu");
    // Keep the real header but omit the map layers, just as the lazy catalog allows.
    libsiedler2::ArchivItem_Map_Header header;
    header.setName("Alpha");
    header.setNumPlayers(2);
    header.setWidth(32);
    header.setHeight(32);
    {
        boost::nowide::ofstream stream((userData / "MAPS/broken.swd").string(), std::ios::binary);
        BOOST_TEST_REQUIRE(header.write(stream) == 0);
    }
    showPlayedMaps();
    auto* page = desktopAs<dskSelectMap>();
    const auto* table = page->GetCtrl<ctrlTable>(1);
    BOOST_TEST_REQUIRE(table->GetNumRows() == 2u);
    click(DrawPoint(200, 70));
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() != nullptr);
    BOOST_TEST(table->GetNumRows() == 1u);
    BOOST_TEST(table->GetItemText(0, 0) == "Zulu");
    // Removing the selected broken row selects/previews the remaining good row.
    BOOST_TEST_REQUIRE(table->GetSelection());
    BOOST_TEST(*table->GetSelection() == 0u);
    BOOST_TEST(page->GetCtrl<ctrlText>(12)->GetText() == "Zulu");
    BOOST_TEST(page->GetCtrl<ctrlButton>(5)->GetEnabled());
    press(pad, PadButton::A);
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    BOOST_TEST(desktopAs<dskSelectMap>() == page);
    BOOST_TEST(page->GetCtrl<ctrlPreviewMinimap>(11)->GetCurMapSize().x > 0u);
    BOOST_TEST(page->GetCtrl<ctrlButton>(5)->GetEnabled());
}

#ifndef _WIN32
BOOST_FIXTURE_TEST_CASE(UnreadableNeighborDoesNotHideTheClassicCategory, MapReturnFixture)
{
    writeMap("one.swd", "Alpha");
    boost::filesystem::create_symlink("one.lua", userData / "MAPS/one.lua");
    showPlayedMaps();
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    const auto* table = desktopAs<dskSelectMap>()->GetCtrl<ctrlTable>(1);
    BOOST_TEST_REQUIRE(table->GetNumRows() == 1u);
    BOOST_TEST(table->GetItemText(0, 0) == "Alpha");
}
#endif

BOOST_AUTO_TEST_SUITE_END()
