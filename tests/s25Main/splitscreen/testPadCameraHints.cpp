// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AsyncChecksum.h"
#include "PadFixture.h"
#include "PadGameFixture.h"
#include "PointOutput.h"
#include "Settings.h"
#include "WindowManager.h"
#include "controls/ctrlComboBox.h"
#include "ingameWindows/iwPadSystemMenu.h"
#include "input/KeyGlyph.h"
#include "ogl/glFont.h"
#include "gameData/GuiConsts.h"
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>

using namespace rttr::test;

namespace {
bool namesAxis(const PlayerView& view, const brief::KeyInput input, const brief::KeyAction action)
{
    const auto& keys = view.GetBrief().keys;
    return std::count(keys.begin(), keys.end(), brief::KeyHint{PadButton{}, action, input}) == 1;
}

void expectCamera(const PlayerView& view)
{
    BOOST_TEST(namesAxis(view, brief::KeyInput::RightStickAxis, brief::KeyAction::PanCamera));
    const float target = view.GetView().GetCurrentTargetZoomFactor();
    BOOST_TEST(namesAxis(view, brief::KeyInput::LeftTriggerAxis, brief::KeyAction::ZoomOut)
               == (target > ZOOM_FACTORS.front()));
    BOOST_TEST(namesAxis(view, brief::KeyInput::RightTriggerAxis, brief::KeyAction::ZoomIn)
               == (target < ZOOM_FACTORS.back()));
}

struct PendingWindow : IngameWindow
{
    explicit PendingWindow(const bool modal)
        : IngameWindow(CGI_HELP, DrawPoint(20, 20), Extent(200, 120), "Pending", nullptr, modal, CloseBehavior::Custom)
    {
        auto* combo = AddComboBox(1, DrawPoint(10, 10), Extent(160, 22), TextureColor::Green1, NormalFont, 80, false);
        combo->AddItem("Original");
        combo->AddItem("Preview");
        combo->SetSelection(0);
    }
    unsigned accepted = 0;
    void Msg_ComboSelectItem(unsigned, unsigned) override { ++accepted; }
};

template<unsigned N>
struct CameraHintFixture : PadViewFixture<N>
{
    template<class F>
    void run(F&& body)
    {
        try
        {
            body();
        } catch(...)
        {
            closeWindows();
            throw;
        }
        closeWindows();
    }
    static void closeWindows()
    {
        while(auto* window = WINDOWMANAGER.GetTopMostWindow())
            WINDOWMANAGER.CloseNow(window);
    }
    void take(const unsigned slot)
    {
        for(unsigned i = 0; i <= slot; ++i)
        {
            this->pads.pickUp(10 + i);
            this->step(16);
        }
        BOOST_REQUIRE(this->view(slot).HasPadCursor());
    }
    void exerciseCamera(const unsigned slot)
    {
        expectCamera(this->view(slot));
        const auto cursor = this->view(slot).GetPadCursor();
        const auto before = this->gwv(slot).GetOffset();
        this->pads.axis(10 + slot, PadAxis::RightX, 1.f);
        this->step(100);
        this->pads.axis(10 + slot, PadAxis::RightX, 0.f);
        this->step(16);
        BOOST_TEST((this->gwv(slot).GetOffset() != before));
        BOOST_TEST((this->view(slot).GetPadCursor() == cursor));
        const float zoom = this->gwv(slot).GetCurrentTargetZoomFactor();
        BOOST_REQUIRE(namesAxis(this->view(slot), brief::KeyInput::RightTriggerAxis, brief::KeyAction::ZoomIn));
        this->pads.axis(10 + slot, PadAxis::TriggerRight, 1.f);
        this->step(100);
        this->pads.axis(10 + slot, PadAxis::TriggerRight, 0.f);
        this->step(16);
        BOOST_TEST(this->gwv(slot).GetCurrentTargetZoomFactor() > zoom);
        expectCamera(this->view(slot));
    }
};

struct LiveCameraHintFixture : PadGameFixture
{
    template<class F>
    void run(F&& body)
    {
        try
        {
            setUpTwoLocalPlayers();
            body();
        } catch(...)
        {
            cleanup();
            throw;
        }
        cleanup();
    }
    void cleanup()
    {
        CameraHintFixture<1>::closeWindows();
        if(dsk)
            tearDownDesktop();
    }
};

[[noreturn]] void throwCleanupProbe()
{
    throw std::runtime_error("camera hint cleanup probe");
}
} // namespace

BOOST_AUTO_TEST_SUITE(PadCameraHints)

