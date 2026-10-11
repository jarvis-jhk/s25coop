// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "frontend/CampaignCatalog.h"
#include "libsiedler2/ArchivItem_Map_Header.h"
#include "rttr/test/TmpFolder.hpp"
#include <boost/filesystem/operations.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>

namespace {
void campaign(const boost::filesystem::path& path, const std::string& name, const std::string& maps = "'mission.SWD'")
{
    boost::filesystem::create_directories(path);
    boost::nowide::ofstream script(path / "campaign.lua");
    script << "function getRequiredLuaVersion() return 1 end\ncampaign={uid='catalog',version=1,author='Author',name='"
           << name << "',shortDescription='Short',longDescription='Long',maxHumanPlayers=1,difficulty='easy',maps={"
           << maps << "},image='missing.bmp',chaptersEnabled={0}}\n";
    BOOST_TEST_REQUIRE(script.good());
    libsiedler2::ArchivItem_Map_Header map;
    map.setName("Mission");
    map.setWidth(32);
    map.setHeight(32);
    map.setNumPlayers(3);
    boost::nowide::ofstream stream(path / "mission.SWD", std::ios::binary);
    BOOST_TEST_REQUIRE(map.write(stream) == 0);
    boost::nowide::ofstream(path / "mission.lua") << "error('Mission scripts must never execute while browsing')\n";
}
} // namespace

BOOST_AUTO_TEST_SUITE(CampaignCatalogTests)

BOOST_AUTO_TEST_CASE(HeadersAndRequiredScriptsAreCheckedWithoutLayersOrMissionExecution)
{
    rttr::test::TmpFolder data;
    campaign(data / "Grüße", "Campaign");
    const auto size = boost::filesystem::file_size(data / "Grüße" / "mission.SWD");
    const auto time = boost::filesystem::last_write_time(data / "Grüße" / "mission.SWD");
    const auto catalog = frontend::ScanCampaigns({data});
    BOOST_TEST_REQUIRE(catalog.entries.size() == 1u);
    BOOST_TEST(catalog.failures.empty());
    const auto& entry = catalog.entries.front();
    BOOST_TEST(entry.folder == data / "Grüße");
    BOOST_TEST(entry.description.name == "Campaign");
    BOOST_TEST(entry.description.author == "Author");
    BOOST_TEST(entry.description.uid == "catalog");
    BOOST_TEST(entry.description.getNumMaps() == 1u);
    BOOST_TEST(entry.description.getLuaFilePath(0) == data / "Grüße" / "mission.lua");
    BOOST_TEST(boost::filesystem::file_size(entry.description.getMapFilePath(0)) == size);
    BOOST_TEST(boost::filesystem::last_write_time(entry.description.getMapFilePath(0)) == time);
    BOOST_TEST(entry.description.image.has_value());
}

BOOST_AUTO_TEST_CASE(DiscoverySortsNamesThenFoldersAndReadsLexicalDuplicatesOnlyOnce)
{
    rttr::test::TmpFolder data;
    campaign(data / "z", "Alpha");
    campaign(data / "a", "Alpha");
    campaign(data / "b", "Zulu");
    boost::nowide::ofstream(data / "ignored.txt") << "not a campaign";
    const auto catalog = frontend::ScanCampaigns({data, data / ".", data});
    BOOST_TEST_REQUIRE(catalog.entries.size() == 3u);
    BOOST_TEST(catalog.failures.empty());
    BOOST_TEST(catalog.entries[0].folder == data / "a");
    BOOST_TEST(catalog.entries[1].folder == data / "z");
    BOOST_TEST(catalog.entries[2].description.name == "Zulu");
}

BOOST_AUTO_TEST_CASE(BrokenDescriptionsEmptyCampaignsMissingScriptsAndBadHeadersAreIsolated)
{
    rttr::test::TmpFolder data;
    for(const auto* folder : {"good", "description", "empty", "script", "header", "scriptdir"})
        campaign(data / folder, folder);
    boost::nowide::ofstream(data / "description" / "campaign.lua") << "campaign={}";
    campaign(data / "empty", "empty", "");
    boost::filesystem::remove(data / "script" / "mission.lua");
    boost::filesystem::remove(data / "scriptdir" / "mission.lua");
    boost::filesystem::create_directory(data / "scriptdir" / "mission.lua");
    boost::nowide::ofstream(data / "header" / "mission.SWD") << "broken";
    const auto catalog = frontend::ScanCampaigns({data});
    BOOST_TEST_REQUIRE(catalog.entries.size() == 1u);
    BOOST_TEST(catalog.entries.front().description.name == "good");
    BOOST_TEST(catalog.failures.size() == 5u);
    for(const auto& failure : catalog.failures)
        BOOST_TEST(!failure.reason.empty());
    campaign(data / "header", "repaired");
    BOOST_TEST(frontend::ScanCampaigns({data}).entries.size() == 2u);
}

BOOST_AUTO_TEST_CASE(MissingSourcesAreEmptyButUnusableSourcesHaveAnError)
{
    rttr::test::TmpFolder data;
    boost::nowide::ofstream(data / "file") << "not a directory";
    campaign(data / "good", "Good");
    const auto catalog = frontend::ScanCampaigns({data / "missing", data / "file", data});
    BOOST_TEST_REQUIRE(catalog.entries.size() == 1u);
    BOOST_TEST_REQUIRE(catalog.failures.size() == 1u);
    BOOST_TEST(catalog.failures.front().path == data / "file");
}

BOOST_AUTO_TEST_CASE(AllMissionHeadersAreRequiredAndVersionErrorsStayIsolated)
{
    rttr::test::TmpFolder data;
    campaign(data / "many", "Many", "'mission.SWD','missing.SWD'");
    campaign(data / "version", "Version");
    boost::nowide::ofstream(data / "version" / "campaign.lua") << "function getRequiredLuaVersion() return 999 end";
    BOOST_TEST(frontend::ScanCampaigns({data}).entries.empty());
    BOOST_TEST(frontend::ScanCampaigns({data}).failures.size() == 2u);
}

#ifndef _WIN32
BOOST_AUTO_TEST_CASE(UninspectableCampaignDoesNotHideTheOtherFolders)
{
    rttr::test::TmpFolder data;
    campaign(data / "good", "Good");
    boost::filesystem::create_symlink("loop", data / "loop");
    const auto catalog = frontend::ScanCampaigns({data});
    BOOST_TEST_REQUIRE(catalog.entries.size() == 1u);
    BOOST_TEST_REQUIRE(catalog.failures.size() == 1u);
    BOOST_TEST(catalog.failures.front().path == data / "loop");
}
#endif

BOOST_AUTO_TEST_SUITE_END()
