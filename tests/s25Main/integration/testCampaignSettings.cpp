// Copyright (C) 2024 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "CampaignSettings.h"
#include "RttrConfig.h"
#include "Settings.h"
#include "files.h"
#include "lua/CampaignDataLoader.h"
#include "gameData/CampaignDescription.h"
#include "rttr/test/TmpFolder.hpp"
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <regex>

namespace {
struct CampaignSettingsFixture
{
    CampaignDescription desc;
    CampaignSettings sut;
};
} // namespace

BOOST_FIXTURE_TEST_CASE(CampaignSettingsIntegrationTest, CampaignSettingsFixture)
{
    constexpr auto cmpgn1 = "roman";
    constexpr auto cmpgn2 = "world";
    rttr::test::TmpFolder tmp;
    {
        boost::nowide::ofstream file(tmp / "campaign.lua");
        // read first campaign - chaptersEnabled not specified
        file << R"(campaign = {
                version = "1",
                uid = "roman",
                author = "Max Meier",
                name = "My campaign",
                shortDescription = "Very short description",
                longDescription = "This is the long description",
                image = "<RTTR_GAME>/GFX/PICS/WORLD.LBM",
                maxHumanPlayers = 1,
                difficulty = "easy",
                mapFolder = "<RTTR_GAME>/DATA/MAPS",
                luaFolder = "<RTTR_GAME>/CAMPAIGNS/ROMAN",
                maps = { "dessert0.WLD", "dessert1.WLD", "dessert2.WLD"}
            }
            )";
        file << "function getRequiredLuaVersion() return 1 end";
    }
    BOOST_TEST_REQUIRE((CampaignDataLoader{desc, tmp}.Load()));

    // chapters 1 and 2 are playable by default
    BOOST_TEST_REQUIRE(sut.isChapterPlayable(desc, 0) == true);
    BOOST_TEST_REQUIRE(sut.isChapterPlayable(desc, 1) == true);
    BOOST_TEST_REQUIRE(sut.isChapterPlayable(desc, 2) == false);
    BOOST_TEST_REQUIRE(sut.isChapterPlayable(desc, 9) == false);

    // complete ch2 and enable ch3
    sut.setChapterCompleted(cmpgn1, 1);
    sut.enableChapter(cmpgn1, 2);
    auto ms = sut.getMissionsStatus(desc);
    BOOST_TEST_REQUIRE(ms[0].playable == true);
    BOOST_TEST_REQUIRE(ms[0].conquered == false);
    BOOST_TEST_REQUIRE(ms[1].playable == true);
    BOOST_TEST_REQUIRE(ms[1].conquered == true);
    BOOST_TEST_REQUIRE(ms[2].playable == true);
    BOOST_TEST_REQUIRE(ms[2].conquered == false);

    // read save data - ch1 completed, ch2 enabled, ch3 disabled
    sut.readSaveData(cmpgn1, "210");
    ms = sut.getMissionsStatus(desc);
    BOOST_TEST_REQUIRE(ms[0].playable == true);
    BOOST_TEST_REQUIRE(ms[0].conquered == true);
    BOOST_TEST_REQUIRE(ms[1].playable == true);
    BOOST_TEST_REQUIRE(ms[1].conquered == false);
    BOOST_TEST_REQUIRE(ms[2].playable == false);
    BOOST_TEST_REQUIRE(ms[2].conquered == false);

    // enable ch3
    sut.enableChapter(cmpgn1, 2);
    // save code should now be 211
    decltype(sut.createSaveData()) expectedSaveData;
    expectedSaveData[cmpgn1] = "211";
    BOOST_TEST_REQUIRE(sut.createSaveData() == expectedSaveData);

    // read and save data for campaign 2
    sut.readSaveData(cmpgn2, "21010");
    expectedSaveData[cmpgn2] = "21010";
    BOOST_TEST_REQUIRE(sut.createSaveData() == expectedSaveData);

    // read second campaign - chaptersEnabled specified
    {
        boost::nowide::ofstream file(tmp / "campaign.lua");
        file.clear();
        file << R"(campaign = {
                version = "1",
                uid = "world",
                author = "Max Meier",
                name = "My campaign",
                shortDescription = "Very short description",
                longDescription = "This is the long description",
                image = "<RTTR_GAME>/GFX/PICS/WORLD.LBM",
                maxHumanPlayers = 1,
                difficulty = "easy",
                mapFolder = "<RTTR_GAME>/DATA/MAPS",
                luaFolder = "<RTTR_GAME>/CAMPAIGNS/ROMAN",
                maps = { "dessert0.WLD", "dessert1.WLD", "dessert2.WLD"},
                chaptersEnabled = {1, 4}
            }
            )";
        file << "function getRequiredLuaVersion() return 1 end";
    }
    BOOST_TEST_REQUIRE((CampaignDataLoader{desc, tmp}.Load()));

    BOOST_TEST_REQUIRE(sut.isChapterPlayable(desc, 0) == true);
    BOOST_TEST_REQUIRE(sut.isChapterPlayable(desc, 1) == true);
    BOOST_TEST_REQUIRE(sut.isChapterPlayable(desc, 2) == false);
    BOOST_TEST_REQUIRE(sut.isChapterPlayable(desc, 3) == true);
    // saved as disabled, but a default chapter is always playable
    BOOST_TEST_REQUIRE(sut.isChapterPlayable(desc, 4) == true);

    sut.enableChapter(cmpgn2, 2);
    BOOST_TEST_REQUIRE(sut.isChapterPlayable(desc, 2) == true);
}

