// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "RTTR_Version.h"
#include "Settings.h"
#include "WindowManager.h"
#include "controls/ctrlMultiline.h"
#include "coop/Changelog.h"
#include "desktops/dskMainMenu.h"
#include "ingameWindows/iwChangelog.h"
#include "uiHelper/uiHelpers.hpp"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/filesystem/operations.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <thread>

namespace bfs = boost::filesystem;

BOOST_FIXTURE_TEST_SUITE(CoopChangelogWindow, uiHelper::Fixture)

BOOST_AUTO_TEST_CASE(ShowsGivenSections)
{
    const std::vector<coop::changelog::Section> sections{{"0.2.0", {"Coop", "Controller"}}, {"0.1.1", {"Reports"}}};
    const iwChangelog wnd(sections);
    const auto txts = wnd.GetCtrls<ctrlMultiline>();
    BOOST_TEST_REQUIRE(txts.size() == 1u);
    std::vector<std::string> lines;
    for(unsigned i = 0; i < txts[0]->GetNumLines(); i++)
        lines.push_back(txts[0]->GetLine(i));
    const std::vector<std::string> expected{"Version 0.2.0", "- Coop", "- Controller", "", "Version 0.1.1",
                                            "- Reports",     ""};
    BOOST_TEST(lines == expected, boost::test_tools::per_element());
}

// The whole path a player takes: new version installed, game starts, main menu opens the window by itself
BOOST_AUTO_TEST_CASE(MainMenuShowsChangelogAfterUpdate)
{
    const std::string current = rttr::version::GetVersion();
    if(!coop::changelog::isReleaseVersion(current))
    {
        BOOST_TEST_MESSAGE("Dev build (" << current << "), the changelog pops up only in release builds");
        return;
    }
    rttr::test::TmpFolder tmp;
    bfs::create_directories(tmp / "texte");
    {
        boost::nowide::ofstream f(tmp / "texte" / "CHANGELOG.md");
        f << "# Changelog\n\n## " << current << "\n\n- Brand new\n\n## 0.0.1\n\n- Old news\n";
    }
    rttr::test::ConfigOverride rttrOverride("RTTR", tmp.get());
    SETTINGS.global.coopChangelogSeen = "0.0.1";

    WINDOWMANAGER.Switch(std::make_unique<dskMainMenu>());
    WINDOWMANAGER.Draw(); // performs the desktop switch
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    WINDOWMANAGER.Draw(); // fires the timer
    WINDOWMANAGER.Draw();

    const auto* wnd = dynamic_cast<const iwChangelog*>(WINDOWMANAGER.GetTopMostWindow());
    BOOST_TEST_REQUIRE(wnd);
    const auto* txt = wnd->GetCtrls<ctrlMultiline>().at(0);
    BOOST_TEST(txt->GetLine(0) == "Version " + current);
    BOOST_TEST(txt->GetLine(1) == "- Brand new");
    BOOST_TEST(txt->GetNumLines() == 3u); // not the old section
    BOOST_TEST(SETTINGS.global.coopChangelogSeen == current);

    // Back to the main menu in the same run: no second pop-up
    WINDOWMANAGER.CloseNow(const_cast<iwChangelog*>(wnd));
    WINDOWMANAGER.Switch(std::make_unique<dskMainMenu>());
    WINDOWMANAGER.Draw();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    WINDOWMANAGER.Draw();
    WINDOWMANAGER.Draw();
    BOOST_TEST(!dynamic_cast<const iwChangelog*>(WINDOWMANAGER.GetTopMostWindow()));
}

BOOST_AUTO_TEST_SUITE_END()
