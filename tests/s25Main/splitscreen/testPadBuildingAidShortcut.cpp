// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "PadFixture.h"
#include "PadGameFixture.h"
#include "Settings.h"
#include "WindowManager.h"
#include "controls/ctrlComboBox.h"
#include "controls/ctrlTextButton.h"
#include "driver/KeyEvent.h"
#include "ingameWindows/iwPadSystemMenu.h"
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <stdexcept>

using namespace rttr::test;

namespace {
constexpr PadButton aidButton = PadButton::LeftStick;

bool namesAid(const PlayerView& view)
{
    const auto& keys = view.GetBrief().keys;
    return std::count(keys.begin(), keys.end(), brief::KeyHint{aidButton, brief::KeyAction::ToggleConstructionAid})
           == 1;
}

struct ChoiceWindow : IngameWindow
{
    explicit ChoiceWindow(const bool modal)
        : IngameWindow(CGI_HELP, DrawPoint(20, 20), Extent(200, 120), "Choice", nullptr, modal, CloseBehavior::Custom)
    {
        auto* combo = AddComboBox(1, DrawPoint(10, 10), Extent(160, 22), TextureColor::Green1, NormalFont, 80, false);
        combo->AddItem("Original");
        combo->AddItem("Preview");
        combo->SetSelection(0);
    }
    unsigned selections = 0;
    void Msg_ComboSelectItem(unsigned, unsigned) override { ++selections; }
};

template<unsigned N>
struct ShortcutFixture : PadViewFixture<N>
{
    const decltype(SETTINGS.ingame) saved = SETTINGS.ingame;

    void cleanup()
    {
        while(auto* window = WINDOWMANAGER.GetTopMostWindow())
            WINDOWMANAGER.CloseNow(window);
        SETTINGS.ingame = saved;
    }

    template<class F>
    void run(F&& body)
    {
        try
        {
            for(unsigned i = 0; i < N; ++i)
                this->gwv(i).SetBqMode(BqMode::Off);
            body();
        } catch(...)
        {
            cleanup();
            throw;
        }
        cleanup();
        BOOST_TEST(SETTINGS.ingame.showBQ == saved.showBQ);
    }

    void take(const unsigned slot)
    {
        for(unsigned i = 0; i <= slot; ++i)
        {
            if(!this->view(i).HasPadCursor())
                this->pads.pickUp(10 + i);
            this->step(16);
        }
        BOOST_REQUIRE(this->view(slot).HasPadCursor());
    }

    void toggle(const unsigned slot)
    {
        BOOST_TEST(namesAid(this->view(slot)));
        this->press(10 + slot, aidButton);
    }
};

struct LiveShortcutFixture : PadGameFixture
{
    const decltype(SETTINGS.ingame) saved = SETTINGS.ingame;

    void cleanup()
    {
        while(auto* window = WINDOWMANAGER.GetTopMostWindow())
            WINDOWMANAGER.CloseNow(window);
        if(ci().game)
            tearDownDesktop();
        SETTINGS.ingame = saved;
    }

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
        BOOST_TEST(SETTINGS.ingame.showBQ == saved.showBQ);
    }
};

[[noreturn]] void throwCleanupProbe()
{
    throw std::runtime_error("building aid cleanup probe");
}
} // namespace

BOOST_AUTO_TEST_SUITE(PadBuildingAidShortcut)

BOOST_FIXTURE_TEST_CASE(TheSameClickTogglesOnlyItsOwnSeatAndDoesNotRepeatWhileHeld, ShortcutFixture<4>)
{
    run([&] {
        for(unsigned i = 0; i < 4; ++i)
            take(i);
        const bool preference = SETTINGS.ingame.showBQ;
        for(unsigned i = 1; i < 4; ++i)
        {
            toggle(i);
            BOOST_TEST((gwv(i).GetBqMode() == BqMode::All));
            for(unsigned j = 0; j < 4; ++j)
                BOOST_TEST(gwv(j).IsShowingBQ() == (j > 0 && j <= i));
            BOOST_TEST(SETTINGS.ingame.showBQ == preference);
        }
        pads.button(11, aidButton, true);
        step(16);
        BOOST_TEST(!gwv(1).IsShowingBQ());
        pads.button(11, aidButton, true);
        step(1000);
        BOOST_TEST(!gwv(1).IsShowingBQ());
        pads.button(11, aidButton, false);
        step(16);
        BOOST_TEST(!gwv(1).IsShowingBQ());
        toggle(1);
        BOOST_TEST((gwv(1).GetBqMode() == BqMode::All));
    });
}

