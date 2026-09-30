// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GlobalVars.h"
#include "Loader.h"
#include "MenuPadFixture.h"
#include "controls/ctrlButton.h"
#include "desktops/dskCredits.h"
#include "desktops/dskMainMenu.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "ingameWindows/iwLobbyConnect.h"
#include "ingameWindows/iwMsgbox.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/test/unit_test.hpp>
#include <memory>

namespace {
constexpr PadDeviceId pad = 98;

struct CreditsReturnFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder userData;
    rttr::test::ConfigOverride userDataOverride{"USERDATA", userData};
    rttr::test::ConfigOverride gameOverride{"GAME", userData};
    bool& running = GLOBALVARS.notdone;
    const bool wasRunning = running;

    CreditsReturnFixture() { running = true; }
    ~CreditsReturnFixture() override { running = wasRunning; }

    void enter()
    {
        padInput().Reset();
        WINDOWMANAGER.Switch(std::make_unique<dskCredits>());
        frame();
        pickUp(pad);
        BOOST_TEST_REQUIRE(desktopAs<dskCredits>() != nullptr);
        BOOST_TEST_REQUIRE((router().GetSlot(pad) == 0u));
        BOOST_TEST_REQUIRE(focused(0) == desktop()->GetCtrl<ctrlButton>(0));
    }

    void expectMain()
    {
        BOOST_TEST_REQUIRE(desktopAs<dskMainMenu>() != nullptr);
        BOOST_TEST(running);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(video.padEvents_.empty());
        frame();
        BOOST_TEST_REQUIRE(desktopAs<dskMainMenu>() != nullptr);
        BOOST_TEST(running);
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(MenuPadCreditsReturnTests)

BOOST_FIXTURE_TEST_CASE(BReturnsAfterEnteringCreditsFromMainMenu, CreditsReturnFixture)
{
    WINDOWMANAGER.Switch(std::make_unique<dskMainMenu>());
    frame();
    pickUp(pad);
    const ctrlButton* credits = nullptr;
    for(const auto* button : desktop()->GetCtrls<ctrlButton>())
    {
        if(button->GetPos().y == 370)
            credits = button;
    }
    BOOST_TEST_REQUIRE(credits != nullptr);
    for(unsigned i = 0; i < 10 && focused(0) != credits; ++i)
        press(pad, PadButton::RightShoulder);
    BOOST_TEST_REQUIRE(focused(0) == credits);
    press(pad, PadButton::A);
    BOOST_TEST_REQUIRE(desktopAs<dskCredits>() != nullptr);
    press(pad, PadButton::B);
    expectMain();
}

BOOST_FIXTURE_TEST_CASE(BClosesRegularWindowBeforeReturningFromCredits, CreditsReturnFixture)
{
    enter();
    WINDOWMANAGER.Show(std::make_unique<iwLobbyConnect>());
    frame();
    BOOST_TEST_REQUIRE(dynamic_cast<iwLobbyConnect*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
    press(pad, PadButton::B);
    BOOST_TEST_REQUIRE(desktopAs<dskCredits>() != nullptr);
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    press(pad, PadButton::B);
    expectMain();
}

BOOST_FIXTURE_TEST_CASE(BDoesNotBypassRequiredAcknowledgement, CreditsReturnFixture)
{
    enter();
    WINDOWMANAGER.Show(std::make_unique<iwMsgbox>("Credits test", "Required acknowledgement", nullptr, MsgboxButton::Ok,
                                                  MsgboxIcon::ExclamationRed));
    frame();
    const auto* confirmation = WINDOWMANAGER.GetTopMostWindow();
    BOOST_TEST_REQUIRE(confirmation != nullptr);
    press(pad, PadButton::B);
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == confirmation);
    BOOST_TEST_REQUIRE(desktopAs<dskCredits>() != nullptr);
    press(pad, PadButton::A);
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    BOOST_TEST_REQUIRE(desktopAs<dskCredits>() != nullptr);
    press(pad, PadButton::B);
    expectMain();
}

BOOST_FIXTURE_TEST_CASE(StartAndNavigationKeepCreditsVisible, CreditsReturnFixture)
{
    enter();
    for(const auto button : {PadButton::Start, PadButton::DpadDown, PadButton::LeftShoulder, PadButton::RightShoulder})
    {
        press(pad, button);
        BOOST_TEST_REQUIRE(desktopAs<dskCredits>() != nullptr);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    }
}

BOOST_FIXTURE_TEST_CASE(MissingGraphicsAndWorldDataKeepTextCreditsBrowsable, CreditsReturnFixture)
{
    for(const bool missingWorldData : {false, true})
    {
        std::unique_ptr<rttr::test::ConfigOverride> worldDataOverride;
        if(missingWorldData)
            worldDataOverride = std::make_unique<rttr::test::ConfigOverride>("RTTR", userData);
        enter();
        // Traverse beyond the last page in both directions using the existing physical inputs.
        for(unsigned i = 0; i < 40; ++i)
        {
            WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Space));
            frame();
            BOOST_TEST_REQUIRE(desktopAs<dskCredits>() != nullptr);
            BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        }
        MouseCoords mc(Position(50, 300));
        for(unsigned i = 0; i < 40; ++i)
        {
            WINDOWMANAGER.Msg_RightUp(mc);
            frame();
            BOOST_TEST_REQUIRE(desktopAs<dskCredits>() != nullptr);
        }
        mc.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mc);
        mc.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mc);
        frame();
        BOOST_TEST_REQUIRE(desktopAs<dskCredits>() != nullptr);
        press(pad, PadButton::B);
        expectMain();
    }
}

BOOST_FIXTURE_TEST_CASE(ExistingConfirmAndMouseBackStillWork, CreditsReturnFixture)
{
    enter();
    press(pad, PadButton::A);
    expectMain();
    enter();
    MouseCoords mc(Position(400, 560));
    mc.ldown = true;
    WINDOWMANAGER.Msg_LeftDown(mc);
    mc.ldown = false;
    WINDOWMANAGER.Msg_LeftUp(mc);
    frame();
    expectMain();
}

BOOST_FIXTURE_TEST_CASE(BackBurstCannotQuitTheMainMenu, CreditsReturnFixture)
{
    enter();
    tap(pad, PadButton::B);
    tap(pad, PadButton::B);
    frame();
    expectMain();
    press(pad, PadButton::B);
    expectMain();
    const ctrlButton* quit = nullptr;
    for(const auto* button : desktop()->GetCtrls<ctrlButton>())
    {
        if(button->GetPos().y == 410)
            quit = button;
    }
    BOOST_TEST_REQUIRE(quit != nullptr);
    for(unsigned i = 0; i < 10 && focused(0) != quit; ++i)
        press(pad, PadButton::RightShoulder);
    BOOST_TEST_REQUIRE(focused(0) == quit);
    press(pad, PadButton::B);
    expectMain();
}

BOOST_AUTO_TEST_SUITE_END()
