// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Loader.h"
#include "MenuPadFixture.h"
#include "RttrConfig.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlMapSelection.h"
#include "controls/ctrlMultiline.h"
#include "controls/ctrlTable.h"
#include "controls/ctrlText.h"
#include "desktops/dskCampaignMissionSelection.h"
#include "desktops/dskCampaignSelection.h"
#include "files.h"
#include "helpers/optional_io.h"
#include "ogl/glFont.h"
#include "test/testConfig.h"
#include "libsiedler2/Archiv.h"
#include "libsiedler2/ArchivItem_Bitmap_Raw.h"
#include "libsiedler2/PixelBufferBGRA.h"
#include "libsiedler2/libsiedler2.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>

namespace {
constexpr PadDeviceId pad = 103;
constexpr unsigned numCampaigns = 9;

struct CampaignArtworkFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder data;
    rttr::test::ConfigOverride builtinOverride{"RTTR", data / "rttr"};
    rttr::test::ConfigOverride userOverride{"USERDATA", data / "user"};
    // Campaign scripts may only name game paths (<RTTR_...>) or files beside the campaign, so the artwork
    // lives in the overridden user data folder. Two levels deep: CampaignDescription takes
    // "<RTTR_USERDATA>/x/file" for a campaign-relative sub folder and rejects its non-alphanumeric name.
    const boost::filesystem::path art = data / "user" / "art" / "pics";
    const boost::filesystem::path goodImage = art / "z_artwork.bmp";

    std::string gamePath(const boost::filesystem::path& file) const
    {
        return "<RTTR_USERDATA>/art/pics/" + file.lexically_relative(art).generic_string();
    }

    void createCampaign(const std::string& name, const std::string& image, const bool selectionMap = false) const
    {
        const auto folder = RTTRCONFIG.ExpandPath(s25::folders::campaignsUser) / name;
        boost::filesystem::create_directories(folder);
        boost::filesystem::copy_file(rttr::test::rttrBaseDir / "tests" / "testData" / "maps" / "LuaFunctions.SWD",
                                     folder / "Mission.SWD");
        boost::nowide::ofstream lua(folder / "Mission.lua");
        lua << "function getRequiredLuaVersion() return 1 end\n";
        BOOST_TEST_REQUIRE(static_cast<bool>(lua));
        boost::nowide::ofstream campaign(folder / "campaign.lua");
        campaign << "function getRequiredLuaVersion() return 1 end\ncampaign = {version=1, author='Test', name='"
                 << name << "', shortDescription='Artwork', longDescription='Playable " << name
                 << "', maxHumanPlayers=1, difficulty='easy', maps={'Mission.SWD'}, chaptersEnabled={0}";
        if(!image.empty())
            campaign << ", image='" << image << "'";
        if(selectionMap)
        {
            campaign << ", selectionMap={";
            for(const auto* resource : {"background", "map", "missionMapMask", "marker", "conquered"})
                campaign << resource << "={'" << gamePath(goodImage) << "',0},";
            campaign << "backgroundOffset={0,0}, disabledColor=0x70000000, missionSelectionInfos={{0xff00ff00,4,4}}}";
        }
        campaign << "}\n";
        BOOST_TEST_REQUIRE(static_cast<bool>(campaign));
    }

    void enter()
    {
        boost::filesystem::create_directories(RTTRCONFIG.ExpandPath(s25::folders::campaignsBuiltin));
        auto bitmap = std::make_unique<libsiedler2::ArchivItem_Bitmap_Raw>();
        BOOST_TEST_REQUIRE(bitmap->create(libsiedler2::PixelBufferBGRA(8, 8, libsiedler2::ColorBGRA(0xff00ff00))) == 0);
        libsiedler2::Archiv archive;
        archive.push(std::move(bitmap));
        boost::filesystem::create_directories(art);
        BOOST_TEST_REQUIRE(libsiedler2::Write(goodImage, archive) == 0);
        const auto corrupt = art / "b_artwork.bmp";
        {
            boost::nowide::ofstream file(corrupt);
            file << "not a bitmap";
            BOOST_TEST_REQUIRE(static_cast<bool>(file));
        }
        createCampaign("Absent", "");
        createCampaign("Missing", gamePath(art / "a_artwork.bmp"));
        createCampaign("Corrupt", gamePath(corrupt));
        createCampaign("Valid", gamePath(goodImage));
        const auto shortName = art / "campaign-preview.bmp";
        const auto longName = art / "campaignpreviewlong.bmp";
        boost::filesystem::copy_file(goodImage, shortName);
        boost::filesystem::copy_file(goodImage, longName);
        createCampaign("InvalidShort", gamePath(shortName));
        createCampaign("InvalidLong", gamePath(longName));
        boost::filesystem::create_directories(art / "good");
        boost::filesystem::create_directories(art / "bad");
        boost::filesystem::copy_file(goodImage, art / "good" / "preview.bmp");
        boost::filesystem::copy_file(corrupt, art / "bad" / "preview.bmp");
        createCampaign("CollisionGood", gamePath(art / "good" / "preview.bmp"));
        createCampaign("CollisionBad", gamePath(art / "bad" / "preview.bmp"));
        createCampaign("Map", gamePath(corrupt), true);
        WINDOWMANAGER.Switch(std::make_unique<dskCampaignSelection>(CreateServerInfo(ServerType::Local, 0, "Artwork")));
        frame();
        pickUp(pad);
        waitForCampaigns();
        frame();
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST_REQUIRE(focused(0) == &table());
    }