BOOST_FIXTURE_TEST_CASE(AxesHaveDistinctNeutralBadgesAndCompleteNarrowTextFallback, CameraHintFixture<1>)
{
    const std::vector<brief::KeyHint> hints{
      {PadButton::A, brief::KeyAction::PanCamera, brief::KeyInput::RightStickAxis},
      {PadButton::B, brief::KeyAction::ZoomOut, brief::KeyInput::LeftTriggerAxis},
      {PadButton::X, brief::KeyAction::ZoomIn, brief::KeyInput::RightTriggerAxis}};
    const std::array<std::string, 3> labels = {_("Right stick"), "LT", "RT"};
    bool fallback = false, badges = false;
    for(const auto width : std::array<unsigned short, 4>{16, 100, 320, 1000})
    {
        const auto lines = brief::LayoutKeyGlyphs(hints, *NormalFont, width, dskGameInterface::keyLineColor);
        BOOST_REQUIRE(!lines.empty());
        std::string text;
        for(const auto& line : lines)
        {
            text += line.text;
            if(line.runs.empty())
            {
                fallback = true;
                BOOST_TEST(NormalFont->getWidth(line.text) <= width);
            }
            for(const auto& part : line.runs)
            {
                BOOST_TEST(part.x + part.width <= width);
                if(part.badgeColor)
                {
                    badges = true;
                    BOOST_TEST(part.badgeColor == brief::neutralBadgeColor);
                }
            }
        }
        const auto compact = [](std::string s) {
            s.erase(std::remove_if(s.begin(), s.end(), [](const char c) { return c == ' ' || c == '-'; }), s.end());
            return s;
        };
        BOOST_TEST(compact(text) == compact(brief::KeyLine(hints)));
    }
    BOOST_TEST(fallback);
    BOOST_TEST(badges);
    for(unsigned i = 0; i < hints.size(); ++i)
    {
        BOOST_TEST(brief::KeyInputLabel(hints[i]) == labels[i]);
        BOOST_TEST((hints[i] == brief::KeyHint{PadButton::Y, hints[i].action, hints[i].input}));
        BOOST_TEST((hints[i] != brief::KeyHint{PadButton::RightStick, hints[i].action}));
    }
    BOOST_TEST((hints[1] != brief::KeyHint{PadButton{}, hints[1].action, brief::KeyInput::RightTriggerAxis}));
}

BOOST_FIXTURE_TEST_CASE(FourSeatsAdvertiseOnlyTheirOwnAvailableZoomDirections, CameraHintFixture<4>)
{
    run([&] {
        take(3);
        for(unsigned slot = 0; slot < 4; ++slot)
        {
            const auto next = (slot + 1) % 4;
            const auto otherOffset = gwv(next).GetOffset();
            const float otherZoom = gwv(next).GetCurrentTargetZoomFactor();
            const auto otherKeys = view(next).GetBrief().keys;
            exerciseCamera(slot);
            BOOST_TEST((gwv(next).GetOffset() == otherOffset));
            BOOST_TEST(gwv(next).GetCurrentTargetZoomFactor() == otherZoom);
            BOOST_CHECK(view(next).GetBrief().keys == otherKeys);
            for(const auto axis : {PadAxis::TriggerRight, PadAxis::TriggerLeft})
            {
                pads.axis(10 + slot, axis, 1.f);
                for(unsigned i = 0; i < 10; ++i)
                    step(1000);
                const auto limit = axis == PadAxis::TriggerRight ? ZOOM_FACTORS.back() : ZOOM_FACTORS.front();
                BOOST_TEST(gwv(slot).GetCurrentTargetZoomFactor() == limit);
                expectCamera(view(slot));
                pads.axis(10 + slot, axis, 0.f);
                step(16);
            }
            // Both opposing triggers cancel, with the remaining direction still advertised.
            pads.axis(10 + slot, PadAxis::TriggerLeft, 1.f);
            pads.axis(10 + slot, PadAxis::TriggerRight, 1.f);
            step(100);
            BOOST_TEST(gwv(slot).GetCurrentTargetZoomFactor() == ZOOM_FACTORS.front());
            pads.axis(10 + slot, PadAxis::TriggerLeft, 0.f);
            pads.axis(10 + slot, PadAxis::TriggerRight, 0.f);
            step(16);
        }
    });
}

