// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "frontend/ScenarioCatalog.h"
#include "libsiedler2/ArchivItem_Map_Header.h"
#include "rttr/test/TmpFolder.hpp"
#include <boost/filesystem/operations.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>

namespace {
void scenario(const boost::filesystem::path& folder, const std::string& name, unsigned players = 3,
              const std::string& maps = "'mission.SWD'", const std::string& uid = "scenario-catalog")
{
    boost::filesystem::create_directories(folder);
    boost::nowide::ofstream(folder / "campaign.lua")
      << "function getRequiredLuaVersion() return 1 end\ncampaign={uid='" << uid << "',version=1,author='Author',name='"
      << name << "',shortDescription='Short',longDescription='Long',maxHumanPlayers=1,difficulty='easy',maps={" << maps
      << "},chaptersEnabled={0}}\n";
    libsiedler2::ArchivItem_Map_Header map;
    map.setName("Mission");
    map.setWidth(32);
    map.setHeight(32);
    map.setNumPlayers(players);
    boost::nowide::ofstream stream(folder / "mission.SWD", std::ios::binary);
    BOOST_TEST_REQUIRE(map.write(stream) == 0);
    boost::nowide::ofstream(folder / "mission.lua") << "error('do not execute the mission during discovery')";
}
} // namespace

BOOST_AUTO_TEST_SUITE(ScenarioCatalogTests)

BOOST_AUTO_TEST_CASE(RegistrationRequiresExactlyOneMissionAndNeverInfersIdentityFromAnAdjacentScript)
{
    rttr::test::TmpFolder data;
    scenario(data / "one", "Single");
    scenario(data / "many", "Many", 3, "'mission.SWD','mission.SWD'");
    scenario(data / "plain", "Unregistered");
    boost::filesystem::remove(data / "plain" / "campaign.lua");
    const auto catalog = frontend::ScanScenarios({data, data / "."});
    BOOST_TEST_REQUIRE(catalog.entries.size() == 1u);
    BOOST_TEST(catalog.entries.front().description.uid == "scenario-catalog");
    BOOST_TEST(catalog.entries.front().description.getNumMaps() == 1u);
    BOOST_TEST(catalog.entries.front().folder == data / "one");
    BOOST_TEST(catalog.failures.size() == 1u);
    BOOST_CHECK_THROW(frontend::ReadScenario(data / "many"), std::runtime_error);
    BOOST_TEST(frontend::ReadScenario(data / "one").description.name == "Single");
}

BOOST_AUTO_TEST_CASE(SortingExactTribeFilterAndOptionalUidKeepTheExistingCampaignContract)
{
    rttr::test::TmpFolder data;
    scenario(data / "z", "Alpha", 3);
    scenario(data / "a", "Alpha", 2, "'mission.SWD'", "");
    scenario(data / "b", "Beta", 2);
    const auto all = frontend::ScanScenarios({data, data});
    BOOST_TEST_REQUIRE(all.entries.size() == 3u);
    BOOST_TEST(all.entries[0].folder == data / "a");
    BOOST_TEST(all.entries[1].folder == data / "z");
    BOOST_TEST(all.entries[2].description.name == "Beta");
    BOOST_TEST(all.entries[0].description.uid.empty());
    const auto filtered = frontend::ScanScenarios({data}, 2);
    BOOST_TEST_REQUIRE(filtered.entries.size() == 2u);
    BOOST_TEST(filtered.failures.empty());
    BOOST_TEST(frontend::ScanScenarios({data}, 4).entries.empty());
    BOOST_TEST(frontend::ScanScenarios({data / "missing"}).entries.empty());
}

BOOST_AUTO_TEST_CASE(BrokenRegistrationsAndMissingRequiredScriptsAreIsolatedAndCanBeRepaired)
{
    rttr::test::TmpFolder data;
    scenario(data / "good", "Good");
    scenario(data / "bad", "Bad");
    scenario(data / "missing", "Missing");
    boost::nowide::ofstream(data / "bad" / "campaign.lua") << "campaign={}";
    boost::filesystem::remove(data / "missing" / "mission.lua");
    const auto catalog = frontend::ScanScenarios({data});
    BOOST_TEST_REQUIRE(catalog.entries.size() == 1u);
    BOOST_TEST(catalog.entries.front().description.name == "Good");
    BOOST_TEST(catalog.failures.size() == 2u);
    scenario(data / "missing", "Repaired");
    BOOST_TEST(frontend::ScanScenarios({data}).entries.size() == 2u);
}

BOOST_AUTO_TEST_CASE(ValidatedMapMetadataRemainsOwnedAfterTheSourceDisappears)
{
    rttr::test::TmpFolder data;
    scenario(data / "good", "Good");
    scenario(data / "disappearing", "Disappearing");
    const auto entry = frontend::ReadScenario(data / "disappearing");
    BOOST_TEST_REQUIRE(entry.maps.size() == 1u);
    boost::filesystem::remove(entry.description.getMapFilePath(0));
    BOOST_TEST(entry.maps.front().name == "Mission");
    BOOST_TEST(entry.maps.front().players == 3u);
    BOOST_TEST(entry.maps.front().width == 32u);
    BOOST_TEST(entry.maps.front().path == data / "disappearing" / "mission.SWD");
    const auto catalog = frontend::ScanScenarios({data}, 3);
    BOOST_TEST_REQUIRE(catalog.entries.size() == 1u);
    BOOST_TEST(catalog.entries.front().description.name == "Good");
    BOOST_TEST(catalog.failures.size() == 1u);
}

BOOST_AUTO_TEST_SUITE_END()