BOOST_FIXTURE_TEST_CASE(DefaultChaptersStayPlayableAfterScriptChanges, CampaignSettingsFixture)
{
    desc.uid = "roman";
    desc.chaptersEnabled = {0, 1};
    // A mission script finished chapter 2 before the campaign screen was ever opened on this profile
    sut.setChapterCompleted("roman", 1);
    sut.enableChapter("roman", 2);
    BOOST_TEST(sut.isChapterPlayable(desc, 0));
    BOOST_TEST(sut.isChapterPlayable(desc, 1));
    BOOST_TEST(sut.isChapterPlayable(desc, 2));
    BOOST_TEST(!sut.isChapterPlayable(desc, 3));
}

BOOST_FIXTURE_TEST_CASE(CompletionStatusIsResetForTheNextGame, CampaignSettingsFixture)
{
    BOOST_TEST(!sut.shouldShowVictoryScreen());
    sut.setChapterCompleted("roman", 3);
    BOOST_TEST(sut.shouldShowVictoryScreen());
    BOOST_TEST(*sut.getCompletedChapter() == 3u);
    sut.resetCompletionStatus();
    BOOST_TEST(!sut.shouldShowVictoryScreen());
}

BOOST_AUTO_TEST_CASE(CampaignProgressIsSavedAndOldConfigsKeepTheirSettings)
{
    const auto configPath = RTTRCONFIG.ExpandPath(s25::resources::config);
    SETTINGS.campaigns = CampaignSettings{};
    SETTINGS.global.coopChangelogSeen = "9.9.9";
    SETTINGS.campaigns.readSaveData("roman", "2210");
    SETTINGS.Save();

    SETTINGS.campaigns = CampaignSettings{};
    SETTINGS.global.coopChangelogSeen.clear();
    SETTINGS.Load();
    BOOST_TEST(SETTINGS.global.coopChangelogSeen == "9.9.9");
    BOOST_TEST(SETTINGS.campaigns.createSaveData()["roman"] == "2210");

    // A config written before campaign progress existed has no [campaigns] section. It must still load, not be
    // replaced by the defaults.
    std::string content;
    {
        boost::nowide::ifstream in(configPath);
        std::string line;
        bool inCampaigns = false;
        while(std::getline(in, line))
        {
            if(!line.empty() && line[0] == '[')
                inCampaigns = line.rfind("[campaigns]", 0) == 0;
            if(!inCampaigns)
                content += line + "\n";
        }
    }
    BOOST_TEST_REQUIRE(content.find("campaigns") == std::string::npos);
    {
        boost::nowide::ofstream out(configPath);
        out << content;
    }
    SETTINGS.campaigns = CampaignSettings{};
    SETTINGS.global.coopChangelogSeen.clear();
    SETTINGS.Load();
    BOOST_TEST(SETTINGS.global.coopChangelogSeen == "9.9.9");
    BOOST_TEST(SETTINGS.campaigns.createSaveData().empty());
    SETTINGS.campaigns = CampaignSettings{};
}

// Every mission script of the shipped campaigns must complete its own chapter (its index in the campaign's map list),
// otherwise winning one mission marks another one as conquered.
BOOST_AUTO_TEST_CASE(ShippedMissionsCompleteTheirOwnChapter)
{
    for(const char* folder : {"roman", "world"})
    {
        CampaignDescription desc;
        BOOST_TEST_REQUIRE((CampaignDataLoader{desc, RTTRCONFIG.ExpandPath("<RTTR_RTTR>/campaigns") / folder}.Load()));
        BOOST_TEST_REQUIRE(desc.uid == folder);
        const std::regex completed("SetCampaignChapterCompleted\\(\"(\\w+)\", (\\d+)\\)");
        for(unsigned i = 0; i < desc.getNumMaps(); ++i)
        {
            boost::nowide::ifstream in(desc.getLuaFilePath(i));
            BOOST_TEST_REQUIRE(in.good(), desc.getLuaFilePath(i));
            const std::string script{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
            std::smatch match;
            BOOST_TEST_INFO(desc.getLuaFilePath(i));
            BOOST_TEST_REQUIRE(std::regex_search(script, match, completed));
            BOOST_TEST(match[1].str() == std::string(folder));
            BOOST_TEST(std::stoul(match[2].str()) == i);
        }
    }
}

BOOST_FIXTURE_TEST_CASE(SaveDataHasNoGaps, CampaignSettingsFixture)
{
    // Chapters never touched between two known ones must be saved as disabled, not as garbage
    sut.enableChapter("world", 4);
    sut.setChapterCompleted("world", 1);
    BOOST_TEST(sut.createSaveData()["world"] == "02001");
}
