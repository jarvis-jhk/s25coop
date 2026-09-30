// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Loader.h"
#include "MenuPadFixture.h"
#include "Settings.h"
#include "controls/ctrlButton.h"
#include "desktops/dskIntro.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "ingameWindows/IngameWindow.h"
#include "ingameWindows/iwMsgbox.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/nowide/cstdlib.hpp>
#include <boost/test/unit_test.hpp>
#include <memory>

namespace {
constexpr PadDeviceId pad = 95;

struct DestinationDesktop : Desktop
{
    unsigned clicks = 0;
    unsigned commands = 0;

    explicit DestinationDesktop(const bool withButton = true) : Desktop(nullptr)
    {
        if(withButton)
            AddTextButton(1, DrawPoint(20, 20), Extent(120, 22), TextureColor::Green1, "Continue", NormalFont);
    }
    bool WantsPadInput() const override { return true; }
    void Msg_ButtonClick(unsigned) override { ++clicks; }
    bool Msg_PadCommand(unsigned, PadButton) override
    {
        ++commands;
        return true;
    }
};

struct IntroPadFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder game;
    rttr::test::ConfigOverride gameOverride{"GAME", game};
    const bool oldMusic = SETTINGS.sound.musicEnabled;
    const bool oldEffects = SETTINGS.sound.effectsEnabled;
    unsigned transitions = 0;

    IntroPadFixture()
    {
        SETTINGS.sound.musicEnabled = false;
        SETTINGS.sound.effectsEnabled = false;
    }
    // Test teardown: a throw here is a failed test, not something to recover from
    ~IntroPadFixture() override // NOLINT(bugprone-exception-escape)
    {
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        frame();
        SETTINGS.sound.musicEnabled = oldMusic;
        SETTINGS.sound.effectsEnabled = oldEffects;
    }

    void enter(const std::string& file = "INTRO.SMK")
    {
        padInput().Reset();
        video.padEvents_.clear();
        transitions = 0;
        WINDOWMANAGER.Switch(std::make_unique<dskIntro>(file, [this] {
            ++transitions;
            return std::make_unique<DestinationDesktop>();
        }));
        frame();
        BOOST_TEST_REQUIRE(desktopAs<dskIntro>() != nullptr);
        pickUp(pad);
    }