BOOST_FIXTURE_TEST_CASE(TheShortcutPreservesRingSelectionAndRefreshesItsBuildAidLabel, ShortcutFixture<2>)
{
    run([&] {
        take(1);
        press(11, PadButton::Back);
        BOOST_REQUIRE(view(1).GetRing().IsOpen());
        auto* menu = dynamic_cast<iwPadSystemMenu*>(view(1).GetFocus().GetRoot());
        BOOST_REQUIRE(menu);
        const auto* focused = view(1).GetFocus().GetFocused();
        const auto page = view(1).GetRing().GetPage();
        const auto aim = view(1).GetRing().GetAimRaw();
        toggle(1);
        WINDOWMANAGER.Draw();
        BOOST_TEST((gwv(1).GetBqMode() == BqMode::All));
        BOOST_TEST(view(1).GetRing().IsOpen());
        BOOST_TEST(view(1).GetFocus().GetFocused() == focused);
        BOOST_TEST(view(1).GetRing().GetPage() == page);
        BOOST_TEST(view(1).GetRing().GetAimRaw().x == aim.x);
        BOOST_TEST(view(1).GetRing().GetAimRaw().y == aim.y);
        BOOST_TEST(menu->GetCtrl<ctrlTextButton>(iwPadSystemMenu::ID_CONSTRUCTION_AID)->GetText()
                   == _("Build aid: all"));
        toggle(1);
        WINDOWMANAGER.Draw();
        BOOST_TEST(!gwv(1).IsShowingBQ());
        BOOST_TEST(menu->GetCtrl<ctrlTextButton>(iwPadSystemMenu::ID_CONSTRUCTION_AID)->GetText()
                   == _("Build aid: off"));
    });
}

BOOST_FIXTURE_TEST_CASE(OpenDropdownAndModalConfirmationKeepTheirPendingChoice, ShortcutFixture<2>)
{
    run([&] {
        take(1);
        for(const bool modal : {false, true})
        {
            auto& window = static_cast<ChoiceWindow&>(WINDOWMANAGER.Show(std::make_unique<ChoiceWindow>(modal)));
            press(11, PadButton::Y);
            auto* combo = window.GetCtrl<ctrlComboBox>(1);
            BOOST_REQUIRE(view(1).GetFocus().GetFocused() == combo);
            press(11, PadButton::A);
            press(11, PadButton::DpadDown);
            BOOST_REQUIRE(combo->CanCancelInput());
            BOOST_TEST((combo->GetSelection() == 1u));
            const auto accepted = window.selections;
            const auto before = gwv(1).IsShowingBQ();
            toggle(1);
            BOOST_TEST(gwv(1).IsShowingBQ() == !before);
            BOOST_TEST(view(1).GetFocus().GetRoot() == &window);
            BOOST_TEST(view(1).GetFocus().GetFocused() == combo);
            BOOST_TEST(combo->CanCancelInput());
            BOOST_TEST((combo->GetSelection() == 1u));
            BOOST_TEST(window.selections == accepted);
            BOOST_TEST(!window.ShouldBeClosed());
            press(11, PadButton::B);
            BOOST_TEST((combo->GetSelection() == 0u));
            BOOST_TEST(window.selections == accepted);
            // Commit a fresh physical choice to prove the notification observer is active.
            press(11, PadButton::A);
            press(11, PadButton::DpadDown);
            press(11, PadButton::A);
            BOOST_TEST((combo->GetSelection() == 1u));
            BOOST_TEST(!combo->CanCancelInput());
            BOOST_TEST(window.selections == accepted + 1u);
            WINDOWMANAGER.CloseNow(&window);
        }
    });
}

