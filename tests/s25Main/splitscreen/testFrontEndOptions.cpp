// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Loader.h"
#include "MenuPadFixture.h"
#include "MusicPlayer.h"
#include "RttrConfig.h"
#include "SteamDeckUi.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlComboBox.h"
#include "controls/ctrlProgress.h"
#include "controls/ctrlText.h"
#include "desktops/dskFrontEndOptions.h"
#include "desktops/dskHome.h"
#include "desktops/dskMainMenu.h"
#include "desktops/dskOptions.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "drivers/AudioDriverWrapper.h"
#include "files.h"
#include "ingameWindows/iwMsgbox.h"
#include "ingameWindows/iwMusicPlayer.h"
#include "ingameWindows/iwTextfile.h"
#include "languages.h"
#include "libsiedler2/ArchivItem_Ini.h"
#include "libsiedler2/libsiedler2.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/test/unit_test.hpp>
#include <stdexcept>

namespace {
using Page = dskFrontEndOptions;
constexpr PadDeviceId pad = 82;

struct OptionsFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder userData;
    rttr::test::ConfigOverride userDataOverride{"USERDATA", userData};
    rttr::test::ConfigOverride gameOverride{"GAME", userData};
    const decltype(SETTINGS.video) savedVideo = SETTINGS.video;
    const decltype(SETTINGS.sound) savedSound = SETTINGS.sound;
    const decltype(SETTINGS.interface) savedInterface = SETTINGS.interface;
    const bool savedSmartCursor = SETTINGS.global.smartCursor;
    const std::string savedLanguage = SETTINGS.language.language;
    const VideoMode savedSize = VIDEODRIVER.GetWindowSize();
    const DisplayMode savedMode = VIDEODRIVER.GetDisplayMode();
    const unsigned savedScale = VIDEODRIVER.getGuiScale().percent();
    const std::vector<VideoMode> savedModes = video.video_modes_;
    const Playlist savedPlaylist = MUSICPLAYER.GetPlaylist();
    bool cleaned = false;

    OptionsFixture() { LOADER.LoadDummyLanguageFiles(); }

