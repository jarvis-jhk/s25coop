// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Loader.h"
#include "MenuPadFixture.h"
#include "MusicPlayer.h"
#include "Playlist.h"
#include "RttrConfig.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlComboBox.h"
#include "controls/ctrlEdit.h"
#include "controls/ctrlGroup.h"
#include "controls/ctrlList.h"
#include "controls/ctrlOptionGroup.h"
#include "desktops/dskOptions.h"
#include "driver/KeyEvent.h"
#include "files.h"
#include "helpers/optional_io.h"
#include "ingameWindows/iwMusicPlayer.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include "s25util/Log.h"
#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr PadDeviceId pad = 100;
const std::vector<std::string> songs{"s01", "s02", "s03"};

[[noreturn]] void throwCleanupProbe()
{
    throw std::runtime_error("cleanup probe");
}

struct MusicPlayerPadFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder userData;
    rttr::test::ConfigOverride userDataOverride{"USERDATA", userData};
    MusicPlayer& player = MUSICPLAYER;
    Playlist savedPlaylist = player.GetPlaylist();
    std::string& playlistSetting = SETTINGS.sound.playlist;
    std::string savedSetting = playlistSetting;
    const boost::filesystem::path playlistPath = RTTRCONFIG.ExpandPath(s25::folders::playlists) / "Controller.pll";
    iwMusicPlayer* window = nullptr;

    MusicPlayerPadFixture()
    {
        LOADER.LoadDummyLanguageFiles();
        boost::filesystem::create_directories(playlistPath.parent_path());
        BOOST_TEST_REQUIRE(Playlist(songs, 0, false).SaveAs(playlistPath));
        playlistSetting = playlistPath.string();
        player.SetPlaylist(Playlist(songs, 0, false));
    }
    ~MusicPlayerPadFixture() override { playlistSetting.swap(savedSetting); }

    void focusUntil(const Window* target, const PadButton direction = PadButton::RightShoulder)
    {
        for(unsigned i = 0; i < 40 && focused(0) != target; ++i)
            press(pad, direction);
        BOOST_TEST_REQUIRE(focused(0) == target);
    }

    void enter()
    {
        WINDOWMANAGER.Switch(std::make_unique<dskOptions>());
        frame();
        pickUp(pad);
        const auto groups = desktop()->GetCtrls<ctrlOptionGroup>();
        BOOST_TEST_REQUIRE(groups.size() == 1u);
        const auto tabs = groups.front()->GetCtrls<ctrlButton>();
        BOOST_TEST_REQUIRE(tabs.size() == 3u);
        focusUntil(tabs.back());
        press(pad, PadButton::A);
        Window* musicButton = nullptr;
        for(auto* group : desktop()->GetCtrls<ctrlGroup>())
        {
            for(auto* button : group->GetCtrls<ctrlButton>())
            {
                if(button->GetPos() == DrawPoint(280, 220))
                    musicButton = button;
            }
        }
        BOOST_TEST_REQUIRE(musicButton != nullptr);
        focusUntil(musicButton);
        press(pad, PadButton::A);
        window = dynamic_cast<iwMusicPlayer*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(window != nullptr);
        frame();
        BOOST_TEST_REQUIRE(focused(0) == window->GetCtrl<ctrlList>(0));
    }

    ctrlList& tracks() { return *window->GetCtrl<ctrlList>(0); }

    void act(const unsigned id, const PadButton direction = PadButton::RightShoulder)
    {
        focusUntil(window->GetCtrl<ctrlButton>(id), direction);
        press(pad, PadButton::A);
    }

    void expectTracks(const std::vector<std::string>& expected)
    {
        BOOST_TEST_REQUIRE(tracks().GetNumLines() == expected.size());
        for(unsigned i = 0; i < expected.size(); ++i)
            BOOST_TEST(tracks().GetItemText(i) == expected[i]);
    }

    void closeAndExpect(const std::vector<std::string>& expected)
    {
        press(pad, PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(desktopAs<dskOptions>() != nullptr);
        BOOST_TEST(player.GetPlaylist().getSongs() == expected, boost::test_tools::per_element());
        Playlist persisted;
        BOOST_TEST_REQUIRE(persisted.Load(LOG, playlistPath));
        BOOST_TEST(persisted.getSongs() == expected, boost::test_tools::per_element());
        BOOST_TEST(playlistSetting == playlistPath.string());
    }

    template<class F>
    void run(F&& test)
    {
        try
        {
            std::forward<F>(test)();
        } catch(...)
        {
            finish();
            throw;
        }
        finish();
    }

    void finish()
    {
        // Potentially throwing singleton cleanup belongs in the test body, not fixture destruction.
        player.Stop();
        player.SetPlaylist(std::move(savedPlaylist));
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(MenuPadMusicPlayerTests)

BOOST_FIXTURE_TEST_CASE(MovingDownAloneUpdatesActivePlaylistAndSavedFile, MusicPlayerPadFixture)
{
    run([&] {
        enter();
        press(pad, PadButton::DpadDown);
        BOOST_TEST_REQUIRE((tracks().GetSelection() == 0u));
        act(9); // Move track down.
        expectTracks({"s02", "s01", "s03"});
        BOOST_TEST((tracks().GetSelection() == 1u));
        closeAndExpect({"s02", "s01", "s03"});
        BOOST_TEST(player.GetPlaylist().getCurrentSong() == "s02");
    });
}

BOOST_FIXTURE_TEST_CASE(MovingUpAloneUpdatesActivePlaylistAndSavedFile, MusicPlayerPadFixture)
{
    run([&] {
        enter();
        pressN(pad, PadButton::DpadDown, 2);
        BOOST_TEST_REQUIRE((tracks().GetSelection() == 1u));
        act(8); // Move track up.
        expectTracks({"s02", "s01", "s03"});
        BOOST_TEST((tracks().GetSelection() == 0u));
        closeAndExpect({"s02", "s01", "s03"});
        BOOST_TEST(player.GetPlaylist().getCurrentSong() == "s02");
    });
}

BOOST_FIXTURE_TEST_CASE(ReorderBoundariesDoNotRestartPlayback, MusicPlayerPadFixture)
{
    run([&] {
        enter();
        BOOST_TEST_REQUIRE(!tracks().GetSelection());
        act(8); // No selection, no change.
        act(9);
        focusUntil(&tracks(), PadButton::LeftShoulder);
        press(pad, PadButton::DpadDown);
        act(8); // First track cannot move up.
        focusUntil(&tracks(), PadButton::LeftShoulder);
        pressN(pad, PadButton::DpadDown, 2);
        BOOST_TEST_REQUIRE((tracks().GetSelection() == 2u));
        act(9); // Last track cannot move down.
        closeAndExpect(songs);
        BOOST_TEST(player.GetPlaylist().getCurrentSong().empty());
    });
}

BOOST_FIXTURE_TEST_CASE(TrackActivationStartsTheSelectedSongAndRemovalPersists, MusicPlayerPadFixture)
{
    run([&] {
        enter();
        pressN(pad, PadButton::DpadDown, 2);
        press(pad, PadButton::A);
        BOOST_TEST(player.GetPlaylist().getCurrentSong() == "s02");
        act(7); // Remove selected track.
        expectTracks({"s01", "s03"});
        closeAndExpect({"s01", "s03"});
    });
}

BOOST_FIXTURE_TEST_CASE(BDiscardsPlaylistDropdownBeforeClosingThePlayer, MusicPlayerPadFixture)
{
    run([&] {
        enter();
        auto* playlists = window->GetCtrl<ctrlComboBox>(2);
        BOOST_TEST_REQUIRE(playlists->GetNumItems() == 2u);
        const auto selected = playlists->GetSelection();
        focusUntil(playlists);
        press(pad, PadButton::A);
        BOOST_TEST_REQUIRE(playlists->IsListOpen());
        press(pad, PadButton::DpadUp); // Browse the built-in playlist without loading it.
        BOOST_TEST_REQUIRE((playlists->GetSelection() != selected));
        press(pad, PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        BOOST_TEST(!playlists->IsListOpen());
        BOOST_TEST((playlists->GetSelection() == selected));
        expectTracks(songs);
        closeAndExpect(songs);
    });
}

BOOST_FIXTURE_TEST_CASE(BCancelsTrackAndDirectoryInputWithoutRestartingPlayback, MusicPlayerPadFixture)
{
    run([&] {
        for(const unsigned id : {5u, 6u}) // Add Track / Add Directory.
        {
            enter();
            act(id);
            frame();
            auto* input = WINDOWMANAGER.GetTopMostWindow();
            BOOST_TEST_REQUIRE(input != window);
            const auto edits = input->GetCtrls<ctrlEdit>();
            BOOST_TEST_REQUIRE(edits.size() == 1u);
            WINDOWMANAGER.Msg_KeyDown(KeyEvent('s'));
            WINDOWMANAGER.Msg_KeyDown(KeyEvent('0'));
            WINDOWMANAGER.Msg_KeyDown(KeyEvent('4'));
            BOOST_TEST_REQUIRE(edits.front()->GetText() == "s04");
            press(pad, PadButton::B);
            BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
            expectTracks(songs);
            closeAndExpect(songs);
            BOOST_TEST(player.GetPlaylist().getCurrentSong().empty());
            player.Stop();
            player.SetPlaylist(Playlist(songs, 0, false));
            padInput().Reset();
        }
    });
}

BOOST_FIXTURE_TEST_CASE(RepeatAndRandomControlsUpdatePlaybackAndSavedFile, MusicPlayerPadFixture)
{
    run([&] {
        enter();
        act(12); // Zero repeats stays at zero.
        act(11, PadButton::LeftShoulder);
        act(11);
        act(12, PadButton::RightShoulder);
        act(13);
        closeAndExpect(songs);
        BOOST_TEST(player.GetPlaylist().getNumRepeats() == 1u);
        BOOST_TEST(player.GetPlaylist().isRandomized());
        Playlist persisted;
        BOOST_TEST_REQUIRE(persisted.Load(LOG, playlistPath));
        BOOST_TEST(persisted.getNumRepeats() == 1u);
        BOOST_TEST(persisted.isRandomized());
    });
}

BOOST_FIXTURE_TEST_CASE(ConfirmedTrackAndDirectoryInputStillUpdatesPlayback, MusicPlayerPadFixture)
{
    run([&] {
        for(const bool directory : {false, true})
        {
            enter();
            act(directory ? 6u : 5u);
            frame();
            auto* input = WINDOWMANAGER.GetTopMostWindow();
            BOOST_TEST_REQUIRE(input != window);
            const auto folder = playlistPath.parent_path() / "tracks";
            const auto track = folder / "extra.ogg";
            boost::filesystem::create_directories(folder);
            {
                boost::nowide::ofstream file(track);
                file << "Dummy audio path: this track is queued after the built-in tracks, never decoded.";
                BOOST_TEST_REQUIRE(static_cast<bool>(file));
            }
            const auto text = directory ? folder.string() : std::string("s04");
            for(const unsigned char c : text)
                WINDOWMANAGER.Msg_KeyDown(KeyEvent(c));
            const auto edits = input->GetCtrls<ctrlEdit>();
            BOOST_TEST_REQUIRE(edits.size() == 1u);
            BOOST_TEST_REQUIRE(edits.front()->GetText() == text);
            const auto buttons = input->GetCtrls<ctrlButton>();
            BOOST_TEST_REQUIRE(buttons.size() == 2u);
            focusUntil(buttons.front()); // Confirm input through A.
            press(pad, PadButton::A);
            BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
            auto expected = songs;
            expected.push_back(directory ? track.string() : "s04");
            expectTracks(expected);
            closeAndExpect(expected);
            BOOST_TEST(player.GetPlaylist().getCurrentSong() == "s01");
            player.Stop();
            player.SetPlaylist(Playlist(songs, 0, false));
            padInput().Reset();
        }
    });
}

BOOST_FIXTURE_TEST_CASE(ExceptionRestoresTheProductionPlaylist, MusicPlayerPadFixture)
{
    const auto originalSongs = savedPlaylist.getSongs();
    const auto originalCurrent = savedPlaylist.getCurrentSong();
    BOOST_CHECK_THROW(run(throwCleanupProbe), std::runtime_error);
    BOOST_TEST(player.GetPlaylist().getSongs() == originalSongs, boost::test_tools::per_element());
    BOOST_TEST(player.GetPlaylist().getCurrentSong() == originalCurrent);
}

BOOST_AUTO_TEST_SUITE_END()