BOOST_FIXTURE_TEST_CASE(WatchingLeavesDeliberatelyAndBuildingAidThenStaysToggleable, ShortcutFixture<2>)
{
    run([&] {
        take(1);
        // The saved cursor-only mode must not turn the requested full-map aid back off.
        gwv(1).SetBqMode(BqMode::Cursor);
        press(11, PadButton::Back);
        BOOST_REQUIRE(view(1).GetRing().IsOpen());
        for(unsigned i = 0; i < 7 && view(1).GetFocus().GetFocused()->GetID() != iwPadSystemMenu::ID_WATCH_ONLY; ++i)
            press(11, PadButton::DpadRight);
        BOOST_REQUIRE(view(1).GetFocus().GetFocused()->GetID() == iwPadSystemMenu::ID_WATCH_ONLY);
        press(11, PadButton::A);
        WINDOWMANAGER.Draw();
        BOOST_REQUIRE(view(1).IsWatchOnly());
        BOOST_REQUIRE(!gwv(1).IsShowingBQ());
        toggle(1);
        BOOST_TEST(!view(1).IsWatchOnly());
        BOOST_TEST((gwv(1).GetBqMode() == BqMode::All));
        toggle(1);
        BOOST_TEST(!gwv(1).IsShowingBQ());
        BOOST_TEST(!view(1).IsWatchOnly());
        BOOST_TEST(!gwv(0).IsShowingBQ());
    });
}

BOOST_FIXTURE_TEST_CASE(DisconnectAndMouseKeyboardKeepTheirExistingBehavior, ShortcutFixture<1>)
{
    run([&] {
        pads.connect(10);
        step(16);
        BOOST_TEST(!view(0).HasPadCursor());
        BOOST_TEST(view(0).GetBrief().empty());
        BOOST_TEST(!gwv(0).IsShowingBQ());
        take(0);
        press(10, PadButton::Start);
        BOOST_TEST(!gwv(0).IsShowingBQ());
        toggle(0);
        BOOST_TEST((gwv(0).GetBqMode() == BqMode::All));
        BOOST_TEST(SETTINGS.ingame.showBQ);
        toggle(0);
        BOOST_TEST(!SETTINGS.ingame.showBQ);
        toggle(0);
        BOOST_TEST(SETTINGS.ingame.showBQ);
        pads.disconnect(10);
        step(16);
        pads.tap(10, aidButton);
        step(16);
        BOOST_TEST((gwv(0).GetBqMode() == BqMode::All));
        BOOST_TEST(view(0).GetBrief().empty());
        BOOST_TEST(dsk->Msg_KeyDown(KeyEvent{U' '}));
        BOOST_TEST(!gwv(0).IsShowingBQ());
        dsk->ToggleConstructionAidFor(view(0));
        BOOST_TEST((gwv(0).GetBqMode() == BqMode::All));
    });
}

BOOST_FIXTURE_TEST_CASE(ExceptionalCleanupDestroysWindowsAndRestoresSettingsBeforeFixtureDestruction,
                        ShortcutFixture<2>)
{
    bool reachedProbe = false;
    BOOST_CHECK_THROW(run([&] {
                          take(1);
                          press(11, PadButton::Back);
                          BOOST_REQUIRE(view(1).GetRing().IsOpen());
                          toggle(1);
                          reachedProbe = true;
                          throwCleanupProbe();
                      }),
                      std::runtime_error);
    BOOST_TEST(reachedProbe);
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    BOOST_TEST(SETTINGS.ingame.showBQ == saved.showBQ);
}

