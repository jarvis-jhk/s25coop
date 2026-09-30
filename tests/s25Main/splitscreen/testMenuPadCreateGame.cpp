// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MenuPadFixture.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlEdit.h"
#include "controls/ctrlOptionGroup.h"
#include "controls/ctrlText.h"
#include "desktops/dskDirectIP.h"
#include "desktops/dskLAN.h"
#include "desktops/dskMultiPlayer.h"
#include "desktops/dskSelectMap.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "ingameWindows/iwDirectIPCreate.h"
#include "ingameWindows/iwMsgbox.h"
#include "network/GameClient.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr PadDeviceId pad = 99;

struct CreateGamePadFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder userData;
    rttr::test::ConfigOverride userDataOverride{"USERDATA", userData};
    ProxyType& proxy = SETTINGS.proxy.type;
    const ProxyType savedProxy = proxy;

    CreateGamePadFixture() { proxy = ProxyType::None; }
    ~CreateGamePadFixture() override { proxy = savedProxy; }

    void focusUntil(const Window* target)
    {
        for(unsigned i = 0; i < 30 && focused(0) != target; ++i)
            press(pad, PadButton::RightShoulder);
        BOOST_TEST_REQUIRE(focused(0) == target);
    }

    iwDirectIPCreate* enter(const bool lan)
    {
        padInput().Reset();
        if(lan)
            WINDOWMANAGER.Switch(std::make_unique<dskLAN>());
        else
            WINDOWMANAGER.Switch(std::make_unique<dskDirectIP>());
        frame();
        pickUp(pad);
        const auto buttons = desktop()->GetCtrls<ctrlButton>();
        BOOST_TEST_REQUIRE(!buttons.empty());
        const auto createButton = std::find_if(buttons.begin(), buttons.end(), [lan](const auto* button) {
            return button->GetPos().y == (lan ? 250 : 180);
        });
        BOOST_TEST_REQUIRE((createButton != buttons.end()));
        focusUntil(*createButton);
        press(pad, PadButton::A);
        auto* create = dynamic_cast<iwDirectIPCreate*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(create != nullptr);
        // The new window becomes the focus root on the next frame.
        frame();
        BOOST_TEST_REQUIRE((router().GetSlot(pad) == 0u));
        return create;
    }

    void expectCancelled(const Desktop* parent)
    {
        BOOST_TEST_REQUIRE(desktop() == parent);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        // Same parent, no map-selection transition and a stopped client: the hosting flow was not entered.
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
        BOOST_TEST(video.padEvents_.empty());
        frame();
        BOOST_TEST(desktop() == parent);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    }

    void click(const DrawPoint pos)
    {
        MouseCoords mc(pos);
        mc.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mc);
        mc.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mc);
        frame();
    }

    void typeInto(ctrlEdit& edit, const std::string& text)
    {
        click(edit.GetDrawPos() + DrawPoint(5, 5));
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::End));
        const auto length = edit.GetText().size();
        for(size_t i = 0; i < length; ++i)
            WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Backspace));
        for(const unsigned char c : text)
            WINDOWMANAGER.Msg_KeyDown(KeyEvent(c));
        BOOST_TEST_REQUIRE(edit.GetText() == text);
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(MenuPadCreateGameTests)

BOOST_FIXTURE_TEST_CASE(BCancelsFromEveryFocusedFormControl, CreateGamePadFixture)
{
    for(const bool lan : {false, true})
    {
        auto* create = enter(lan);
        std::vector<unsigned> ids;
        for(const auto* button : create->GetCtrls<ctrlButton>())
            ids.push_back(button->GetID());
        BOOST_TEST_REQUIRE(ids.size() == 2u);
        for(const auto id : ids)
        {
            create = enter(lan);
            const auto* parent = desktop();
            focusUntil(create->GetCtrl<Window>(id));
            press(pad, PadButton::B);
            expectCancelled(parent);
        }
        for(const unsigned id : {1u, 3u, 5u})
        {
            create = enter(lan);
            const auto* parent = desktop();
            auto* edit = create->GetCtrl<ctrlEdit>(id);
            click(edit->GetDrawPos() + DrawPoint(5, 5));
            BOOST_TEST_REQUIRE(edit->HasFocus()); // Text fields deliberately belong to the keyboard, not pad focus.
            press(pad, PadButton::B);
            expectCancelled(parent);
        }
        for(const unsigned id : {0u, 1u})
        {
            create = enter(lan);
            const auto* parent = desktop();
            focusUntil(create->GetCtrl<ctrlOptionGroup>(12)->GetCtrl<ctrlButton>(id));
            press(pad, PadButton::B);
            expectCancelled(parent);
        }
    }
}