    void waitForCampaigns()
    {
        for(unsigned frames = 0; frames < 200; ++frames)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            frame();
            if(table().GetNumRows() == numCampaigns)
                break;
        }
        BOOST_TEST_REQUIRE(table().GetNumRows() == numCampaigns);
    }

    ctrlTable& table() const
    {
        auto* screen = desktopAs<dskCampaignSelection>();
        BOOST_TEST_REQUIRE(screen != nullptr);
        const auto tables = screen->GetCtrls<ctrlTable>();
        BOOST_TEST_REQUIRE(tables.size() == 1u);
        return *tables.front();
    }

    ctrlText& fallback() const
    {
        const auto texts = desktopAs<dskCampaignSelection>()->GetCtrls<ctrlText>();
        // The image-area text is separate from the title and campaign description.
        const DrawPoint pos(640, 20 + LargeFont->getHeight() + 110);
        const auto found =
          std::find_if(texts.begin(), texts.end(), [pos](const auto* text) { return text->GetPos() == pos; });
        BOOST_TEST_REQUIRE((found != texts.end()));
        return **found;
    }

    void select(const std::string& name)
    {
        std::optional<unsigned> target;
        for(unsigned row = 0; row < table().GetNumRows(); ++row)
        {
            if(table().GetItemText(row, 0) == name)
                target = row;
        }
        BOOST_TEST_REQUIRE(target.has_value());
        for(unsigned i = 0; i < numCampaigns + 1 && table().GetSelection() != target; ++i)
        {
            const auto selection = table().GetSelection();
            press(pad, selection && *selection > *target ? PadButton::DpadUp : PadButton::DpadDown);
        }
        BOOST_TEST_REQUIRE((table().GetSelection() == target));
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(MenuPadCampaignArtworkTests)

BOOST_FIXTURE_TEST_CASE(OptionalArtworkFailuresDoNotDiscardCampaignsOrLaterValidPictures, CampaignArtworkFixture)
{
    enter();
    BOOST_TEST(!fallback().IsVisible());
    for(const std::string name : {"Missing", "Corrupt", "Absent", "InvalidShort", "InvalidLong"})
    {
        select(name);
        BOOST_TEST(fallback().IsVisible());
        BOOST_TEST(!fallback().GetText().empty());
        const auto descriptions = desktopAs<dskCampaignSelection>()->GetCtrls<ctrlMultiline>();
        BOOST_TEST_REQUIRE(descriptions.size() == 1u);
        BOOST_TEST(descriptions.front()->IsVisible());
        BOOST_TEST_REQUIRE(descriptions.front()->GetNumLines() > 0u);
        BOOST_TEST(descriptions.front()->GetLine(0) == "Playable " + name);
        select("Valid");
        BOOST_TEST(!fallback().IsVisible());
        BOOST_TEST(LOADER.GetImageN(ResourceId::fromPath(goodImage.generic_string()), 0) != nullptr);
    }
}

BOOST_FIXTURE_TEST_CASE(CampaignsWithoutUsableArtworkStillContinueThroughTheController, CampaignArtworkFixture)
{
    enter();
    for(const std::string name : {"Absent", "Missing", "Corrupt", "Valid", "InvalidShort", "InvalidLong"})
    {
        select(name);
        press(pad, PadButton::A);
        BOOST_TEST_REQUIRE(desktopAs<dskCampaignMissionSelection>() != nullptr);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        press(pad, PadButton::B);
        BOOST_TEST_REQUIRE(desktopAs<dskCampaignSelection>() != nullptr);
        waitForCampaigns();
        frame();
        BOOST_TEST_REQUIRE(focused(0) == &table());
        // Returning to the earlier control must work after physically moving to a later one.
        press(pad, PadButton::RightShoulder);
        BOOST_TEST_REQUIRE(focused(0) != &table());
        for(unsigned i = 0; i < 4 && focused(0) != &table(); ++i)
            press(pad, PadButton::LeftShoulder);
        BOOST_TEST_REQUIRE(focused(0) == &table());
    }
}

BOOST_FIXTURE_TEST_CASE(EqualFilenamesAndSelectionMapsNeverShowStaleOrFallbackPictures, CampaignArtworkFixture)
{
    enter();
    for(unsigned i = 0; i < 2; ++i)
    {
        select("CollisionGood");
        BOOST_TEST(!fallback().IsVisible());
        select("CollisionBad");
        BOOST_TEST(fallback().IsVisible());
        select("Map");
        BOOST_TEST(!fallback().IsVisible());
        const auto maps = desktopAs<dskCampaignSelection>()->GetCtrls<ctrlMapSelection>();
        BOOST_TEST_REQUIRE(maps.size() == 1u);
        BOOST_TEST(maps.front()->IsVisible());
        select("Valid");
        BOOST_TEST(!fallback().IsVisible());
        BOOST_TEST(desktopAs<dskCampaignSelection>()->GetCtrls<ctrlMapSelection>().empty());
        select("Missing");
        BOOST_TEST(fallback().IsVisible());
        BOOST_TEST(desktopAs<dskCampaignSelection>()->GetCtrls<ctrlMapSelection>().empty());
    }
}

BOOST_AUTO_TEST_SUITE_END()