BOOST_FIXTURE_TEST_CASE(ARealRoadPreviewAndTheRecordedWorldStayUnchanged, LiveShortcutFixture)
{
    run([&] {
        const auto* hq = world().GetSpecObj<nobBaseWarehouse>(world().GetPlayer(1).GetHQPos());
        BOOST_REQUIRE(hq);
        const auto flag = hq->GetFlagPos();
        aimPadAt(10, 0, world().GetPlayer(0).GetHQPos());
        aimPadAt(11, 1, flag);
        press(11, PadButton::A);
        auto& playerView = dsk->GetPlayerView(1);
        const auto spot = findRoadSpotFromHQ(playerView.GetViewer());
        BOOST_REQUIRE(spot.isValid());
        aimAt(1, spot.end);
        press(11, PadButton::A);
        BOOST_REQUIRE(!playerView.GetRoad().route.empty());
        BOOST_REQUIRE(playerView.GetRoad().mode == RoadBuildMode::Normal);
        const auto route = playerView.GetRoad().route;
        const auto point = playerView.GetRoad().point;
        const auto start = playerView.GetRoad().start;
        const auto selected = playerView.GetView().GetSelectedPt();
        const auto before = playerView.GetView().IsShowingBQ();
        BOOST_TEST(namesAid(playerView));
        press(11, aidButton);
        BOOST_TEST(playerView.GetView().IsShowingBQ() == !before);
        BOOST_TEST((playerView.GetRoad().mode == RoadBuildMode::Normal));
        BOOST_REQUIRE(playerView.GetRoad().route.size() == route.size());
        for(unsigned i = 0; i < route.size(); ++i)
            BOOST_TEST((playerView.GetRoad().route[i] == route[i]));
        BOOST_TEST((playerView.GetRoad().point == point));
        BOOST_TEST((playerView.GetRoad().start == start));
        BOOST_TEST((playerView.GetView().GetSelectedPt() == selected));
        BOOST_TEST(playerView.GetRejectionCount() == 0u);
        // Force the real local client/server through more than one NWF before closing the recording.
        const auto gf = GAMECLIENT.GetGFNumber();
        pumpUntil([&] { return GAMECLIENT.GetGFNumber() > gf + 24; }, "view-only shortcut NWF");
        // End the uncommitted road and leave a real ring for the desktop transition to close.
        for(unsigned i = 0; i <= route.size(); ++i)
            press(11, PadButton::B);
        BOOST_REQUIRE(playerView.GetRoad().mode == RoadBuildMode::Disabled);
        press(11, PadButton::Back);
        BOOST_REQUIRE(playerView.GetRing().IsOpen());
        BOOST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() != nullptr);
        while(auto* window = WINDOWMANAGER.GetTopMostWindow())
            WINDOWMANAGER.CloseNow(window);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        tearDownDesktop();
        // Reuse the same live world with two seats controlling the same tribe, not two players.
        GAMECLIENT.SetAdditionalLocalPlayers({});
        GAMECLIENT.SetSharedLocalViews(1);
        dsk =
          std::make_unique<TestableGameInterface>(ci().game, GAMECLIENT.GetNWFInfo(), GAMECLIENT.GetPlayerId(), false);
        BOOST_REQUIRE(dsk->GetNumViews() == 2u);
        BOOST_TEST(dsk->GetPlayerView(0).GetPlayerId() == dsk->GetPlayerView(1).GetPlayerId());
        aimPadAt(10, 0, world().GetPlayer(0).GetHQPos());
        aimPadAt(11, 1, world().GetPlayer(0).GetHQPos());
        auto& shared = dsk->GetPlayerView(1).GetView();
        const bool neighbor = dsk->GetPlayerView(0).GetView().IsShowingBQ();
        const bool sharedBefore = shared.IsShowingBQ();
        const bool preference = SETTINGS.ingame.showBQ;
        press(11, aidButton);
        BOOST_TEST(shared.IsShowingBQ() == !sharedBefore);
        BOOST_TEST(dsk->GetPlayerView(0).GetView().IsShowingBQ() == neighbor);
        BOOST_TEST(SETTINGS.ingame.showBQ == preference);
        pumpUntilGF(GAMECLIENT.GetGFNumber() + 24u);
        tearDownDesktop();
        const auto replay = stopAndGetReplay();
        BOOST_TEST(numGCsForPlayer(replay, 0) == 0u);
        BOOST_TEST(numGCsForPlayer(replay, 1) == 0u);
    });
}

BOOST_FIXTURE_TEST_CASE(ALiveExceptionWithAnOpenRingAlsoCleansUpBeforeWorldDestruction, LiveShortcutFixture)
{
    bool reachedProbe = false;
    BOOST_CHECK_THROW(run([&] {
                          aimPadAt(10, 0, world().GetPlayer(0).GetHQPos());
                          aimPadAt(11, 1, world().GetPlayer(1).GetHQPos());
                          press(11, PadButton::Back);
                          BOOST_REQUIRE(dsk->GetPlayerView(1).GetRing().IsOpen());
                          BOOST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() != nullptr);
                          press(11, aidButton);
                          reachedProbe = true;
                          throwCleanupProbe();
                      }),
                      std::runtime_error);
    BOOST_TEST(reachedProbe);
    BOOST_TEST(!dsk);
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    BOOST_TEST(SETTINGS.ingame.showBQ == saved.showBQ);
}

BOOST_AUTO_TEST_SUITE_END()