    void finish()
    {
        video.rejectResize_ = false;
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        frame();
        SETTINGS.video = savedVideo;
        SETTINGS.sound = savedSound;
        MUSICPLAYER.SetPlaylist(savedPlaylist);
        restoreAudio();
        SETTINGS.interface = savedInterface;
        SETTINGS.global.smartCursor = savedSmartCursor;
        SETTINGS.language.language = savedLanguage;
        LANGUAGES.setLanguage(savedLanguage);
        VIDEODRIVER.SetMouseWarping(savedSmartCursor);
        VIDEODRIVER.setTargetFramerate(savedVideo.framerate);
        video.video_modes_ = savedModes;
        VIDEODRIVER.setUiReferenceHeight(deck::UiReferenceHeight(savedVideo.tvMode, savedVideo.steamDeckUi));
        VIDEODRIVER.setGuiScalePercent(savedScale);
        VIDEODRIVER.ResizeScreen(savedSize, savedMode);
        // Resize callbacks update windowedSize; preserve the saved configuration independently of driver state.
        SETTINGS.video = savedVideo;
        dskHome::ForgetLastChoice();
        cleaned = true;
    }
    static void restoreAudio()
    {
        AUDIODRIVER.SetMasterEffectVolume(SETTINGS.sound.effectsVolume);
        AUDIODRIVER.SetMusicVolume(SETTINGS.sound.musicVolume);
        if(SETTINGS.sound.musicEnabled)
            MUSICPLAYER.Play();
        else
            MUSICPLAYER.Stop();
    }
    template<class F>
    void run(F&& body)
    {
        try
        {
            body();
        } catch(...)
        {
            finish();
            throw;
        }
        finish();
        BOOST_TEST(SETTINGS.sound.musicVolume == savedSound.musicVolume);
        BOOST_TEST(SETTINGS.video.guiScale == savedVideo.guiScale);
        BOOST_TEST((SETTINGS.video.windowedSize == savedVideo.windowedSize));
        BOOST_TEST((SETTINGS.video.displayMode == savedVideo.displayMode));
        BOOST_TEST(SETTINGS.interface.enableWindowPinning == savedInterface.enableWindowPinning);
        BOOST_TEST(SETTINGS.language.language == savedLanguage);
        BOOST_TEST((VIDEODRIVER.GetWindowSize() == savedSize));
    }
    void enter()
    {
        dskHome::ForgetLastChoice();
        SETTINGS.Save();
        WINDOWMANAGER.Switch(dskHome::Create());
        frame();
        pickUp(pad);
        choose(dskHome::ID_Options);
        BOOST_TEST_REQUIRE(desktopAs<Page>() != nullptr);
    }
    void focus(unsigned id)
    {
        for(unsigned i = 0; i < 32 && focusedId(0) != id; ++i)
            press(pad, focusedId(0) < id ? PadButton::RightShoulder : PadButton::LeftShoulder);
        BOOST_TEST_REQUIRE(focusedId(0) == id);
    }
    void choose(unsigned id)
    {
        focus(id);
        press(pad, PadButton::A);
        frame();
    }
    void select(unsigned id, unsigned index)
    {
        focus(id);
        auto* combo = desktop()->GetCtrl<ctrlComboBox>(id);
        BOOST_TEST_REQUIRE(combo != nullptr);
        press(pad, PadButton::A);
        BOOST_TEST_REQUIRE(combo->IsListOpen());
        for(unsigned i = 0; i < combo->GetNumItems() && combo->GetSelection() != index; ++i)
            press(pad, *combo->GetSelection() < index ? PadButton::DpadDown : PadButton::DpadUp);
        BOOST_TEST_REQUIRE(combo->GetSelection().has_value());
        BOOST_TEST_REQUIRE(*combo->GetSelection() == index);
        press(pad, PadButton::A);
        BOOST_TEST(!combo->IsListOpen());
    }
    void back()
    {
        press(pad, PadButton::B);
        frame();
    }
    void click(Window& control)
    {
        const Rect r = control.GetDrawRect();
        MouseCoords mc(Position((r.left + r.right) / 2, (r.top + r.bottom) / 2));
        mc.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mc);
        mc.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mc);
        frame();
        frame();
    }
    std::string persisted(const std::string& section, const std::string& key) const
    {
        // Reload the written archive without creating/destroying a second Settings singleton.
        libsiedler2::Archiv file;
        BOOST_TEST_REQUIRE(libsiedler2::Load(RTTRCONFIG.ExpandPath(s25::resources::config), file) == 0);
        const auto* values = dynamic_cast<const libsiedler2::ArchivItem_Ini*>(file.find(section));
        BOOST_TEST_REQUIRE(values != nullptr);
        return values->getValue(key);
    }
    void inContent(Window& control)
    {
        const auto& content = desktopAs<Page>()->GetFrame().content;
        const auto r = control.GetDrawRect();
        BOOST_TEST((r.left >= content.left && r.right <= content.right));
        BOOST_TEST((r.top >= content.top && r.bottom <= content.bottom));
    }
};

[[noreturn]] void throwProbe()
{
    throw std::runtime_error("options cleanup probe");
}
} // namespace

BOOST_AUTO_TEST_SUITE(FrontEndOptionsTests)

BOOST_FIXTURE_TEST_CASE(AllGroupsReturnThroughTheTrailWithoutChangingSettings, OptionsFixture)
{
    run([&] {
        enter();
        BOOST_TEST(desktopAs<Page>()->GetItems().size() == 6u);
        for(unsigned id : {Page::ID_Display, Page::ID_Audio, Page::ID_Controls, Page::ID_Language})
        {
            choose(id);
            BOOST_TEST_REQUIRE(desktopAs<Page>() != nullptr);
            BOOST_TEST(desktopAs<Page>()->GetTrail().size() == 2u);
            BOOST_TEST_REQUIRE(focused(0) != nullptr);
            BOOST_TEST(focusedId(0) != Page::ID_btBack);
            BOOST_TEST(!desktopAs<Page>()->GetFooterKeys().empty());
            back();
            BOOST_TEST_REQUIRE(desktopAs<Page>() != nullptr);
            BOOST_TEST((desktopAs<Page>()->GetSection() == Page::Section::Overview));
            BOOST_TEST(focusedId(0) == id);
        }
        BOOST_TEST(SETTINGS.sound.effectsVolume == savedSound.effectsVolume);
        BOOST_TEST(SETTINGS.video.guiScale == savedVideo.guiScale);
        BOOST_TEST(SETTINGS.language.language == savedLanguage);
        back();
        BOOST_TEST_REQUIRE(desktopAs<dskHome>() != nullptr);
        BOOST_TEST(focusedId(0) == dskHome::ID_Options);
    });
}