BOOST_FIXTURE_TEST_CASE(RingAndWatchingKeepCameraHintsAndTheirExistingExit, CameraHintFixture<2>)
{
    run([&] {
        take(1);
        press(11, PadButton::Back);
        BOOST_REQUIRE(view(1).GetRing().IsOpen());
        const auto* focused = view(1).GetFocus().GetFocused();
        const auto page = view(1).GetRing().GetPage();
        exerciseCamera(1);
        BOOST_TEST(view(1).GetFocus().GetFocused() == focused);
        BOOST_TEST(view(1).GetRing().GetPage() == page);
        for(unsigned i = 0; i < 7 && view(1).GetFocus().GetFocused()->GetID() != iwPadSystemMenu::ID_WATCH_ONLY; ++i)
            press(11, PadButton::DpadRight);
        BOOST_REQUIRE(view(1).GetFocus().GetFocused()->GetID() == iwPadSystemMenu::ID_WATCH_ONLY);
        press(11, PadButton::A);
        WINDOWMANAGER.Draw();
        BOOST_REQUIRE(view(1).IsWatchOnly());
        const auto cursor = view(1).GetPadCursor();
        exerciseCamera(1);
        pads.axis(11, PadAxis::LeftX, 1.f);
        step(100);
        pads.axis(11, PadAxis::LeftX, 0.f);
        step(16);
        BOOST_TEST((view(1).GetPadCursor() == cursor));
        BOOST_TEST(view(1).IsWatchOnly());
        press(11, PadButton::B);
        BOOST_TEST(!view(1).IsWatchOnly());
        expectCamera(view(1));
    });
}

BOOST_FIXTURE_TEST_CASE(FocusedDropdownAndModalKeepPendingSelectionWhileTheCameraMoves, CameraHintFixture<2>)
{
    run([&] {
        take(1);
        for(const bool modal : {false, true})
        {
            auto& window = static_cast<PendingWindow&>(WINDOWMANAGER.Show(std::make_unique<PendingWindow>(modal)));
            press(11, PadButton::Y);
            auto* combo = window.GetCtrl<ctrlComboBox>(1);
            BOOST_REQUIRE(view(1).GetFocus().GetFocused() == combo);
            press(11, PadButton::A);
            press(11, PadButton::DpadDown);
            BOOST_REQUIRE(combo->CanCancelInput());
            BOOST_TEST((combo->GetSelection() == 1u));
            exerciseCamera(1);
            BOOST_TEST(combo->CanCancelInput());
            BOOST_TEST((combo->GetSelection() == 1u));
            BOOST_TEST(window.accepted == 0u);
            BOOST_TEST(view(1).GetFocus().GetRoot() == &window);
            BOOST_TEST(!window.ShouldBeClosed());
            press(11, PadButton::B);
            BOOST_TEST((combo->GetSelection() == 0u));
            BOOST_TEST(window.accepted == 0u);
            press(11, PadButton::A);
            press(11, PadButton::DpadDown);
            press(11, PadButton::A);
            BOOST_TEST(window.accepted == 1u);
            WINDOWMANAGER.CloseNow(&window);
        }
    });
}

BOOST_FIXTURE_TEST_CASE(UnassignedDisconnectedAndMouseViewsKeepAnEmptyBrief, CameraHintFixture<1>)
{
    pads.connect(10);
    step(16);
    BOOST_TEST(view(0).GetBrief().empty());
    take(0);
    expectCamera(view(0));
    pads.disconnect(10);
    step(16);
    BOOST_TEST(view(0).GetBrief().empty());
    const auto offset = gwv(0).GetOffset();
    pads.axis(10, PadAxis::RightX, 1.f);
    step(100);
    BOOST_TEST((gwv(0).GetOffset() == offset));
    BOOST_TEST(view(0).GetBrief().empty());
}

BOOST_FIXTURE_TEST_CASE(ExceptionalCleanupClosesAnActuallyOpenRing, CameraHintFixture<1>)
{
    bool reached = false;
    BOOST_CHECK_THROW(run([&] {
                          take(0);
                          press(10, PadButton::Back);
                          BOOST_REQUIRE(view(0).GetRing().IsOpen());
                          expectCamera(view(0));
                          reached = true;
                          throwCleanupProbe();
                      }),
                      std::runtime_error);
    BOOST_TEST(reached);
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
}

