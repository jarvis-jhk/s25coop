// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "BuildingRegister.h"
#include "GamePlayer.h"
#include "PadFixture.h"
#include "RttrForeachPt.h"
#include "Settings.h"
#include "WindowManager.h"
#include "buildings/nobBaseWarehouse.h"
#include "driver/MouseCoords.h"
#include "factories/BuildingFactory.h"
#include "helpers/EnumRange.h"
#include "ingameWindows/iwBuilding.h"
#include "ingameWindows/iwPadSystemMenu.h"
#include "ingameWindows/iwPostWindow.h"
#include "ingameWindows/iwStatistics.h"
#include "world/GameWorld.h"
#include "gameData/MapConsts.h"
#include "gameData/const_gui_ids.h"
#include <boost/test/unit_test.hpp>
#include <algorithm>

using namespace rttr::test;
using Tab = ControllerPanelNavigation::Tab;

namespace {

struct PanelFixture : PadViewFixture<2>
{
    const bool saved = SETTINGS.interface.controllerPanel;
    GameWorld& world() { return worldFixture.world; }

    PanelFixture()
    {
        SETTINGS.interface.controllerPanel = true;
        for(unsigned i = 0; i < 2; ++i)
        {
            pads.pickUp(10 + i);
            step(16);
            BOOST_TEST_REQUIRE(view(i).HasPadCursor());
        }
    }
    // NOLINTNEXTLINE(bugprone-exception-escape)
    ~PanelFixture()
    {
        while(auto* window = WINDOWMANAGER.GetTopMostWindow())
            WINDOWMANAGER.CloseNow(window);
        SETTINGS.interface.controllerPanel = saved;
    }
    void press(const unsigned slot, const PadButton button) { PadViewFixture::press(10 + slot, button); }
    const panel::Row* row(const unsigned slot, const unsigned id)
    {
        static std::vector<panel::Row> rows;
        rows = dsk->PanelRows(view(slot));
        const auto it = std::find_if(rows.begin(), rows.end(), [id](const panel::Row& r) { return r.id == id; });
        return it == rows.end() ? nullptr : &*it;
    }
    void goToTab(const unsigned slot, const Tab tab)
    {
        for(unsigned i = 0; i < 5 && view(slot).GetPanel().GetTab() != tab; ++i)
            press(slot, PadButton::RightShoulder);
        BOOST_TEST_REQUIRE((view(slot).GetPanel().GetTab() == tab));
    }
    nobBaseWarehouse& hq(const unsigned player)
    {
        auto* result = world().GetSpecObj<nobBaseWarehouse>(world().GetPlayer(player).GetHQPos());
        BOOST_REQUIRE(result != nullptr);
        return *result;
    }
    MapPoint freeCastleSpot(const unsigned player)
    {
        RTTR_FOREACH_PT(MapPoint, world().GetSize())
        {
            bool clear = world().GetNode(pt).obj == nullptr;
            for(const auto direction : helpers::EnumRange<Direction>{})
                clear = clear && world().GetNode(world().GetNeighbour(pt, direction)).obj == nullptr;
            if(clear && world().GetBQ(pt, player) == BuildingQuality::Castle)
                return pt;
        }
        return MapPoint::Invalid(); // LCOV_EXCL_LINE
    }
};

} // namespace

BOOST_AUTO_TEST_SUITE(ControllerPanelShell)

BOOST_FIXTURE_TEST_CASE(WithoutTheOptionBackStillOpensTheMenuRing, PanelFixture)
{
    SETTINGS.interface.controllerPanel = false;
    press(0, PadButton::Back);
    BOOST_TEST(!view(0).GetPanel().IsOpen());
    BOOST_TEST(view(0).GetRing().IsOpen());
    BOOST_TEST(WINDOWMANAGER.FindNonModalWindow(CGI_PADMENU, 0) != nullptr);
}