BOOST_FIXTURE_TEST_CASE(AudioPreviewCancelAndAcceptedValuesPersist, OptionsFixture)
{
    run([&] {
        SETTINGS.sound.effectsEnabled = false;
        SETTINGS.sound.birdsEnabled = false;
        SETTINGS.sound.musicEnabled = false;
        SETTINGS.sound.effectsVolume = 127;
        SETTINGS.sound.musicVolume = 131;
        enter();
        choose(Page::ID_Audio);
        focus(Page::ID_Effects);
        press(pad, PadButton::A);
        press(pad, PadButton::DpadDown);
        BOOST_TEST(!SETTINGS.sound.effectsEnabled);
        press(pad, PadButton::B);
        BOOST_TEST(!SETTINGS.sound.effectsEnabled);
        BOOST_TEST_REQUIRE(desktopAs<Page>() != nullptr);
        select(Page::ID_Effects, 1);
        select(Page::ID_Birds, 1);
        select(Page::ID_Music, 1);
        BOOST_TEST(SETTINGS.sound.effectsEnabled);
        BOOST_TEST(SETTINGS.sound.birdsEnabled);
        BOOST_TEST(SETTINGS.sound.musicEnabled);
        restoreAudio();
        select(Page::ID_Music, 0);
        BOOST_TEST(!SETTINGS.sound.musicEnabled);
        restoreAudio();
        focus(Page::ID_EffectsVolume);
        press(pad, PadButton::DpadRight);
        BOOST_TEST(SETTINGS.sound.effectsVolume == 128u);
        focus(Page::ID_MusicVolume);
        press(pad, PadButton::DpadLeft);
        BOOST_TEST(SETTINGS.sound.musicVolume == 130u);
        // The byte values, rather than quantized percentages, survive leaving and rebuilding the page.
        BOOST_TEST(persisted("sound", "effekte_volume") == "128");
        BOOST_TEST(persisted("sound", "musik_volume") == "130");
        back();
        choose(Page::ID_Audio);
        BOOST_TEST(desktop()->GetCtrl<ctrlProgress>(Page::ID_EffectsVolume)->GetPosition() == 128u);
        BOOST_TEST(desktop()->GetCtrl<ctrlProgress>(Page::ID_MusicVolume)->GetPosition() == 130u);
        choose(Page::ID_MusicPlayer);
        BOOST_TEST_REQUIRE(dynamic_cast<iwMusicPlayer*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        back();
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST((desktopAs<Page>()->GetSection() == Page::Section::Audio));
    });
}

BOOST_FIXTURE_TEST_CASE(MouseVolumeAndKeyboardBackShareThePage, OptionsFixture)
{
    run([&] {
        enter();
        click(*desktop()->GetCtrl<Window>(Page::ID_Audio));
        BOOST_TEST_REQUIRE(desktopAs<Page>() != nullptr);
        auto* volume = desktop()->GetCtrl<ctrlProgress>(Page::ID_EffectsVolume);
        const Rect r = volume->GetDrawRect();
        WINDOWMANAGER.Msg_LeftDown(MouseCoords(Position((r.left + r.right) / 2, (r.top + r.bottom) / 2)));
        BOOST_TEST(SETTINGS.sound.effectsVolume >= 125u);
        BOOST_TEST(SETTINGS.sound.effectsVolume <= 130u);
        const auto before = SETTINGS.sound.effectsVolume;
        WINDOWMANAGER.Msg_WheelUp(MouseCoords(volume->GetDrawRect().getOrigin() + DrawPoint(5, 5)));
        BOOST_TEST(SETTINGS.sound.effectsVolume == before + 1u);
        click(*desktop()->GetCtrl<Window>(Page::ID_btBack));
        BOOST_TEST((desktopAs<Page>()->GetSection() == Page::Section::Overview));
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
        frame();
        BOOST_TEST(desktopAs<dskHome>() != nullptr);
    });
}

BOOST_FIXTURE_TEST_CASE(DisplayModeSizeAndFramerateUseTheRealDriver, OptionsFixture)
{
    run([&] {
        SETTINGS.video.guiScale = 100;
        SETTINGS.video.windowedSize = VideoMode(800, 600);
        SETTINGS.video.fullscreenSize = VideoMode(1280, 800);
        SETTINGS.video.displayMode = DisplayMode::Windowed;
        video.video_modes_ = {VideoMode(800, 600), VideoMode(1280, 800)};
        VIDEODRIVER.setGuiScalePercent(100);
        VIDEODRIVER.ResizeScreen(VideoMode(800, 600), DisplayMode::Windowed);
        enter();
        choose(Page::ID_Display);
        select(Page::ID_Mode, 1);
        BOOST_TEST((VIDEODRIVER.GetDisplayMode() == DisplayMode::Fullscreen));
        BOOST_TEST((VIDEODRIVER.GetWindowSize() == VideoMode(1280, 800)));
        select(Page::ID_Size, 0);
        BOOST_TEST((SETTINGS.video.fullscreenSize == VideoMode(800, 600)));
        BOOST_TEST((VIDEODRIVER.GetWindowSize() == VideoMode(800, 600)));
        select(Page::ID_Mode, 2);
        BOOST_TEST((VIDEODRIVER.GetDisplayMode() == DisplayMode::BorderlessWindow));
        BOOST_TEST(!desktop()->GetCtrl<Window>(Page::ID_Size)->IsVisible());
        select(Page::ID_Mode, 0);
        BOOST_TEST(desktop()->GetCtrl<Window>(Page::ID_Size)->IsVisible());
        select(Page::ID_Size, 1);
        BOOST_TEST((VIDEODRIVER.GetWindowSize() == SETTINGS.video.windowedSize));
        auto* rate = desktop()->GetCtrl<ctrlComboBox>(Page::ID_Framerate);
        select(Page::ID_Framerate, rate->GetNumItems() - 1);
        BOOST_TEST(SETTINGS.video.framerate == Settings::SCREEN_REFRESH_RATES.back());
        BOOST_TEST(persisted("video", "displayMode") == "0");
        BOOST_TEST(persisted("video", "windowed_width") == std::to_string(SETTINGS.video.windowedSize.width));
        BOOST_TEST(persisted("video", "framerate") == std::to_string(Settings::SCREEN_REFRESH_RATES.back()));
    });
}

BOOST_FIXTURE_TEST_CASE(FailedDisplayChangeKeepsTheWorkingModeAndRequiresAcknowledgement, OptionsFixture)
{
    run([&] {
        SETTINGS.video.displayMode = DisplayMode::Windowed;
        SETTINGS.video.windowedSize = VIDEODRIVER.GetWindowSize();
        enter();
        choose(Page::ID_Display);
        video.rejectResize_ = true;
        select(Page::ID_Mode, 1);
        BOOST_TEST_REQUIRE(dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        BOOST_TEST((SETTINGS.video.displayMode == DisplayMode::Windowed));
        BOOST_TEST((VIDEODRIVER.GetDisplayMode() == DisplayMode::Windowed));
        BOOST_TEST(*desktop()->GetCtrl<ctrlComboBox>(Page::ID_Mode)->GetSelection() == 0u);
        BOOST_TEST((VIDEODRIVER.GetWindowSize() == savedSize));
        back();
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() != nullptr);
        press(pad, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        video.rejectResize_ = false;
        back();
        BOOST_TEST((desktopAs<Page>()->GetSection() == Page::Section::Overview));
    });
}

BOOST_FIXTURE_TEST_CASE(AutomaticProfilesResetFixedScaleButDisablingPreservesIt, OptionsFixture)
{
    run([&] {
        SETTINGS.video.guiScale = 125;
        SETTINGS.video.tvMode = false;
        SETTINGS.video.steamDeckUi = false;
        VIDEODRIVER.setGuiScalePercent(125);
        VIDEODRIVER.ResizeScreen(VideoMode(1600, 1200), DisplayMode::Windowed);
        enter();
        choose(Page::ID_Display);
        auto* scale = desktop()->GetCtrl<ctrlComboBox>(Page::ID_Scale);
        BOOST_TEST(scale->GetSelectedText().value() == "125%");
        BOOST_TEST(SETTINGS.video.guiScale == 125u);
        select(Page::ID_Tv, 1);
        BOOST_TEST(SETTINGS.video.tvMode);
        BOOST_TEST(SETTINGS.video.guiScale == 0u);
        BOOST_TEST(persisted("video", "tv_mode") == "1");
        BOOST_TEST(persisted("video", "gui_scale") == "0");
        BOOST_TEST(*scale->GetSelection() == 0u);
        BOOST_TEST(VIDEODRIVER.getGuiScale().percent() == VIDEODRIVER.getGuiScaleRange().recommendedPercent);
        select(Page::ID_Scale, 1);
        BOOST_TEST(SETTINGS.video.guiScale == 100u);
        select(Page::ID_Tv, 0);
        BOOST_TEST(!SETTINGS.video.tvMode);
        BOOST_TEST(SETTINGS.video.guiScale == 100u);
        select(Page::ID_Deck, 1);
        BOOST_TEST(SETTINGS.video.steamDeckUi);
        BOOST_TEST(persisted("video", "steam_deck_ui") == "1");
        BOOST_TEST(SETTINGS.video.guiScale == 0u);
        select(Page::ID_Scale, 1);
        select(Page::ID_Deck, 0);
        BOOST_TEST(!SETTINGS.video.steamDeckUi);
        BOOST_TEST(SETTINGS.video.guiScale == 100u);
    });
}

BOOST_FIXTURE_TEST_CASE(ResizeCancelsDisplayPreviewAndKeepsRowsInsideTheFrame, OptionsFixture)
{
    run([&] {
        SETTINGS.video.guiScale = 100;
        SETTINGS.video.displayMode = DisplayMode::Windowed;
        VIDEODRIVER.setGuiScalePercent(100);
        enter();
        choose(Page::ID_Display);
        for(const VideoMode size : {VideoMode(1280, 800), VideoMode(800, 600)})
        {
            VIDEODRIVER.ResizeScreen(size, DisplayMode::Windowed);
            frame();
            for(unsigned id :
                {Page::ID_Mode, Page::ID_Size, Page::ID_Scale, Page::ID_Tv, Page::ID_Deck, Page::ID_Framerate})
            {
                inContent(*desktop()->GetCtrl<Window>(id));
                inContent(*desktop()->GetCtrl<Window>(Page::ID_LabelStart + id));
                focus(id);
                press(pad, PadButton::A);
                inContent(*desktop()->GetCtrl<ctrlComboBox>(id)->GetCtrl<ctrlList>(0));
                press(pad, PadButton::B);
            }
        }
        focus(Page::ID_Scale);
        press(pad, PadButton::A);
        BOOST_TEST_REQUIRE(desktop()->GetCtrl<ctrlComboBox>(Page::ID_Scale)->IsListOpen());
        VIDEODRIVER.ResizeScreen(VideoMode(1280, 800), DisplayMode::Windowed);
        frame();
        BOOST_TEST(!desktop()->GetCtrl<ctrlComboBox>(Page::ID_Scale)->IsListOpen());
        BOOST_TEST(SETTINGS.video.guiScale == 100u);
        // The driver size is also the visible current window size, after an external resize.
        BOOST_TEST(desktop()->GetCtrl<ctrlComboBox>(Page::ID_Size)->GetSelectedText().value() == "1280 x 800");
        select(Page::ID_Scale, desktop()->GetCtrl<ctrlComboBox>(Page::ID_Scale)->GetNumItems() - 1);
        BOOST_TEST_REQUIRE(SETTINGS.video.guiScale > 100u);
        VIDEODRIVER.ResizeScreen(VideoMode(800, 600), DisplayMode::Windowed);
        frame();
        BOOST_TEST(SETTINGS.video.guiScale == 100u);
        BOOST_TEST(VIDEODRIVER.getGuiScale().percent() == 100u);
        BOOST_TEST(persisted("video", "gui_scale") == "100");
    });
}

BOOST_FIXTURE_TEST_CASE(ControlsCommitOnlyAcceptedChoicesAndHelpClosesFirst, OptionsFixture)
{
    run([&] {
        SETTINGS.interface.mapScrollMode = MapScrollMode::ScrollSame;
        SETTINGS.interface.enableWindowPinning = false;
        SETTINGS.global.smartCursor = false;
        enter();
        choose(Page::ID_Controls);
        select(Page::ID_Scroll, 2);
        select(Page::ID_SmartCursor, 1);
        select(Page::ID_Pinning, 1);
        BOOST_TEST((SETTINGS.interface.mapScrollMode == MapScrollMode::GrabAndDrag));
        BOOST_TEST(SETTINGS.global.smartCursor);
        BOOST_TEST(SETTINGS.interface.enableWindowPinning);
        BOOST_TEST(persisted("interface", "enable_window_pinning") == "1");
        BOOST_TEST(persisted("interface", "map_scroll_mode") == "2");
        BOOST_TEST(persisted("global", "smartCursor") == "1");
        focus(Page::ID_Pinning);
        press(pad, PadButton::A);
        press(pad, PadButton::DpadUp);
        press(pad, PadButton::B);
        BOOST_TEST(SETTINGS.interface.enableWindowPinning);
        choose(Page::ID_KeyboardLayout);
        BOOST_TEST_REQUIRE(dynamic_cast<iwTextfile*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        back();
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST((desktopAs<Page>()->GetSection() == Page::Section::Controls));
        back();
        choose(Page::ID_Controls);
        BOOST_TEST(*desktop()->GetCtrl<ctrlComboBox>(Page::ID_Pinning)->GetSelection() == 1u);
    });
}

BOOST_FIXTURE_TEST_CASE(LanguageConfirmationKeepsTrailAndRestoresHomeFocus, OptionsFixture)
{
    run([&] {
        SETTINGS.language.language = "";
        enter();
        choose(Page::ID_Language);
        BOOST_TEST_REQUIRE(LANGUAGES.size() > 1u);
        focus(Page::ID_LanguageChoice);
        press(pad, PadButton::A);
        press(pad, PadButton::DpadDown);
        BOOST_TEST(SETTINGS.language.language.empty());
        press(pad, PadButton::B);
        BOOST_TEST(SETTINGS.language.language.empty());
        select(Page::ID_LanguageChoice, 1);
        BOOST_TEST(SETTINGS.language.language == LANGUAGES.getLanguage(1).code);
        BOOST_TEST(persisted("language", "language") == LANGUAGES.getLanguage(1).code);
        BOOST_TEST(desktopAs<Page>()->GetTrail().size() == 2u);
        back();
        BOOST_TEST(focusedId(0) == Page::ID_Language);
        back();
        BOOST_TEST_REQUIRE(desktopAs<dskHome>() != nullptr);
        BOOST_TEST(focusedId(0) == dskHome::ID_Options);
    });
}

BOOST_FIXTURE_TEST_CASE(AdvancedAndClassicRoutesRetainLegacyAccess, OptionsFixture)
{
    run([&] {
        enter();
        choose(Page::ID_Advanced);
        BOOST_TEST_REQUIRE(desktopAs<dskOptions>() != nullptr);
        back();
        BOOST_TEST_REQUIRE(desktopAs<dskHome>() != nullptr);
        choose(dskHome::ID_Options);
        choose(Page::ID_Classic);
        BOOST_TEST_REQUIRE(desktopAs<dskMainMenu>() != nullptr);
        BOOST_TEST((frontend::GetMenuStyle() == frontend::MenuStyle::Classic));
    });
}

BOOST_FIXTURE_TEST_CASE(QueuedBackDoesNotSkipTheOptionsOverview, OptionsFixture)
{
    run([&] {
        enter();
        choose(Page::ID_Audio);
        tap(pad, PadButton::B);
        tap(pad, PadButton::B);
        frame();
        frame();
        BOOST_TEST_REQUIRE(desktopAs<Page>() != nullptr);
        BOOST_TEST((desktopAs<Page>()->GetSection() == Page::Section::Overview));
        BOOST_TEST(focusedId(0) == Page::ID_Audio);
    });
}

BOOST_FIXTURE_TEST_CASE(ExceptionCleanupRestoresSettingsBeforeFixtureDestruction, OptionsFixture)
{
    bool reachedProbe = false;
    BOOST_CHECK_THROW(run([&] {
                          enter();
                          choose(Page::ID_Audio);
                          focus(Page::ID_MusicVolume);
                          press(pad, PadButton::DpadLeft);
                          BOOST_TEST_REQUIRE(desktopAs<Page>() != nullptr);
                          reachedProbe = true;
                          throwProbe();
                      }),
                      std::runtime_error);
    BOOST_TEST(reachedProbe);
    BOOST_TEST(cleaned);
    BOOST_TEST(SETTINGS.sound.musicVolume == savedSound.musicVolume);
    BOOST_TEST(SETTINGS.video.guiScale == savedVideo.guiScale);
    BOOST_TEST(SETTINGS.language.language == savedLanguage);
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