BOOST_FIXTURE_TEST_CASE(BCancelsAfterValidationErrors, CreateGamePadFixture)
{
    for(const bool lan : {false, true})
    {
        for(const bool invalidPort : {false, true})
        {
            auto* create = enter(lan);
            const auto* parent = desktop();
            typeInto(*create->GetCtrl<ctrlEdit>(1), invalidPort ? "Controller game" : "");
            typeInto(*create->GetCtrl<ctrlEdit>(3), invalidPort ? "65536" : "12345");
            focusUntil(create->GetCtrl<ctrlButton>(7));
            press(pad, PadButton::A);
            BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == create);
            BOOST_TEST_REQUIRE(!create->GetCtrl<ctrlText>(6)->GetText().empty());
            BOOST_TEST(!create->GetCtrl<ctrlButton>(7)->GetEnabled());
            BOOST_TEST(create->GetCtrl<ctrlEdit>(invalidPort ? 3 : 1)->HasFocus());
            BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
            press(pad, PadButton::B);
            expectCancelled(parent);
        }
    }
}

BOOST_FIXTURE_TEST_CASE(ValidStartStillEntersMapSelection, CreateGamePadFixture)
{
    for(const bool lan : {false, true})
    {
        auto* create = enter(lan);
        typeInto(*create->GetCtrl<ctrlEdit>(1), "Controller game");
        typeInto(*create->GetCtrl<ctrlEdit>(3), "12345");
        focusUntil(create->GetCtrl<ctrlButton>(7));
        press(pad, PadButton::A);
        BOOST_TEST_REQUIRE(desktopAs<dskSelectMap>() != nullptr);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
        press(pad, PadButton::B);
        if(lan)
            BOOST_TEST(desktopAs<dskLAN>() != nullptr);
        else
            BOOST_TEST(desktopAs<dskDirectIP>() != nullptr);
    }
}

BOOST_FIXTURE_TEST_CASE(VisibleBackTitleCloseAndKeyboardCancelTheSameForm, CreateGamePadFixture)
{
    for(const bool lan : {false, true})
    {
        for(const unsigned method : {0u, 1u, 2u, 3u, 4u})
        {
            auto* create = enter(lan);
            const auto* parent = desktop();
            if(method == 0u)
            {
                focusUntil(create->GetCtrl<ctrlButton>(8));
                press(pad, PadButton::A);
            } else if(method == 1u)
                click(create->GetCtrl<ctrlButton>(8)->GetDrawPos() + DrawPoint(5, 5));
            else if(method == 2u)
            {
                WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
                frame();
            } else if(method == 3u)
                click(create->GetPos() + DrawPoint(5, 5)); // Title close button.
            else
            {
                KeyEvent close('w');
                close.alt = true;
                WINDOWMANAGER.Msg_KeyDown(close);
                frame();
            }
            expectCancelled(parent);
        }
    }
}

BOOST_FIXTURE_TEST_CASE(OverlayKeepsItsOwnCancellationPolicy, CreateGamePadFixture)
{
    for(const bool confirmation : {false, true})
    {
        auto* create = enter(false);
        const auto* parent = desktop();
        if(confirmation)
            WINDOWMANAGER.Show(std::make_unique<iwMsgbox>("Confirm", "Acknowledge first", nullptr, MsgboxButton::Ok,
                                                          MsgboxIcon::ExclamationGreen, 0));
        else
            WINDOWMANAGER.Show(std::make_unique<IngameWindow>(CGI_HELP, IngameWindow::posCenter, Extent(100, 80),
                                                              "Overlay", nullptr, true));
        frame();
        const auto* overlay = WINDOWMANAGER.GetTopMostWindow();
        BOOST_TEST_REQUIRE(overlay != create);
        press(pad, PadButton::B);
        if(confirmation)
        {
            BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == overlay);
            press(pad, PadButton::A);
        }
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == create);
        BOOST_TEST(desktop() == parent);
        press(pad, PadButton::B);
        expectCancelled(parent);
    }
}

BOOST_FIXTURE_TEST_CASE(StartNavigationAndRightClickDoNotCancel, CreateGamePadFixture)
{
    auto* create = enter(false);
    typeInto(*create->GetCtrl<ctrlEdit>(5), "private test password");
    for(const auto button : {PadButton::Start, PadButton::RightShoulder, PadButton::LeftShoulder, PadButton::DpadDown})
    {
        press(pad, button);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == create);
        BOOST_TEST(create->GetCtrl<ctrlEdit>(5)->GetText() == "private test password");
    }
    MouseCoords mc(create->GetDrawPos() + DrawPoint(10, 170));
    mc.rdown = true;
    WINDOWMANAGER.Msg_RightDown(mc);
    mc.rdown = false;
    WINDOWMANAGER.Msg_RightUp(mc);
    frame();
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == create);
    const auto* parent = desktop();
    press(pad, PadButton::B);
    expectCancelled(parent);
}

BOOST_FIXTURE_TEST_CASE(BackBurstCancelsOnlyTheForm, CreateGamePadFixture)
{
    for(const bool lan : {false, true})
    {
        enter(lan);
        const auto* parent = desktop();
        tap(pad, PadButton::B);
        tap(pad, PadButton::B);
        frame();
        expectCancelled(parent);
        press(pad, PadButton::B);
        BOOST_TEST(desktopAs<dskMultiPlayer>() != nullptr);
    }
}

BOOST_AUTO_TEST_SUITE_END()
