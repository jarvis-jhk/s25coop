// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#define BOOST_TEST_MODULE RTTR_Config

#include "RTTR_Version.h"
#include "RttrConfig.h"
#include "s25util/System.h"
#include <rttr/test/BaseFixture.hpp>
#include <rttr/test/LogAccessor.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/nowide/args.hpp>
#include <boost/test/unit_test.hpp>

#if RTTR_HAS_VLD
#    include <vld.h>
#endif

namespace fs = boost::filesystem;

BOOST_AUTO_TEST_CASE(CoopWindowTitle)
{
    BOOST_TEST(rttr::version::GetTitle() == "The Settlers II: Coop");
}

class ResetWorkDir
{
    fs::path oldWorkDir;

public:
    ResetWorkDir() : oldWorkDir(fs::current_path()) {}
    ~ResetWorkDir() { fs::current_path(oldWorkDir); }
};

BOOST_FIXTURE_TEST_CASE(PrefixPath, rttr::test::BaseFixture)
{
    rttr::test::LogAccessor logAcc;
    fs::path prefixPath = RTTRCONFIG.GetPrefixPath();
    BOOST_TEST_REQUIRE(!prefixPath.empty());
    BOOST_TEST(fs::exists(prefixPath));
    BOOST_TEST(fs::is_directory(prefixPath));
    // No entry of the path should be the NULL terminator
    for(const char c : prefixPath.string())
    {
        BOOST_TEST(c != '\0');
    }
    // If the env var is not set (usually should not be) then set it to check if that is used as the prefix path
    if(!System::envVarExists("RTTR_PREFIX_DIR"))
    {
        fs::path fakePrefixPath = fs::current_path() / "testPrefixPath";
        BOOST_TEST_REQUIRE(System::setEnvVar("RTTR_PREFIX_DIR", fakePrefixPath.string()));
        BOOST_TEST(RTTRCONFIG.GetPrefixPath() == fakePrefixPath);
        RTTR_REQUIRE_LOG_CONTAINS("manually set", false);
        BOOST_TEST_REQUIRE(System::removeEnvVar("RTTR_PREFIX_DIR"));
    }
    {
        ResetWorkDir resetWorkDir;
        // Just change it in case we would not change it back
        fs::current_path(fs::current_path().parent_path());
        BOOST_TEST_REQUIRE(RTTRCONFIG.Init());
        BOOST_TEST(prefixPath == fs::current_path());
    }
}

BOOST_FIXTURE_TEST_CASE(EnvOverride, rttr::test::BaseFixture)
{
    rttr::test::LogAccessor logAcc;
    const fs::path pathOverride = fs::current_path() / "testPathOverride";
    BOOST_TEST_REQUIRE(System::setEnvVar("RTTR_GAME_DIR", pathOverride.string()));
    // fakePath is just used as default path. if != pathOverride -> test failed
    BOOST_TEST(RTTRCONFIG.getEnvOverride("GAME", "fakePath") == pathOverride);
    RTTR_REQUIRE_LOG_CONTAINS("manually set", false);
    BOOST_TEST_REQUIRE(System::removeEnvVar("RTTR_GAME_DIR"));
}

BOOST_FIXTURE_TEST_CASE(WarpzoneGameDir, rttr::test::BaseFixture)
{
    rttr::test::LogAccessor logAcc;
    const fs::path base = fs::current_path() / "testWarpzoneGameDir";
    const fs::path found = base / "found", empty = base / "empty", installed = base / "installed";
    fs::remove_all(base);
    for(const fs::path& dir : {found, installed})
    {
        fs::create_directories(dir / "DATA");
        fs::create_directories(dir / "GFX");
    }
    fs::create_directories(empty / "DATA"); // DATA alone is not a usable copy
    BOOST_TEST_REQUIRE(System::removeEnvVar("RTTR_GAME_DIR"));

    // Not launched by Warpzone: the default as before
    BOOST_TEST_REQUIRE(System::removeEnvVar("WARPZONE_DATA_S2_GOLD"));
    BOOST_TEST(RttrConfig::getGameDir(empty) == empty);

    BOOST_TEST_REQUIRE(System::setEnvVar("WARPZONE_DATA_S2_GOLD", found.string()));
    BOOST_TEST(RttrConfig::getGameDir(empty) == found);
    RTTR_REQUIRE_LOG_CONTAINS("Warpzone launcher found", false);
    // Files already in place win, and so does an explicit RTTR_GAME_DIR
    BOOST_TEST(RttrConfig::getGameDir(installed) == installed);
    BOOST_TEST_REQUIRE(System::setEnvVar("RTTR_GAME_DIR", empty.string()));
    BOOST_TEST(RttrConfig::getGameDir(installed) == empty);
    logAcc.clearLog();
    BOOST_TEST_REQUIRE(System::removeEnvVar("RTTR_GAME_DIR"));
    // Something the game cannot use (e.g. an installer file) leaves the default alone
    BOOST_TEST_REQUIRE(System::setEnvVar("WARPZONE_DATA_S2_GOLD", (empty / "DATA").string()));
    BOOST_TEST(RttrConfig::getGameDir(empty) == empty);

    BOOST_TEST_REQUIRE(System::removeEnvVar("WARPZONE_DATA_S2_GOLD"));
    fs::remove_all(base);
}

BOOST_AUTO_TEST_CASE(YearIsValid)
{
    const std::string year = rttr::version::GetYear();
    BOOST_TEST(year.length() == 4u);
    int iYear;
    BOOST_TEST_REQUIRE(boost::conversion::try_lexical_convert(year, iYear));
    BOOST_TEST(iYear >= 2000);
    BOOST_TEST(iYear < 3000);
}

BOOST_AUTO_TEST_CASE(BuildDateIsValid)
{
    const std::string date = rttr::version::GetBuildDate();
    BOOST_TEST(date.length() == 8u);
    int iDate;
    BOOST_TEST_REQUIRE(boost::conversion::try_lexical_convert(date, iDate));
    BOOST_TEST(iDate >= 20000000);
    BOOST_TEST(iDate < 30000000);
}
