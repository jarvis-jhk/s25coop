// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "RttrConfig.h"
#include "SmackerVideo.h"
#include <rttr/test/TmpFolder.hpp>
#include <boost/nowide/cstdlib.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <cstring>

namespace bfs = boost::filesystem;

BOOST_AUTO_TEST_SUITE(SmackerVideoTests)

BOOST_AUTO_TEST_CASE(FindsVideoIgnoringCase)
{
    const bfs::path oldGame = RTTRCONFIG.ExpandPath("<RTTR_GAME>");
    rttr::test::TmpFolder game;
    RTTRCONFIG.overridePathMapping("GAME", game.get());
    BOOST_TEST(SmackerVideo::findOriginal("INTRO.SMK").empty());
    bfs::create_directories(game / "video");
    boost::nowide::ofstream(game / "video" / "Intro.smk") << "not a video";
    BOOST_TEST(SmackerVideo::findOriginal("INTRO.SMK") == game / "video" / "Intro.smk");
    BOOST_TEST(SmackerVideo::findOriginal("CREDITS.SMK").empty());

    SmackerVideo video;
    BOOST_TEST(!video.open(game / "video" / "Intro.smk"));
    BOOST_TEST(!video.isOpen());
    BOOST_TEST(!video.nextFrame());
    BOOST_TEST(video.decodeAudioAsWav().empty());
    RTTRCONFIG.overridePathMapping("GAME", oldGame);
}

// Only with the original game: RTTR_COOP_S2_DIR=<folder with VIDEO/INTRO.SMK> in the environment.
// CI has no game data, so the body is excluded from the test coverage check.
BOOST_AUTO_TEST_CASE(PlaysOriginalIntro)
{
    const char* s2 = boost::nowide::getenv("RTTR_COOP_S2_DIR");
    if(!s2 || !bfs::exists(bfs::path(s2) / "VIDEO" / "INTRO.SMK"))
    {
        BOOST_TEST_MESSAGE("RTTR_COOP_S2_DIR not set, original intro not tested");
        return;
    }
    // LCOV_EXCL_START
    SmackerVideo video;
    BOOST_TEST_REQUIRE(video.open(bfs::path(s2) / "VIDEO" / "INTRO.SMK"));
    BOOST_TEST(video.getSize().x == 320u);
    BOOST_TEST(video.getSize().y == 200u);
    BOOST_TEST(video.getNumFrames() == 2224u);
    BOOST_TEST(video.getFrameDuration().count() == 66000);
    BOOST_TEST(video.getFrameBGRA().size() == 320u * 200u * 4u);
    unsigned frames = 1;
    bool anyColor = false;
    while(video.nextFrame())
    {
        ++frames;
        if(frames == 1000)
        {
            const auto& px = video.getFrameBGRA();
            for(size_t i = 0; i < px.size() && !anyColor; i += 4)
                anyColor = px[i] || px[i + 1] || px[i + 2];
        }
    }
    BOOST_TEST(frames == 2224u);
    BOOST_TEST(anyColor);

    const auto wav = video.decodeAudioAsWav();
    BOOST_TEST_REQUIRE(wav.size() > 44u);
    BOOST_TEST(std::memcmp(wav.data(), "RIFF", 4) == 0);
    BOOST_TEST(std::memcmp(wav.data() + 8, "WAVEfmt ", 8) == 0);
    // 22050 Hz, 8 bit stereo: about 146 s, like the 2224 frames of 66 ms
    const uint32_t rate = wav[24] | wav[25] << 8 | wav[26] << 16;
    BOOST_TEST(rate == 22050u);
    BOOST_TEST(wav[22] == 2); // channels
    const double seconds = (wav.size() - 44) / (rate * 2.0);
    BOOST_TEST(seconds > 140.);
    BOOST_TEST(seconds < 150.);
    // LCOV_EXCL_STOP
}

BOOST_AUTO_TEST_SUITE_END()
