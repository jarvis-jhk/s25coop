// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MenuPadFixture.h"
#include "RttrConfig.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlMultiline.h"
#include "controls/ctrlScrollBar.h"
#include "desktops/dskMainMenu.h"
#include "driver/MouseCoords.h"
#include "files.h"
#include "ingameWindows/iwChangelog.h"
#include "ingameWindows/iwHelp.h"
#include "ingameWindows/iwMsgbox.h"
#include "ingameWindows/iwTextfile.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <memory>
#include <string>

namespace {
constexpr PadDeviceId pad = 101;
enum class TextWindow
{
    Readme,
    Help,
    Changelog
};

std::string makeText(const unsigned count)
{
    std::string text;
    for(unsigned i = 0; i < count; ++i)
        text += "Controller text line " + std::to_string(i) + "\n";
    return text;
}

struct TextWindowPadFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder data;
    rttr::test::ConfigOverride rttrOverride{"RTTR", data / "rttr"};
    rttr::test::ConfigOverride userDataOverride{"USERDATA", data / "user"};
    std::string& seen = SETTINGS.global.coopChangelogSeen;
    std::string savedSeen = seen;
    IngameWindow* window = nullptr;
    ctrlMultiline* text = nullptr;
    ctrlScrollBar* scroll = nullptr;

    TextWindowPadFixture()
    {
        boost::filesystem::create_directories(RTTRCONFIG.ExpandPath(s25::folders::texte));
        boost::filesystem::create_directories(data / "user");
        // The generated changelog belongs to this test, not to the update-on-start feature.
        seen = "999.0.0";
    }
    ~TextWindowPadFixture() override { seen.swap(savedSeen); }

    void write(const std::string& filename, const std::string& content)
    {
        boost::nowide::ofstream file(RTTRCONFIG.ExpandPath(s25::folders::texte) / filename);
        file << content;
        BOOST_TEST_REQUIRE(static_cast<bool>(file));
    }

    void enter(const TextWindow kind, const unsigned lines = 90)
    {
        const auto content = makeText(lines);
        write("readme.txt", content);
        std::string changelog = "## 0.0.1\n\n";
        for(unsigned i = 0; i < lines; ++i)
            changelog += "- Controller entry " + std::to_string(i) + "\n";
        write("CHANGELOG.md", changelog);
        padInput().Reset();
        WINDOWMANAGER.Switch(std::make_unique<dskMainMenu>());
        frame();
        pickUp(pad);
        if(kind == TextWindow::Help)
            WINDOWMANAGER.Show(std::make_unique<iwHelp>(content));
        else
        {
            const auto buttons = desktop()->GetCtrls<ctrlButton>();
            const auto target = std::find_if(buttons.begin(), buttons.end(), [kind](const auto* button) {
                return button->GetPos().y == (kind == TextWindow::Readme ? 310 : 340);
            });
            BOOST_TEST_REQUIRE((target != buttons.end()));
            for(unsigned i = 0; i < 20 && focused(0) != *target; ++i)
                press(pad, PadButton::RightShoulder);
            BOOST_TEST_REQUIRE(focused(0) == *target);
            press(pad, PadButton::A);
        }
        frame();
        window = WINDOWMANAGER.GetTopMostWindow();
        BOOST_TEST_REQUIRE(window != nullptr);
        if(kind == TextWindow::Readme)
            BOOST_TEST_REQUIRE(dynamic_cast<iwTextfile*>(window) != nullptr);
        else if(kind == TextWindow::Help)
            BOOST_TEST_REQUIRE(dynamic_cast<iwHelp*>(window) != nullptr);
        else
            BOOST_TEST_REQUIRE(dynamic_cast<iwChangelog*>(window) != nullptr);
        const auto multiline = window->GetCtrls<ctrlMultiline>();
        BOOST_TEST_REQUIRE(multiline.size() == 1u);
        text = multiline.front();
        scroll = text->GetCtrl<ctrlScrollBar>(0);
        BOOST_TEST_REQUIRE(scroll != nullptr);
        if(scroll->IsVisible())
            BOOST_TEST_REQUIRE(padInput().GetFocus(0).GetRoot() == window);
        else
            BOOST_TEST_REQUIRE(padInput().GetFocus(0).GetRoot() == nullptr);
        BOOST_TEST_REQUIRE(desktopAs<dskMainMenu>() != nullptr);
    }

    unsigned maximum() const
    {
        const auto range = scroll->GetValueRange();
        BOOST_TEST_REQUIRE(range.has_value());
        return range->max;
    }

    void expectOpen() const
    {
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        BOOST_TEST_REQUIRE(desktopAs<dskMainMenu>() != nullptr);
        BOOST_TEST_REQUIRE(focused(0) == scroll);
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
};
} // namespace

BOOST_AUTO_TEST_SUITE(MenuPadTextWindowTests)

BOOST_FIXTURE_TEST_CASE(LongReadmeHelpAndChangelogScrollWithThePad, TextWindowPadFixture)
{
    for(const auto kind : {TextWindow::Readme, TextWindow::Help, TextWindow::Changelog})
    {
        enter(kind);
        BOOST_TEST_REQUIRE(scroll->IsVisible());
        BOOST_TEST_REQUIRE(maximum() > 5u);
        BOOST_TEST_REQUIRE(scroll->GetScrollPos() == 0u);
        expectOpen();
        pressN(pad, PadButton::DpadDown, 5);
        BOOST_TEST(scroll->GetScrollPos() == 5u);
        pressN(pad, PadButton::DpadUp, 2);
        BOOST_TEST(scroll->GetScrollPos() == 3u);
        expectOpen();
        press(pad, PadButton::B);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(desktopAs<dskMainMenu>() != nullptr);
    }
}