    DestinationDesktop& destination()
    {
        auto* result = desktopAs<DestinationDesktop>();
        BOOST_TEST_REQUIRE(result != nullptr);
        BOOST_TEST(transitions == 1u);
        BOOST_TEST(result->clicks == 0u);
        BOOST_TEST(result->commands == 0u);
        BOOST_TEST(video.padEvents_.empty());
        return *result;
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(MenuPadIntroTests)

BOOST_FIXTURE_TEST_CASE(MissingVideoCanBeLeftWithAOrBOrStart, IntroPadFixture)
{
    for(const auto button : {PadButton::A, PadButton::B, PadButton::Start})
    {
        enter();
        BOOST_TEST_REQUIRE(desktopAs<dskIntro>() != nullptr); // Pickup cannot skip.
        BOOST_TEST_REQUIRE(router().GetSlot(pad) == 0u);
        BOOST_TEST_REQUIRE(focused(0) != nullptr); // The existing Back button handles A.
        press(pad, button);
        destination();
    }
}

BOOST_FIXTURE_TEST_CASE(SkipBurstTransitionsOnceAndDoesNotClickTheNextDesktop, IntroPadFixture)
{
    enter();
    tap(pad, PadButton::B);
    tap(pad, PadButton::A);
    tap(pad, PadButton::Start);
    frame();
    auto& next = destination();
    frame();
    BOOST_TEST(next.clicks == 0u);
    press(pad, PadButton::A);
    BOOST_TEST(next.clicks == 1u); // A fresh press works normally on the destination.
    BOOST_TEST(transitions == 1u);
}

BOOST_FIXTURE_TEST_CASE(BClosesRegularWindowsBeforeSkipping, IntroPadFixture)
{
    enter();
    WINDOWMANAGER.Show(std::make_unique<IngameWindow>(CGI_HELP, DrawPoint(10, 10), Extent(200, 100), "Overlay", nullptr,
                                                      false, CloseBehavior::Regular));
    frame();
    press(pad, PadButton::B);
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    BOOST_TEST_REQUIRE(desktopAs<dskIntro>() != nullptr);
    BOOST_TEST(transitions == 0u);
    press(pad, PadButton::B);
    destination();
}

BOOST_FIXTURE_TEST_CASE(SkipDoesNotBypassRequiredDialogConfirmation, IntroPadFixture)
{
    enter();
    auto& box = WINDOWMANAGER.Show(std::make_unique<iwMsgbox>("Error", "Confirm this first", nullptr, MsgboxButton::Ok,
                                                              MsgboxIcon::ExclamationRed, 1));
    frame();
    press(pad, PadButton::B);
    press(pad, PadButton::Start);
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == &box);
    BOOST_TEST_REQUIRE(desktopAs<dskIntro>() != nullptr);
    BOOST_TEST(transitions == 0u);
    press(pad, PadButton::A);
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    BOOST_TEST_REQUIRE(desktopAs<dskIntro>() != nullptr);
    press(pad, PadButton::Start);
    destination();
}

BOOST_FIXTURE_TEST_CASE(MouseAndKeyboardKeepTheirExistingExitRoutes, IntroPadFixture)
{
    for(unsigned route = 0; route < 3; ++route)
    {
        enter();
        if(route == 0)
            WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Space));
        else if(route == 1)
            WINDOWMANAGER.Msg_RightUp(MouseCoords(Position(10, 10)));
        else
        {
            MouseCoords mc(Position(400, 560));
            mc.ldown = true;
            WINDOWMANAGER.Msg_LeftDown(mc);
            mc.ldown = false;
            WINDOWMANAGER.Msg_LeftUp(mc);
        }
        frame();
        // These routes also work when the intro has not accepted its queued pad pickup yet.
        BOOST_TEST_REQUIRE(desktopAs<DestinationDesktop>() != nullptr);
        BOOST_TEST(transitions == 1u);
    }
}

BOOST_FIXTURE_TEST_CASE(AFallsBackOnlyWithoutAFocusedControlOrWindow, IntroPadFixture)
{
    for(const bool withButton : {false, true})
    {
        padInput().Reset();
        WINDOWMANAGER.Switch(std::make_unique<DestinationDesktop>(withButton));
        frame();
        pickUp(pad);
        auto& next = *desktopAs<DestinationDesktop>();
        press(pad, PadButton::A);
        BOOST_TEST(next.clicks == (withButton ? 1u : 0u));
        BOOST_TEST(next.commands == (withButton ? 0u : 1u));
        WINDOWMANAGER.Show(std::make_unique<IngameWindow>(CGI_HELP, DrawPoint(10, 10), Extent(200, 100), "Overlay",
                                                          nullptr, false, CloseBehavior::Regular));
        frame();
        const auto before = next.commands;
        press(pad, PadButton::A);
        BOOST_TEST(next.commands == before); // Even a window without controls owns its input.
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() != nullptr);
    }
}

// Original files stay outside the repository. The routing/fallback tests above always run in CI.
BOOST_FIXTURE_TEST_CASE(OriginalMovieCanBeSkippedWithoutAnOnscreenControl, IntroPadFixture)
{
    const char* original = boost::nowide::getenv("RTTR_COOP_S2_DIR");
    if(!original)
    {
        BOOST_TEST_MESSAGE("RTTR_COOP_S2_DIR not set; original movie path not tested");
        return;
    }
    // LCOV_EXCL_START
    rttr::test::ConfigOverride originalGame{"GAME", original};
    BOOST_TEST_REQUIRE(dskIntro::isAvailable());
    for(const auto button : {PadButton::A, PadButton::B, PadButton::Start})
    {
        enter();
        BOOST_TEST_REQUIRE(desktop()->GetCtrls<Window>().empty()); // Successfully opened movie, not fallback.
        BOOST_TEST_REQUIRE(focused(0) == nullptr);
        press(pad, button);
        destination();
    }
    // LCOV_EXCL_STOP
}

BOOST_AUTO_TEST_SUITE_END()
