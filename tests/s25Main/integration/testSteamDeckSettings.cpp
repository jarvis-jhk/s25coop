// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "RttrConfig.h"
#include "Settings.h"
#include "files.h"
#include "languages.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include "s25util/System.h"
#include <boost/filesystem/operations.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <iterator>
#include <stdexcept>
#include <tuple>

namespace {
auto settingsFields()
{
    return std::tie(SETTINGS.global, SETTINGS.video, SETTINGS.language, SETTINGS.driver, SETTINGS.sound, SETTINGS.lobby,
                    SETTINGS.server, SETTINGS.proxy, SETTINGS.interface, SETTINGS.ingame, SETTINGS.windows,
                    SETTINGS.addons, SETTINGS.campaigns);
}

template<class F>
void withSettings(F body)
{
    // Copy the public values, never the singleton object or its lifetime registration.
    const auto saved = std::apply([](const auto&... fields) { return std::make_tuple(fields...); }, settingsFields());
    const bool hadFlag = System::envVarExists("SteamDeck");
    const auto oldFlag = System::getEnvVar("SteamDeck");
    rttr::test::TmpFolder data;
    rttr::test::ConfigOverride userData("USERDATA", data);
    const auto config = RTTRCONFIG.ExpandPath(s25::resources::config);
    boost::filesystem::create_directories(config.parent_path());
    const auto restore = [&] {
        settingsFields() = saved;
        LANGUAGES.setLanguage(SETTINGS.language.language);
        if(hadFlag)
            BOOST_TEST_REQUIRE(System::setEnvVar("SteamDeck", oldFlag));
        else
            BOOST_TEST_REQUIRE(System::removeEnvVar("SteamDeck"));
        BOOST_TEST(SETTINGS.video.steamDeckUi == std::get<1>(saved).steamDeckUi);
        BOOST_TEST(SETTINGS.video.guiScale == std::get<1>(saved).guiScale);
        BOOST_TEST(System::envVarExists("SteamDeck") == hadFlag);
        BOOST_TEST(System::getEnvVar("SteamDeck") == oldFlag);
    };
    try
    {
        body(config);
    } catch(...)
    {
        restore();
        throw;
    }
    restore();
}

void stripDeckProfile(const boost::filesystem::path& config)
{
    boost::nowide::ifstream in(config);
    std::string text, line;
    while(std::getline(in, line))
        if(line.find("steam_deck_ui") == std::string::npos)
            text += line + '\n';
    in.close();
    boost::nowide::ofstream(config) << text;
}

[[noreturn]] void throwProbe()
{
    throw std::runtime_error("settings cleanup probe");
}
} // namespace

BOOST_AUTO_TEST_SUITE(SteamDeckSettings)

BOOST_AUTO_TEST_CASE(FirstRunProfileIsSavedAndRetainedAcrossLaunches)
{
    withSettings([](const auto& config) {
        BOOST_TEST_REQUIRE(System::setEnvVar("SteamDeck", "1"));
        SETTINGS.Load();
        BOOST_TEST_REQUIRE(SETTINGS.video.steamDeckUi);
        BOOST_TEST(SETTINGS.video.guiScale == 0u);
        BOOST_TEST(!SETTINGS.video.tvMode);
        boost::nowide::ifstream in(config);
        const std::string persisted((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        BOOST_TEST(persisted.find("steam_deck_ui=1") != std::string::npos);
        BOOST_TEST_REQUIRE(System::setEnvVar("SteamDeck", "0"));
        SETTINGS.video.steamDeckUi = false;
        SETTINGS.Load();
        BOOST_TEST(SETTINGS.video.steamDeckUi);
        BOOST_TEST(SETTINGS.video.guiScale == 0u);
    });
}

BOOST_AUTO_TEST_CASE(NestedCleanupRestoresAnExistingLaunchFlag)
{
    withSettings([](const auto&) {
        BOOST_TEST_REQUIRE(System::setEnvVar("SteamDeck", "0"));
        withSettings([](const auto&) {
            BOOST_TEST_REQUIRE(System::setEnvVar("SteamDeck", "1"));
            SETTINGS.Load();
            BOOST_TEST(SETTINGS.video.steamDeckUi);
        });
        BOOST_TEST(System::getEnvVar("SteamDeck") == "0");
    });
}

BOOST_AUTO_TEST_CASE(NonDeckFirstRunKeepsDesktopDefaults)
{
    withSettings([](const auto&) {
        BOOST_TEST_REQUIRE(System::setEnvVar("SteamDeck", "0"));
        SETTINGS.Load();
        BOOST_TEST(!SETTINGS.video.steamDeckUi);
        BOOST_TEST(SETTINGS.video.guiScale == 0u);
        BOOST_TEST(!SETTINGS.video.tvMode);
    });
}

BOOST_AUTO_TEST_CASE(OldAndExplicitSettingsWinOverDetection)
{
    withSettings([](const auto& config) {
        BOOST_TEST_REQUIRE(System::setEnvVar("SteamDeck", "0"));
        SETTINGS.Load();
        for(const unsigned scale : {0u, 100u, 125u})
        {
            SETTINGS.video.guiScale = scale;
            SETTINGS.video.tvMode = true;
            SETTINGS.video.steamDeckUi = false;
            SETTINGS.global.coopChangelogSeen = "preserved";
            SETTINGS.Save();
            stripDeckProfile(config);
            BOOST_TEST_REQUIRE(System::setEnvVar("SteamDeck", "1"));
            SETTINGS.video.steamDeckUi = true;
            SETTINGS.video.guiScale = 999u;
            SETTINGS.Load();
            BOOST_TEST(!SETTINGS.video.steamDeckUi);
            BOOST_TEST(SETTINGS.video.guiScale == scale);
            BOOST_TEST(SETTINGS.video.tvMode);
            BOOST_TEST(SETTINGS.global.coopChangelogSeen == "preserved");
        }
        // An explicit false in the new format also wins.
        SETTINGS.Save();
        SETTINGS.video.steamDeckUi = true;
        SETTINGS.Load();
        BOOST_TEST(!SETTINGS.video.steamDeckUi);
        // A chosen scale is retained even when the new Deck profile is enabled.
        SETTINGS.video.steamDeckUi = true;
        SETTINGS.video.guiScale = 110;
        SETTINGS.Save();
        SETTINGS.video.steamDeckUi = false;
        SETTINGS.video.guiScale = 0;
        SETTINGS.Load();
        BOOST_TEST(SETTINGS.video.steamDeckUi);
        BOOST_TEST(SETTINGS.video.guiScale == 110u);
    });
}

BOOST_AUTO_TEST_CASE(ExceptionalCleanupRestoresSingletonValuesAndEnvironment)
{
    bool reached = false;
    BOOST_CHECK_THROW(withSettings([&](const auto&) {
                          BOOST_TEST_REQUIRE(System::setEnvVar("SteamDeck", "1"));
                          SETTINGS.Load();
                          BOOST_TEST_REQUIRE(SETTINGS.video.steamDeckUi);
                          reached = true;
                          throwProbe();
                      }),
                      std::runtime_error);
    BOOST_TEST(reached);
}

BOOST_AUTO_TEST_SUITE_END()
