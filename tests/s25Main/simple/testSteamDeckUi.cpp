// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "SteamDeckUi.h"
#include "TvDisplay.h"
#include "rttr/test/TmpFolder.hpp"
#include <boost/filesystem/operations.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(SteamDeckDetection)

BOOST_AUTO_TEST_CASE(ExplicitSteamFlagWinsWithoutHardwareFiles)
{
    rttr::test::TmpFolder dmi;
    BOOST_TEST(deck::Detect("1", dmi));
    BOOST_TEST(!deck::Detect("0", dmi));
    BOOST_TEST(!deck::Detect("", dmi));
    BOOST_TEST(!deck::Detect("true", dmi));
    BOOST_TEST(!deck::Detect("SteamOS", dmi));
    BOOST_TEST(!deck::Detect("1 ", dmi));
    BOOST_TEST(!deck::Detect("", dmi / "missing"));
}

BOOST_AUTO_TEST_CASE(RequiresValveAndAnExactLcdOrOledProduct)
{
    rttr::test::TmpFolder dmi;
    for(const auto* vendor : {"Valve", "Other", "", "Valve Corporation"})
    {
        for(const auto* product : {"Jupiter", "Galileo", "Steam Deck", "Jupiter clone", "", "Other"})
        {
            BOOST_TEST_CONTEXT(vendor << "/" << product)
            {
                boost::nowide::ofstream(dmi / "sys_vendor") << vendor << '\n';
                boost::nowide::ofstream(dmi / "product_name") << product << '\n';
                const bool expected =
                  std::string_view(vendor) == "Valve"
                  && (std::string_view(product) == "Jupiter" || std::string_view(product) == "Galileo");
                BOOST_TEST(deck::Detect("", dmi) == expected);
                BOOST_TEST(deck::Detect("unexpected", dmi) == expected);
                BOOST_TEST(!deck::Detect("0", dmi));
                BOOST_TEST(deck::Detect("1", dmi));
            }
        }
    }
    boost::nowide::ofstream(dmi / "sys_vendor") << "Valve\n";
    boost::filesystem::remove(dmi / "product_name");
    BOOST_TEST(!deck::Detect("", dmi));
    boost::filesystem::remove(dmi / "sys_vendor");
    boost::nowide::ofstream(dmi / "product_name") << "Galileo\n";
    BOOST_TEST(!deck::Detect("", dmi));
}

BOOST_AUTO_TEST_CASE(ReferenceProfilePrecedence)
{
    BOOST_TEST(deck::UiReferenceHeight(false, false) == 0u);
    BOOST_TEST(deck::UiReferenceHeight(false, true) == 640u);
    BOOST_TEST(deck::UiReferenceHeight(true, false) == tv::UI_REFERENCE_HEIGHT);
    BOOST_TEST(deck::UiReferenceHeight(true, true) == tv::UI_REFERENCE_HEIGHT);
}

BOOST_AUTO_TEST_SUITE_END()
