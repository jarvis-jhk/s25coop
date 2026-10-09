// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "frontend/MapCatalog.h"
#include "gameData/MapConsts.h"
#include "gameData/MaxPlayers.h"
#include "libsiedler2/ArchivItem_Map.h"
#include "libsiedler2/ArchivItem_Map_Header.h"
#include "libsiedler2/prototypen.h"
#include "rttr/test/TmpFolder.hpp"
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <memory>
#include <stdexcept>

namespace {
void writeHeader(const boost::filesystem::path& path, const std::string& name = "Map", const uint8_t players = 2,
                 const uint16_t width = 64, const uint16_t height = 48)
{
    libsiedler2::ArchivItem_Map_Header header;
    header.setName(name);
    header.setAuthor("Author");
    header.setNumPlayers(players);
    header.setWidth(width);
    header.setHeight(height);
    header.setGfxSet(1);
    boost::nowide::ofstream stream(path.string(), std::ios::binary);
    BOOST_TEST_REQUIRE(header.write(stream) == 0);
}
} // namespace

BOOST_AUTO_TEST_SUITE(MapCatalogTests)

BOOST_AUTO_TEST_CASE(HeaderMetadataDoesNotRequireMapLayersOrExecuteScripts)
{
    rttr::test::TmpFolder folder;
    const auto path = folder / "Grüße.SWD";
    writeHeader(path, "Gr\xFC\xDF"
                      "e");
    boost::nowide::ofstream((folder / "Grüße.lua").string()) << "error('must not execute during browsing')";
    const auto entry = frontend::ReadMapEntry(path);
    BOOST_TEST(entry.path == path);
    BOOST_TEST(entry.name == "Grüße");
    BOOST_TEST(entry.author == "Author");
    BOOST_TEST(entry.players == 2u);
    BOOST_TEST(entry.width == 64u);
    BOOST_TEST(entry.height == 48u);
    BOOST_TEST(entry.landscape == 1u);
    BOOST_TEST(entry.hasScript);
    libsiedler2::Archiv archive;
    BOOST_TEST(libsiedler2::loader::LoadMAP(path, archive) != 0);
    boost::filesystem::remove(folder / "Grüße.lua");
    boost::filesystem::create_directory(folder / "Grüße.lua");
    BOOST_TEST(!frontend::ReadMapEntry(path).hasScript);
}

#ifndef _WIN32
BOOST_AUTO_TEST_CASE(UnreadableScriptAnnotationDoesNotExcludeAReadableMap)
{
    rttr::test::TmpFolder folder;
    writeHeader(folder / "map.swd");
    boost::filesystem::create_symlink("map.lua", folder / "map.lua");
    const auto catalog = frontend::ScanMaps({folder});
    BOOST_TEST_REQUIRE(catalog.entries.size() == 1u);
    BOOST_TEST(catalog.failures.empty());
    BOOST_TEST(!catalog.entries.front().hasScript);
}
#endif

BOOST_AUTO_TEST_CASE(FullMapUsesTheSameHeaderAndSurvivesBrowsingUnchanged)
{
    rttr::test::TmpFolder folder;
    const auto path = folder / "full.wld";
    auto header = std::make_unique<libsiedler2::ArchivItem_Map_Header>();
    header->setName("Full map");
    header->setAuthor("Map author");
    header->setNumPlayers(3);
    header->setWidth(32);
    header->setHeight(32);
    libsiedler2::ArchivItem_Map map;
    map.init(std::move(header));
    {
        boost::nowide::ofstream stream(path.string(), std::ios::binary);
        BOOST_TEST_REQUIRE(map.write(stream) == 0);
    }
    const auto size = boost::filesystem::file_size(path);
    const auto time = boost::filesystem::last_write_time(path);
    const auto catalog = frontend::ScanMaps({folder});
    BOOST_TEST_REQUIRE(catalog.entries.size() == 1u);
    BOOST_TEST(catalog.failures.empty());
    BOOST_TEST(catalog.entries.front().name == "Full map");
    BOOST_TEST(catalog.entries.front().author == "Map author");
    BOOST_TEST(catalog.entries.front().players == 3u);
    BOOST_TEST(!catalog.entries.front().hasScript);
    BOOST_TEST(boost::filesystem::file_size(path) == size);
    BOOST_TEST(boost::filesystem::last_write_time(path) == time);
    libsiedler2::Archiv archive;
    BOOST_TEST_REQUIRE(libsiedler2::loader::LoadMAP(path, archive) == 0);
    const auto* loaded = dynamic_cast<const libsiedler2::ArchivItem_Map*>(archive[0]);
    BOOST_TEST_REQUIRE(loaded);
    BOOST_TEST(loaded->getLayer(libsiedler2::MapLayer::Terrain1) == map.getLayer(libsiedler2::MapLayer::Terrain1),
               boost::test_tools::per_element());
}