BOOST_FIXTURE_TEST_CASE(BackOpensOnlyThisSeatsPanelAndShouldersCycleTabs, PanelFixture)
{
    press(0, PadButton::Back);
    BOOST_TEST_REQUIRE(view(0).GetPanel().IsOpen());
    BOOST_TEST(!view(1).GetPanel().IsOpen());
    BOOST_TEST(!view(0).GetRing().IsOpen());
    BOOST_TEST(WINDOWMANAGER.FindNonModalWindow(CGI_PADMENU, 0) == nullptr);
    BOOST_TEST((view(0).GetPanel().GetTab() == Tab::Buildings));

    // RB five times comes round, LB goes back; the neighbour's panel never moves.
    press(1, PadButton::Back);
    BOOST_TEST_REQUIRE(view(1).GetPanel().IsOpen());
    for(const Tab expected : {Tab::Stock, Tab::Statistics, Tab::Post, Tab::Settings, Tab::Buildings})
    {
        press(0, PadButton::RightShoulder);
        BOOST_TEST((view(0).GetPanel().GetTab() == expected));
    }
    press(0, PadButton::LeftShoulder);
    BOOST_TEST((view(0).GetPanel().GetTab() == Tab::Settings));
    BOOST_TEST((view(1).GetPanel().GetTab() == Tab::Buildings));

    // A world action would undo road steps (LB) or open the action window (RB): neither happened.
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);

    press(0, PadButton::B);
    BOOST_TEST(!view(0).GetPanel().IsOpen());
    BOOST_TEST(view(1).GetPanel().IsOpen());
    press(1, PadButton::Back);
    BOOST_TEST(!view(1).GetPanel().IsOpen());
    // The tab is remembered for this game.
    press(0, PadButton::Back);
    BOOST_TEST((view(0).GetPanel().GetTab() == Tab::Settings));
}

BOOST_FIXTURE_TEST_CASE(StockShowsTheActualInventoryOfThisSeatsPlayerLive, PanelFixture)
{
    press(0, PadButton::Back);
    goToTab(0, Tab::Stock);
    const auto wood = rttr::enum_cast(GoodType::Wood);
    const auto* woodRow = row(0, wood);
    BOOST_REQUIRE(woodRow != nullptr);
    BOOST_TEST(woodRow->value == std::to_string(world().GetPlayer(0).GetInventory()[GoodType::Wood]));
    const auto helpers = panel::JobRowBase + rttr::enum_cast(Job::Helper);
    BOOST_REQUIRE(row(0, helpers) != nullptr);
    BOOST_TEST(row(0, helpers)->value == std::to_string(world().GetPlayer(0).GetInventory()[Job::Helper]));

    // Live: wood arriving in the HQ shows without reopening; the other player's stock is separate.
    const unsigned before = world().GetPlayer(0).GetInventory()[GoodType::Wood];
    GoodsAndPeopleCounts added;
    added[GoodType::Wood] = 37;
    hq(0).AddToInventory(added, true);
    BOOST_TEST(row(0, wood)->value == std::to_string(before + 37));
    press(1, PadButton::Back);
    goToTab(1, Tab::Stock);
    BOOST_TEST(row(1, wood)->value == std::to_string(world().GetPlayer(1).GetInventory()[GoodType::Wood]));
    BOOST_TEST(world().GetPlayer(1).GetInventory()[GoodType::Wood] != before + 37);

    // D-pad moves the selection; the right stick does not pan the map behind the panel.
    press(0, PadButton::DpadDown);
    press(0, PadButton::DpadDown);
    const auto& page = view(0).GetPanel().GetPage(Tab::Stock);
    BOOST_REQUIRE(page.selectedId.has_value());
    BOOST_TEST(*page.selectedId == rttr::enum_cast(GoodType::Stones));
    const Position cameraBefore = gwv(0).GetOffset();
    pads.axis(10, PadAxis::RightX, 1.f);
    for(unsigned i = 0; i < 10; ++i)
        step(16);
    releaseSticks(10);
    BOOST_TEST((gwv(0).GetOffset() == cameraBefore));
    // Many rows: scrolling far down keeps the selection visible.
    for(unsigned i = 0; i < 6; ++i)
        press(0, PadButton::DpadRight);
    const auto& scrolled = view(0).GetPanel().GetPage(Tab::Stock);
    BOOST_TEST(scrolled.firstVisibleRow > 0u);
    BOOST_TEST(*scrolled.selectedId == scrolled.rows.back());
    // The real draw path runs with both panels open (one scrolled), without touching the selection.
    dsk->paintForReal = true;
    dsk->Msg_PaintAfter();
    dsk->paintForReal = false;
    BOOST_TEST(*view(0).GetPanel().GetPage(Tab::Stock).selectedId == scrolled.rows.back());
}