BOOST_FIXTURE_TEST_CASE(ScrollingAtEitherBoundaryKeepsTheWindowAndFocus, TextWindowPadFixture)
{
    for(const auto kind : {TextWindow::Readme, TextWindow::Help, TextWindow::Changelog})
    {
        enter(kind);
        const auto end = maximum();
        BOOST_TEST_REQUIRE(end > 0u);
        press(pad, PadButton::DpadUp);
        BOOST_TEST(scroll->GetScrollPos() == 0u);
        pressN(pad, PadButton::DpadDown, end + 3);
        BOOST_TEST(scroll->GetScrollPos() == end);
        expectOpen();
        press(pad, PadButton::A); // A on a scrollbar cannot activate the underlying menu.
        press(pad, PadButton::Start);
        BOOST_TEST(scroll->GetScrollPos() == end);
        expectOpen();
        pressN(pad, PadButton::DpadUp, end + 3);
        BOOST_TEST(scroll->GetScrollPos() == 0u);
        expectOpen();
    }
}

BOOST_FIXTURE_TEST_CASE(MouseWheelAndArrowButtonsShareTheSameScrollPosition, TextWindowPadFixture)
{
    for(const auto kind : {TextWindow::Readme, TextWindow::Help, TextWindow::Changelog})
    {
        enter(kind);
        MouseCoords inside(text->GetDrawPos() + DrawPoint(10, 10));
        WINDOWMANAGER.Msg_WheelDown(inside);
        frame();
        BOOST_TEST(scroll->GetScrollPos() == 3u);
        press(pad, PadButton::DpadDown);
        BOOST_TEST(scroll->GetScrollPos() == 4u);
        WINDOWMANAGER.Msg_WheelUp(inside);
        frame();
        BOOST_TEST(scroll->GetScrollPos() == 1u);
        click(scroll->GetCtrl<ctrlButton>(1)->GetDrawPos() + DrawPoint(5, 5));
        BOOST_TEST(scroll->GetScrollPos() == 2u);
        click(scroll->GetCtrl<ctrlButton>(0)->GetDrawPos() + DrawPoint(5, 5));
        BOOST_TEST(scroll->GetScrollPos() == 1u);
        expectOpen();
    }
}

BOOST_FIXTURE_TEST_CASE(ShortTextHasNoScrollFocusButBCanStillCloseIt, TextWindowPadFixture)
{
    for(const auto kind : {TextWindow::Readme, TextWindow::Help, TextWindow::Changelog})
    {
        enter(kind, 1);
        BOOST_TEST_REQUIRE(!scroll->IsVisible());
        BOOST_TEST(focused(0) == nullptr);
        for(const auto button : {PadButton::DpadDown, PadButton::DpadUp, PadButton::A, PadButton::Start})
        {
            press(pad, button);
            BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == window);
            BOOST_TEST(scroll->GetScrollPos() == 0u);
        }
        press(pad, PadButton::B);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(desktopAs<dskMainMenu>() != nullptr);
    }
}

BOOST_FIXTURE_TEST_CASE(RequiredConfirmationOwnsInputBeforeTheTextWindow, TextWindowPadFixture)
{
    enter(TextWindow::Readme);
    WINDOWMANAGER.Show(
      std::make_unique<iwMsgbox>("Confirm", "Acknowledge", nullptr, MsgboxButton::Ok, MsgboxIcon::ExclamationGreen));
    frame();
    const auto* confirmation = WINDOWMANAGER.GetTopMostWindow();
    BOOST_TEST_REQUIRE(confirmation != window);
    press(pad, PadButton::DpadDown);
    BOOST_TEST(scroll->GetScrollPos() == 0u);
    press(pad, PadButton::B);
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == confirmation);
    press(pad, PadButton::A);
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
    frame();
    press(pad, PadButton::DpadDown);
    BOOST_TEST(scroll->GetScrollPos() == 1u);
    expectOpen();
}

BOOST_FIXTURE_TEST_CASE(ClearRefillAndResizeKeepScrollingWithinTheCurrentText, TextWindowPadFixture)
{
    enter(TextWindow::Changelog);
    pressN(pad, PadButton::DpadDown, 10);
    BOOST_TEST_REQUIRE(scroll->GetScrollPos() == 10u);
    // Simulate a content/size update, not a pad selection or a UI action.
    text->Clear();
    frame();
    BOOST_TEST(!scroll->IsVisible());
    BOOST_TEST(scroll->GetScrollPos() == 0u);
    BOOST_TEST(focused(0) == nullptr);
    text->AddString(makeText(100), COLOR_WHITE, false);
    frame();
    pressN(pad, PadButton::DpadDown, 2);
    BOOST_TEST(scroll->GetScrollPos() > 0u);
    BOOST_TEST(scroll->GetScrollPos() <= maximum());
    const auto original = text->GetSize();
    text->Resize(Extent(original.x / 2, original.y / 2));
    frame();
    pressN(pad, PadButton::DpadDown, maximum() + 2);
    BOOST_TEST(scroll->GetScrollPos() == maximum());
    const unsigned oldBottom = scroll->GetScrollPos();
    text->Resize(original);
    frame();
    BOOST_TEST_REQUIRE(maximum() < oldBottom);
    BOOST_TEST(scroll->GetScrollPos() == std::min(oldBottom, maximum()));
    pressN(pad, PadButton::DpadUp, maximum() + 2);
    BOOST_TEST(scroll->GetScrollPos() == 0u);
    expectOpen();
}

BOOST_AUTO_TEST_SUITE_END()
