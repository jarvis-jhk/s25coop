// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "coop/Changelog.h"
#include <boost/test/unit_test.hpp>
#include <sstream>

using namespace coop::changelog;

namespace {
std::vector<Section> sample()
{
    std::istringstream in("# Changelog\n\nIntro text, ignored.\n\n## 0.10.0\n\n- Coop campaigns\n\n"
                          "## 0.9.1\n\n- Fixed a crash when\n  saving `DATA` files\n- [Readme](README.md) link\n\n"
                          "## 0.1.0\r\n\r\n- First build\r\n");
    return parse(in);
}
std::vector<std::string> versions(const std::vector<Section>& sections)
{
    std::vector<std::string> result;
    result.reserve(sections.size());
    for(const auto& s : sections)
        result.push_back(s.version);
    return result;
}
} // namespace

BOOST_AUTO_TEST_SUITE(CoopChangelog)

BOOST_AUTO_TEST_CASE(ParsesSectionsAndStripsMarkdown)
{
    const auto sections = sample();
    BOOST_TEST_REQUIRE(sections.size() == 3u);
    BOOST_TEST(sections[0].version == "0.10.0");
    BOOST_TEST(sections[0].entries == std::vector<std::string>{"Coop campaigns"});
    BOOST_TEST(sections[1].entries
               == (std::vector<std::string>{"Fixed a crash when saving DATA files", "Readme link"}));
    BOOST_TEST(sections[2].version == "0.1.0");
    BOOST_TEST(sections[2].entries == std::vector<std::string>{"First build"});
}

BOOST_AUTO_TEST_CASE(ComparesVersionsNumerically)
{
    BOOST_TEST(compareVersions("0.10.0", "0.9.3") > 0);
    BOOST_TEST(compareVersions("0.1", "0.1.0") == 0);
    BOOST_TEST(compareVersions("1.2.3", "1.2.4") < 0);
    BOOST_TEST(isReleaseVersion("0.1.1"));
    BOOST_TEST(!isReleaseVersion("20260927"));
    BOOST_TEST(!isReleaseVersion(""));
    BOOST_TEST(!isReleaseVersion("0.1.1-dev"));
}

BOOST_AUTO_TEST_CASE(ShowsWhatIsNewSinceLastRun)
{
    const auto sections = sample();
    // Update over two releases: both, newest first
    BOOST_TEST(versions(newSince(sections, "0.1.0", "0.10.0")) == (std::vector<std::string>{"0.10.0", "0.9.1"}));
    // Same version again: nothing
    BOOST_TEST(newSince(sections, "0.10.0", "0.10.0").empty());
    // Downgrade: nothing
    BOOST_TEST(newSince(sections, "0.10.0", "0.9.1").empty());
    // First run: only the newest section that belongs to this build
    BOOST_TEST(versions(newSince(sections, "", "0.9.1")) == std::vector<std::string>{"0.9.1"});
    // Dev builds (date as version) never pop up
    BOOST_TEST(newSince(sections, "0.1.0", "20260927").empty());
}

BOOST_AUTO_TEST_SUITE_END()