BOOST_AUTO_TEST_CASE(DiscoverySortsNamesAndPathsDeduplicatesAndFiltersExactTribes)
{
    rttr::test::TmpFolder folder;
    const auto extra = folder / "extra";
    boost::filesystem::create_directory(extra);
    writeHeader(folder / "z.SwD", "Alpha", 4);
    writeHeader(folder / "a.WLD", "Alpha", 2);
    writeHeader(folder / "first.swd", "Zulu", 1);
    writeHeader(extra / "nested.swd", "Nested", 3);
    writeHeader(folder / "ignored.sav", "Ignored", 2);
    boost::filesystem::create_directory(folder / "directory.swd");
    const auto all = frontend::ScanMaps({folder, folder / ".", folder});
    BOOST_TEST_REQUIRE(all.entries.size() == 3u);
    BOOST_TEST(all.failures.empty());
    BOOST_TEST(all.entries[0].path == folder / "a.WLD");
    BOOST_TEST(all.entries[1].path == folder / "z.SwD");
    BOOST_TEST(all.entries[2].name == "Zulu");
    for(unsigned players = 0; players <= MAX_PLAYERS + 1u; ++players)
    {
        const auto filtered = frontend::ScanMaps({folder}, players);
        const bool expected = players == 1u || players == 2u || players == 4u;
        BOOST_TEST(filtered.entries.size() == (expected ? 1u : 0u));
        BOOST_TEST(filtered.failures.empty());
        for(const auto& entry : filtered.entries)
            BOOST_TEST(entry.players == players);
    }
    const auto withExtra = frontend::ScanMaps({extra, folder});
    BOOST_TEST_REQUIRE(withExtra.entries.size() == 4u);
    BOOST_TEST(withExtra.entries[2].name == "Nested");
}

BOOST_AUTO_TEST_CASE(MalformedHeadersAndUnsupportedLimitsDoNotHideGoodMaps)
{
    rttr::test::TmpFolder folder;
    writeHeader(folder / "good.swd", "Good", 2, MAX_MAP_SIZE, MAX_MAP_SIZE);
    writeHeader(folder / "wide.swd", "Wide", 2, MAX_MAP_SIZE + 1u, 32);
    writeHeader(folder / "tall.wld", "Tall", 2, 32, MAX_MAP_SIZE + 1u);
    writeHeader(folder / "zero-width.swd", "Empty", 2, 0, 32);
    writeHeader(folder / "zero-height.wld", "Empty", 2, 32, 0);
    writeHeader(folder / "zero-players.swd", "Nobody", 0);
    writeHeader(folder / "players.wld", "Too many", MAX_PLAYERS + 1u);
    writeHeader(folder / "truncated.swd");
    boost::filesystem::resize_file(folder / "truncated.swd", 20);
    boost::nowide::ofstream((folder / "invalid.wld").string()) << "not a map";
    {
        boost::nowide::ofstream empty((folder / "empty.swd").string());
        BOOST_TEST_REQUIRE(empty.good());
    }
    const auto catalog = frontend::ScanMaps({folder, folder / "."});
    BOOST_TEST_REQUIRE(catalog.entries.size() == 1u);
    BOOST_TEST(catalog.entries.front().name == "Good");
    BOOST_TEST(catalog.failures.size() == 9u);
    for(const auto& failure : catalog.failures)
    {
        BOOST_TEST(!failure.reason.empty());
        BOOST_CHECK_THROW(frontend::ReadMapEntry(failure.path), std::runtime_error);
    }
    const auto filtered = frontend::ScanMaps({folder}, 4u);
    BOOST_TEST(filtered.entries.empty());
    BOOST_TEST(filtered.failures.size() == 9u);
    BOOST_CHECK_THROW(frontend::ReadMapEntry(folder / "missing.swd"), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(MissingAndInvalidFoldersAreIsolatedFromOtherSources)
{
    rttr::test::TmpFolder folder;
    const auto missing = frontend::ScanMaps({folder / "missing"});
    BOOST_TEST(missing.entries.empty());
    BOOST_TEST(missing.failures.empty());
    writeHeader(folder / "one.swd");
    const auto catalog = frontend::ScanMaps({folder / "one.swd", folder});
    BOOST_TEST_REQUIRE(catalog.entries.size() == 1u);
    BOOST_TEST_REQUIRE(catalog.failures.size() == 1u);
    BOOST_TEST(catalog.failures.front().path == folder / "one.swd");
    BOOST_TEST(!catalog.failures.front().reason.empty());
    const auto empty = frontend::ScanMaps({});
    BOOST_TEST(empty.entries.empty());
    BOOST_TEST(empty.failures.empty());
}

BOOST_AUTO_TEST_SUITE_END()