BOOST_FIXTURE_TEST_CASE(BuildingsCountAndGoToTheFirstBuilding, PanelFixture)
{
    const MapPoint pos = freeCastleSpot(0);
    BOOST_REQUIRE(pos.isValid());
    BOOST_REQUIRE(BuildingFactory::CreateBuilding(world(), BuildingType::Woodcutter, pos, 0, Nation::Romans));
    press(0, PadButton::Back);
    BOOST_TEST_REQUIRE((view(0).GetPanel().GetTab() == Tab::Buildings));
    const auto woodcutter = rttr::enum_cast(BuildingType::Woodcutter);
    BOOST_REQUIRE(row(0, woodcutter) != nullptr);
    BOOST_TEST(row(0, woodcutter)->value.rfind("1  ", 0) == 0u); // "1  <productivity>%"
    BOOST_TEST(row(1, woodcutter)->value == "0");

    // An empty category is inert: the panel stays and nothing opens.
    const auto quarry = rttr::enum_cast(BuildingType::Quarry);
    for(unsigned i = 0; i < 40 && view(0).GetPanel().GetPage(Tab::Buildings).selectedId != quarry; ++i)
        press(0, PadButton::DpadDown);
    BOOST_TEST_REQUIRE(*view(0).GetPanel().GetPage(Tab::Buildings).selectedId == quarry);
    press(0, PadButton::A);
    BOOST_TEST(view(0).GetPanel().IsOpen());
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);

    for(unsigned i = 0; i < 2; ++i)
        press(0, PadButton::DpadUp);
    BOOST_TEST_REQUIRE(*view(0).GetPanel().GetPage(Tab::Buildings).selectedId == woodcutter);
    press(0, PadButton::A);
    BOOST_TEST(!view(0).GetPanel().IsOpen());
    auto* wnd = dynamic_cast<iwBuilding*>(WINDOWMANAGER.GetTopMostWindow(0));
    BOOST_TEST_REQUIRE(wnd != nullptr);
    BOOST_TEST(view(0).GetFocus().GetRoot() == wnd);
    // The camera of this seat went there and the pointer sits on it.
    const auto& gameView = gwv(0);
    const Position observed = gameView.ViewPosToMap(gameView.GetPos() + gameView.GetSize() / 2u) + gameView.GetOffset();
    const Position expected = world().GetNodePos(pos);
    const Position mapPixels(world().GetWidth() * TR_W, world().GetHeight() * TR_H);
    BOOST_TEST((observed.x - expected.x) % mapPixels.x == 0);
    BOOST_TEST((observed.y - expected.y) % mapPixels.y == 0);
    BOOST_TEST((view(0).GetPadCursor() == view(0).GetViewCenter()));
}