BOOST_FIXTURE_TEST_CASE(LiveRoadAndSharedTribeBrowsingNeverSendCommands, LiveCameraHintFixture)
{
    run([&] {
        auto& owner = dsk->GetPlayerView(1);
        const auto spot = findRoadSpotFromHQ(owner.GetViewer());
        BOOST_REQUIRE(spot.isValid());
        aimPadAt(10, 0, world().GetPlayer(0).GetHQPos());
        aimPadAt(11, 1, spot.start);
        press(11, PadButton::A);
        aimAt(1, spot.end);
        press(11, PadButton::A);
        BOOST_REQUIRE(!owner.GetRoad().route.empty());
        const auto route = owner.GetRoad().route;
        const auto roadPoint = owner.GetRoad().point;
        const auto checksum = AsyncChecksum::create(*ci().game);
        const auto gf = GAMECLIENT.GetGFNumber();
        expectCamera(owner);
        const auto roadOffset = owner.GetView().GetOffset();
        const float roadZoom = owner.GetView().GetCurrentTargetZoomFactor();
        pads.axis(11, PadAxis::RightY, 1.f);
        pads.axis(11, PadAxis::TriggerLeft, 1.f);
        step(100);
        pads.axis(11, PadAxis::RightY, 0.f);
        pads.axis(11, PadAxis::TriggerLeft, 0.f);
        step(16);
        expectCamera(owner);
        BOOST_TEST((owner.GetView().GetOffset() != roadOffset));
        BOOST_TEST(owner.GetView().GetCurrentTargetZoomFactor() < roadZoom);
        BOOST_CHECK(owner.GetRoad().route == route);
        BOOST_TEST((owner.GetRoad().point == roadPoint));
        BOOST_TEST((owner.GetRoad().mode == RoadBuildMode::Normal));
        BOOST_TEST((AsyncChecksum::create(*ci().game) == checksum));
        BOOST_TEST(GAMECLIENT.GetGFNumber() == gf);
        for(unsigned i = 0; i <= route.size(); ++i)
            press(11, PadButton::B);
        BOOST_REQUIRE(owner.GetRoad().mode == RoadBuildMode::Disabled);
        cleanup();
        GAMECLIENT.SetAdditionalLocalPlayers({});
        GAMECLIENT.SetSharedLocalViews(1);
        dsk =
          std::make_unique<TestableGameInterface>(ci().game, GAMECLIENT.GetNWFInfo(), GAMECLIENT.GetPlayerId(), false);
        BOOST_REQUIRE(dsk->GetNumViews() == 2u);
        BOOST_TEST(dsk->GetPlayerView(0).GetPlayerId() == dsk->GetPlayerView(1).GetPlayerId());
        pads.pickUp(10);
        pads.pickUp(11);
        step(16);
        auto& shared = dsk->GetPlayerView(1);
        const auto otherOffset = dsk->GetPlayerView(0).GetView().GetOffset();
        const auto otherZoom = dsk->GetPlayerView(0).GetView().GetCurrentTargetZoomFactor();
        press(11, PadButton::Back);
        BOOST_REQUIRE(shared.GetRing().IsOpen());
        const auto* focused = shared.GetFocus().GetFocused();
        expectCamera(shared);
        const auto sharedOffset = shared.GetView().GetOffset();
        const float sharedZoom = shared.GetView().GetCurrentTargetZoomFactor();
        pads.axis(11, PadAxis::RightX, 1.f);
        pads.axis(11, PadAxis::TriggerRight, 1.f);
        step(100);
        pads.axis(11, PadAxis::RightX, 0.f);
        pads.axis(11, PadAxis::TriggerRight, 0.f);
        step(16);
        expectCamera(shared);
        BOOST_TEST((shared.GetView().GetOffset() != sharedOffset));
        BOOST_TEST(shared.GetView().GetCurrentTargetZoomFactor() > sharedZoom);
        BOOST_TEST(shared.GetFocus().GetFocused() == focused);
        BOOST_TEST((dsk->GetPlayerView(0).GetView().GetOffset() == otherOffset));
        BOOST_TEST(dsk->GetPlayerView(0).GetView().GetCurrentTargetZoomFactor() == otherZoom);
        pumpUntilGF(GAMECLIENT.GetGFNumber() + 24u);
        cleanup();
        const auto replay = stopAndGetReplay();
        BOOST_TEST(numGCsForPlayer(replay, 0) == 0u);
        BOOST_TEST(numGCsForPlayer(replay, 1) == 0u);
    });
}

BOOST_FIXTURE_TEST_CASE(LiveExceptionalCleanupClosesTheRingBeforeDestroyingItsWorldView, LiveCameraHintFixture)
{
    bool reached = false;
    BOOST_CHECK_THROW(run([&] {
                          pads.pickUp(10);
                          pads.pickUp(11);
                          step(16);
                          press(11, PadButton::Back);
                          BOOST_REQUIRE(dsk->GetPlayerView(1).GetRing().IsOpen());
                          expectCamera(dsk->GetPlayerView(1));
                          reached = true;
                          throwCleanupProbe();
                      }),
                      std::runtime_error);
    BOOST_TEST(reached);
    BOOST_TEST(!dsk);
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
