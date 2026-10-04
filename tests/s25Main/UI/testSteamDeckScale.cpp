// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Loader.h"
#include "PointOutput.h"
#include "Settings.h"
#include "SteamDeckUi.h"
#include "WindowManager.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlGroup.h"
#include "controls/ctrlOptionGroup.h"
#include "controls/ctrlText.h"
#include "desktops/Desktop.h"
#include "desktops/dskOptions.h"
#include "driver/MouseCoords.h"
#include "drivers/VideoDriverWrapper.h"
#include "mockupDrivers/MockupVideoDriver.h"
#include "uiHelper/uiHelpers.hpp"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/test/unit_test.hpp>
#include <memory>
#include <stdexcept>

namespace {
template<class F>
void withScale(F body)
{
    const auto sound = SETTINGS.sound;
    const auto addons = SETTINGS.addons;
    uiHelper::Fixture gui;
    rttr::test::TmpFolder data;
    rttr::test::ConfigOverride userData("USERDATA", data);
    const auto video = SETTINGS.video;
    const auto size = VIDEODRIVER.GetWindowSize();
    const auto mode = VIDEODRIVER.GetDisplayMode();
    const auto scale = VIDEODRIVER.getGuiScale().percent();
    const auto reference = VIDEODRIVER.getUiReferenceHeight();
    const auto mouse = VIDEODRIVER.GetMousePos();
    const auto restore = [&] {
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        WINDOWMANAGER.Draw();
        VIDEODRIVER.setUiReferenceHeight(reference);
        VIDEODRIVER.setGuiScalePercent(scale);
        VIDEODRIVER.ResizeScreen(size, mode);
        uiHelper::GetVideoDriver()->SetMousePos(mouse);
        SETTINGS.video = video;
        SETTINGS.sound = sound;
        SETTINGS.addons = addons;
        BOOST_TEST((SETTINGS.addons.configuration == addons.configuration));
        BOOST_TEST(SETTINGS.video.steamDeckUi == video.steamDeckUi);
        BOOST_TEST(SETTINGS.video.guiScale == video.guiScale);
        BOOST_TEST(VIDEODRIVER.getUiReferenceHeight() == reference);
        BOOST_TEST(VIDEODRIVER.getGuiScale().percent() == scale);
    };
    try
    {
        body();
    } catch(...)
    {
        restore();
        throw;
    }
    restore();
}

void useScreen(unsigned width, unsigned height, bool tvMode = false, unsigned scale = 0)
{
    SETTINGS.video.steamDeckUi = true;
    SETTINGS.video.tvMode = tvMode;
    SETTINGS.video.guiScale = scale;
    VIDEODRIVER.ResizeScreen(VideoMode(width, height), DisplayMode::Windowed);
    VIDEODRIVER.setUiReferenceHeight(deck::UiReferenceHeight(tvMode, true));
    VIDEODRIVER.setGuiScalePercent(scale);
}

void click(const ctrlButton& button)
{
    const auto rect = button.GetDrawRect();
    MouseCoords mc(rect.getOrigin() + Position(rect.getSize()) / 2);
    WINDOWMANAGER.Msg_LeftDown(mc);
    WINDOWMANAGER.Msg_LeftUp(mc);
    WINDOWMANAGER.Draw();
}

ctrlOptionGroup& tvButtons()
{
    auto* options = dynamic_cast<dskOptions*>(WINDOWMANAGER.GetCurrentDesktop());
    BOOST_TEST_REQUIRE(options);
    for(auto* group : options->GetCtrls<ctrlGroup>())
        for(const auto* text : group->GetCtrls<ctrlText>())
            if(text->GetText() == _("TV mode:"))
                for(auto* buttons : group->GetCtrls<ctrlOptionGroup>())
                    if(buttons->GetButton(1)->GetDrawRect().top <= text->GetDrawPos().y
                       && text->GetDrawPos().y < buttons->GetButton(1)->GetDrawRect().bottom)
                        return *buttons;
    BOOST_FAIL("TV controls absent");             // LCOV_EXCL_LINE
    throw std::logic_error("TV controls absent"); // LCOV_EXCL_LINE
}

[[noreturn]] void throwProbe()
{
    throw std::runtime_error("scale cleanup probe");
}
} // namespace