BOOST_FIXTURE_TEST_CASE(OtherTabsOpenTheirClassicWindowForThisSeat, PanelFixture)
{
    press(0, PadButton::Back);
    goToTab(0, Tab::Statistics);
    press(0, PadButton::A);
    BOOST_TEST(!view(0).GetPanel().IsOpen());
    auto* stats = dynamic_cast<iwStatistics*>(WINDOWMANAGER.GetTopMostWindow(0));
    BOOST_TEST_REQUIRE(stats != nullptr);
    BOOST_TEST(view(0).GetFocus().GetRoot() == stats);
    // A second time with the window still open (focus left): A enters it instead of closing it.
    press(0, PadButton::B);
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow(0) == stats);
    press(0, PadButton::Back);
    goToTab(0, Tab::Statistics);
    press(0, PadButton::A);
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow(0) == stats);
    BOOST_TEST(view(0).GetFocus().GetRoot() == stats);
    press(0, PadButton::B);
    press(0, PadButton::B);

    press(1, PadButton::Back);
    goToTab(1, Tab::Post);
    press(1, PadButton::A);
    auto* post = dynamic_cast<iwPostWindow*>(WINDOWMANAGER.GetTopMostWindow(1));
    BOOST_TEST_REQUIRE(post != nullptr);
    BOOST_TEST(view(1).GetFocus().GetRoot() == post);
    while(auto* window = WINDOWMANAGER.GetTopMostWindow())
        WINDOWMANAGER.CloseNow(window);
    step(16);

    // Menu keeps the whole ring reachable: map display, saving, leaving.
    press(0, PadButton::Back);
    goToTab(0, Tab::Settings);
    press(0, PadButton::A);
    BOOST_TEST(!view(0).GetPanel().IsOpen());
    BOOST_TEST(view(0).GetRing().IsOpen());
    BOOST_TEST(WINDOWMANAGER.FindNonModalWindow(CGI_PADMENU, 0) != nullptr);
    // Back closes the ring again rather than opening the panel on top of it.
    press(0, PadButton::Back);
    BOOST_TEST(!view(0).GetRing().IsOpen());
    BOOST_TEST(!view(0).GetPanel().IsOpen());
}

BOOST_FIXTURE_TEST_CASE(DisconnectAndMouseClickCloseThePanel, PanelFixture)
{
    press(0, PadButton::Back);
    press(0, PadButton::RightShoulder);
    BOOST_TEST_REQUIRE(view(0).GetPanel().IsOpen());
    pads.disconnect(10);
    step(16);
    BOOST_TEST(!view(0).GetPanel().IsOpen());
    BOOST_TEST((view(0).GetPanel().GetTab() == Tab::Stock));

    press(1, PadButton::Back);
    BOOST_TEST_REQUIRE(view(1).GetPanel().IsOpen());
    const Rect rect = dsk->PanelLayout(view(1)).panel;
    // The panel stays inside its own view.
    const Rect viewRect(gwv(1).GetPos(), gwv(1).GetSize());
    BOOST_TEST(rect.left >= viewRect.left);
    BOOST_TEST(rect.right <= viewRect.right);
    BOOST_TEST(rect.bottom <= viewRect.bottom);
    MouseCoords click(rect.getOrigin() + Position(rect.getSize()) / 2);
    click.ldown = true;
    BOOST_TEST(dsk->Msg_LeftDown(click));
    BOOST_TEST(!view(1).GetPanel().IsOpen());
}

BOOST_FIXTURE_TEST_CASE(NoPanelDuringARoadOrInFrontOfAModal, PanelFixture)
{
    WINDOWMANAGER.Show(std::make_unique<IngameWindow>(CGI_HELP, DrawPoint(20, 20), Extent(200, 120), "Modal", nullptr,
                                                      /*modal*/ true, CloseBehavior::Custom));
    step(16);
    BOOST_TEST(!dsk->CanOpenPanel(view(0)));
    press(0, PadButton::Back);
    BOOST_TEST(!view(0).GetPanel().IsOpen());
}

BOOST_FIXTURE_TEST_CASE(AModalAppearingClosesThePanelSoNothingActsBehindIt, PanelFixture)
{
    press(0, PadButton::Back);
    BOOST_TEST_REQUIRE(view(0).GetPanel().IsOpen());
    WINDOWMANAGER.Show(std::make_unique<IngameWindow>(CGI_HELP, DrawPoint(20, 20), Extent(200, 120), "Modal", nullptr,
                                                      /*modal*/ true, CloseBehavior::Custom));
    step(16);
    press(0, PadButton::DpadDown);
    BOOST_TEST(!view(0).GetPanel().IsOpen());
    // An ownerless modal is in front of every seat: no panel opens there either.
    press(1, PadButton::Back);
    BOOST_TEST(!view(1).GetPanel().IsOpen());
}

BOOST_AUTO_TEST_SUITE_END()