BOOST_AUTO_TEST_SUITE(SteamDeckScale)

BOOST_AUTO_TEST_CASE(HandheldAutoIs125PercentAndResizesWithinBothUiBounds)
{
    withScale([] {
        useScreen(1280, 800);
        BOOST_TEST(VIDEODRIVER.getGuiScale().percent() == 125u);
        BOOST_TEST(VIDEODRIVER.GetRenderSize() == Extent(1024, 640));
        for(const auto& size :
            {Extent(800, 600), Extent(1280, 720), Extent(1920, 1080), Extent(3840, 2160), Extent(1080, 1920)})
        {
            VIDEODRIVER.ResizeScreen(VideoMode(size.x, size.y), DisplayMode::Windowed);
            BOOST_TEST(VIDEODRIVER.GetRenderSize().x >= 800u);
            BOOST_TEST(VIDEODRIVER.GetRenderSize().y >= 600u);
            BOOST_TEST(VIDEODRIVER.getGuiScale().percent() == VIDEODRIVER.getGuiScaleRange().recommendedPercent);
        }
        useScreen(1280, 800, false, 110);
        BOOST_TEST(VIDEODRIVER.getGuiScale().percent() == 110u);
        useScreen(3840, 2160, true);
        BOOST_TEST(VIDEODRIVER.getGuiScale().percent() == 200u);
        useScreen(3840, 2160, true, 150);
        BOOST_TEST(VIDEODRIVER.getGuiScale().percent() == 150u);
    });
}

BOOST_AUTO_TEST_CASE(PhysicalTvToggleRestoresDeckProfileAndRespectsChosenScale)
{
    withScale([] {
        useScreen(1280, 800);
        LOADER.LoadDummyLanguageFiles();
        WINDOWMANAGER.Switch(std::make_unique<dskOptions>());
        WINDOWMANAGER.Draw();
        auto* options = dynamic_cast<dskOptions*>(WINDOWMANAGER.GetCurrentDesktop());
        BOOST_TEST_REQUIRE(options);
        const auto tabs = options->GetCtrls<ctrlOptionGroup>();
        BOOST_TEST_REQUIRE(tabs.size() == 1u);
        const auto tabButtons = tabs.front()->GetCtrls<ctrlButton>();
        BOOST_TEST_REQUIRE(tabButtons.size() == 3u);
        click(*tabButtons[1]);
        click(*tvButtons().GetButton(1));
        BOOST_TEST_REQUIRE(SETTINGS.video.tvMode);
        BOOST_TEST(VIDEODRIVER.getUiReferenceHeight() == 1080u);
        BOOST_TEST(VIDEODRIVER.getGuiScale().percent() == 100u);
        click(*tvButtons().GetButton(0));
        BOOST_TEST(!SETTINGS.video.tvMode);
        BOOST_TEST(VIDEODRIVER.getUiReferenceHeight() == 640u);
        BOOST_TEST(VIDEODRIVER.getGuiScale().percent() == 125u);
        click(*tvButtons().GetButton(1));
        SETTINGS.video.guiScale = 110;
        VIDEODRIVER.setGuiScalePercent(110);
        click(*tvButtons().GetButton(0));
        BOOST_TEST(VIDEODRIVER.getUiReferenceHeight() == 640u);
        BOOST_TEST(SETTINGS.video.guiScale == 110u);
        BOOST_TEST(VIDEODRIVER.getGuiScale().percent() == 110u);
    });
}

BOOST_AUTO_TEST_CASE(ExceptionalCleanupDestroysOptionsBeforeRestoringScale)
{
    bool reached = false;
    BOOST_CHECK_THROW(withScale([&] {
                          useScreen(1280, 800);
                          LOADER.LoadDummyLanguageFiles();
                          WINDOWMANAGER.Switch(std::make_unique<dskOptions>());
                          WINDOWMANAGER.Draw();
                          BOOST_TEST_REQUIRE(dynamic_cast<dskOptions*>(WINDOWMANAGER.GetCurrentDesktop()));
                          reached = true;
                          throwProbe();
                      }),
                      std::runtime_error);
    BOOST_TEST(reached);
}

BOOST_AUTO_TEST_SUITE_END()
