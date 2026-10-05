// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

// Couch coop, M3c step 3 (doc/coop/SharedLocalViews.md): several views on ONE player need their
// own seat identity, and a road built from one view must not leave another view's preview in a
// state it can never build. Step 4: an economy window one view leaves open follows the settings
// another view on the same player transmits.

#include "GamePlayer.h"
#include "LeatherLoader.h"
#include "Loader.h"
#include "PadGameFixture.h"
#include "PointOutput.h"
#include "WindowManager.h"
#include "buildings/nobBaseWarehouse.h"
#include "controls/ctrlImageButton.h"
#include "controls/ctrlList.h"
#include "controls/ctrlOptionGroup.h"
#include "controls/ctrlProgress.h"
#include "controls/ctrlTab.h"
#include "desktops/PlayerView.h"
#include "helpers/EnumRange.h"
#include "ingameWindows/IngameWindow.h"
#include "ingameWindows/iwBuildOrder.h"
#include "ingameWindows/iwDistribution.h"
#include "ingameWindows/iwMilitary.h"
#include "ingameWindows/iwTools.h"
#include "ingameWindows/iwTransport.h"
#include "network/GameClient.h"
#include "pathfinding/FindPathForRoad.h"
#include "world/GameWorld.h"
#include "gameTypes/RoadBuildMode.h"
#include "rttr/test/TmpFolder.hpp"
#include "s25util/colors.h"
#include <libsiedler2/Archiv.h>
#include <libsiedler2/ArchivItem_Bitmap_Raw.h>
#include <libsiedler2/PixelBufferBGRA.h>
#include <libsiedler2/libsiedler2.h>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <type_traits>
#include <vector>

using namespace rttr::test;

namespace {

struct ButtonWnd : IngameWindow
{
    ButtonWnd() : IngameWindow(CGI_HELP, DrawPoint(0, 0), Extent(200, 120), "", nullptr, false, CloseBehavior::Regular)
    {
        AddTextButton(1, DrawPoint(10, 10), Extent(80, 20), TextureColor::Green1, "A", NormalFont);
    }
};

struct SharedViewsFixture : PadGameFixture
{
    /// Player 0 seen through two views, players 1 and 2 stay AIs (no slot taken over).
    void setUpSharedViews()
    {
        hostAndEnterLobby();
        GAMECLIENT.SetSharedLocalViews(1);
        lobby().SetPlayerState(1, PlayerState::AI, AI::Info(AI::Type::Dummy));
        lobby().SetPlayerState(2, PlayerState::AI, AI::Info(AI::Type::Dummy));
        pumpUntil(
          [] {
              const auto l = GAMECLIENT.GetGameLobby();
              return l->getPlayer(1).ps == PlayerState::AI && l->getPlayer(2).ps == PlayerState::AI;
          },
          "lobby to apply the player configuration");
        startGame();
        dsk = std::make_unique<TestableGameInterface>(ci().game, GAMECLIENT.GetNWFInfo(), GAMECLIENT.GetPlayerId(),
                                                      /*initOGL*/ false);
        BOOST_TEST_REQUIRE(dsk->GetNumViews() == 2u);
        BOOST_TEST_REQUIRE(dsk->GetPlayerView(0).GetPlayerId() == 0u);
        BOOST_TEST_REQUIRE(dsk->GetPlayerView(1).GetPlayerId() == 0u);
    }

    /// Starts a road preview in this view through the same calls the pad path uses.
    void preview(const unsigned viewIdx, const RoadSpot& spot)
    {
        PlayerView& v = dsk->GetPlayerView(viewIdx);
        dsk->StartRoadBuilding(v, spot.start, false);
        MapPoint target = spot.end;
        BOOST_TEST_REQUIRE((dsk->BuildRoadPart(v, target) == dskGameInterface::RoadPartResult::Built));
        BOOST_TEST_REQUIRE((v.GetRoad().route == spot.route), boost::test_tools::per_element());
        BOOST_TEST_REQUIRE(viewerDrawsRoad(v.GetViewer(), spot.start, spot.route));
    }

    /// Continues this view's preview by one or more edges to `target`.
    void extend(const unsigned viewIdx, const MapPoint target)
    {
        MapPoint t = target;
        BOOST_TEST_REQUIRE(
          (dsk->BuildRoadPart(dsk->GetPlayerView(viewIdx), t) == dskGameInterface::RoadPartResult::Built));
        BOOST_TEST_REQUIRE((t == target));
    }

    /// Opens an economy window exactly as dskGameInterface does for this view.
    template<class T>
    T& openFor(const unsigned viewIdx)
    {
        PlayerView& v = dsk->GetPlayerView(viewIdx);
        const dskGameInterface::ViewScope ownerScope(viewIdx);
        if constexpr(std::is_same_v<T, iwBuildOrder>)
            return WINDOWMANAGER.Show(std::make_unique<T>(v.GetViewer()));
        else
            return WINDOWMANAGER.Show(std::make_unique<T>(v.GetViewer(), dskGameInterface::gcFactoryFor(v)));
    }

    void buildFromView0(const RoadSpot& spot)
    {
        BOOST_TEST_REQUIRE(dsk->CommitRoad(dsk->GetPlayerView(0)));
        pumpUntil([&] { return worldHasRoad(world(), spot.start, spot.route); }, "road from view 0 to be built");
    }
};

/// A second road from the HQ flag whose nodes, apart from the shared start flag, stay away from
/// `other`'s nodes and their neighbours.
RoadSpot findDisjointSpot(const GameWorldViewer& viewer, const RoadSpot& other)
{
    const GameWorldBase& world = viewer.GetWorld();
    const std::vector<MapPoint> otherPts = roadPoints(world, other.start, other.route);
    const auto nearOther = [&](const MapPoint pt) {
        return std::any_of(otherPts.begin() + 1, otherPts.end(),
                           [&](const MapPoint o) { return world.CalcDistance(pt, o) < 2u; });
    };
    RoadSpot spot;
    spot.start = other.start;
    for(const MapPoint& pt : world.GetPointsInRadiusWithCenter(spot.start, 7))
    {
        if(pt == spot.start || world.GetNode(pt).obj || world.IsFlagAround(pt)
           || world.GetBQ(pt, viewer.GetPlayerId()) == BuildingQuality::Nothing)
            continue;
        std::vector<Direction> route = FindPathForRoad(viewer, spot.start, pt, false, 100);
        const auto runsNearOther = [&]() {
            const std::vector<MapPoint> pts = roadPoints(world, spot.start, route);
            return std::any_of(pts.begin() + 1, pts.end(), nearOther);
        };
        // One combined filter: which test rejects a node depends on the map, and a separate
        // continue that this map never reaches fails the coverage check.
        if(route.size() < 2u || route.size() > 5u || runsNearOther())
            continue;
        spot.end = pt;
        spot.route = std::move(route);
        return spot;
    }
    return {}; // LCOV_EXCL_LINE
}

bool contains(const std::vector<MapPoint>& pts, const MapPoint pt)
{
    return std::find(pts.begin(), pts.end(), pt) != pts.end();
}

/// A free node next to `from` that is not on `avoid`, and the direction to it.
std::pair<MapPoint, Direction> freeNeighbour(const GameWorldViewer& viewer, const MapPoint from,
                                             const std::vector<MapPoint>& avoid)
{
    const GameWorldBase& world = viewer.GetWorld();
    for(const Direction dir : helpers::EnumRange<Direction>{})
    {
        const MapPoint pt = world.GetNeighbour(from, dir);
        if(!contains(avoid, pt) && !world.GetNode(pt).obj && !world.IsFlagAround(pt)
           && world.GetBQ(pt, viewer.GetPlayerId()) != BuildingQuality::Nothing)
            return {pt, dir};
    }
    return {MapPoint::Invalid(), Direction::West}; // LCOV_EXCL_LINE
}

/// Waypoints from `spot.start` to a neighbour of `spot.end` whose way touches none of `spot`'s
/// other nodes: continued through `spot.end` it meets that road only at its END flag. The way
/// goes around through a free node first, because the direct one runs along the road.
std::vector<MapPoint> approachEndFlag(const GameWorldViewer& viewer, const RoadSpot& spot)
{
    const GameWorldBase& world = viewer.GetWorld();
    const std::vector<MapPoint> roadPts = roadPoints(world, spot.start, spot.route);
    const auto avoids = [&](const MapPoint from, const std::vector<Direction>& way,
                            const std::vector<MapPoint>& avoid) {
        const std::vector<MapPoint> pts = roadPoints(world, from, way);
        return std::none_of(pts.begin() + 1, pts.end(), [&](const MapPoint pt) { return contains(avoid, pt); });
    };
    for(const MapPoint& w : world.GetPointsInRadius(spot.start, 7))
    {
        const std::vector<Direction> first = (contains(roadPts, w) || world.GetNode(w).obj) ?
                                               std::vector<Direction>{} :
                                               FindPathForRoad(viewer, spot.start, w, false, 100);
        // Combined for the same reason as in findDisjointSpot.
        if(first.empty() || !avoids(spot.start, first, roadPts))
            continue;
        std::vector<MapPoint> taken = roadPts;
        for(const MapPoint pt : roadPoints(world, spot.start, first))
            taken.push_back(pt);
        for(const Direction dir : helpers::EnumRange<Direction>{})
        {
            const MapPoint x = world.GetNeighbour(spot.end, dir);
            if(contains(taken, x) || world.GetNode(x).obj)
                continue;
            const std::vector<Direction> second = FindPathForRoad(viewer, w, x, false, 100);
            if(!second.empty() && avoids(w, second, taken))
                return {w, x};
        }
    }
    return {}; // LCOV_EXCL_LINE
}

/// Moves a slider to a different value through the window's own change handler, as input does.
unsigned shiftBar(Window& wnd, ctrlProgress& bar, const unsigned ctrlId)
{
    const unsigned newPos = bar.GetPosition() == 0u ? 1u : bar.GetPosition() - 1u;
    bar.SetPosition(newPos);
    wnd.Msg_ProgressChange(ctrlId, newPos);
    return newPos;
}

ctrlProgress& militaryBar(iwMilitary& w)
{
    // Ids 0 and 1 are Help and Default; the first slider follows.
    return *w.GetCtrls<ctrlProgress>().front();
}

/// The distribution window needs the leather addon's pig icon, which the dummy GUI files lack.
void loadPigIcon(const rttr::test::TmpFolder& graphics)
{
    const auto path = graphics / "leather_bobs.lst";
    boost::filesystem::create_directories(path);
    const auto icon = leatheraddon::bobIndex[leatheraddon::BobType::DistributionOfPigsIcon];
    libsiedler2::Archiv sprite;
    auto bitmap = std::make_unique<libsiedler2::ArchivItem_Bitmap_Raw>();
    bitmap->create(libsiedler2::PixelBufferBGRA(1, 1));
    sprite.push(std::move(bitmap));
    BOOST_TEST_REQUIRE(libsiedler2::Write(path / (std::to_string(icon) + ".bmp"), sprite) == 0);
    BOOST_TEST_REQUIRE(LOADER.LoadFiles({path.string()}));
}

ctrlProgress& distributionBar(iwDistribution& w)
{
    return *w.GetCtrl<ctrlTab>(0)->GetGroup(0)->GetCtrl<ctrlProgress>(0);
}

const std::string& firstTransportTooltip(iwTransport& w)
{
    return w.GetCtrl<ctrlOptionGroup>(6)->GetCtrl<ctrlImageButton>(0)->GetTooltip();
}

} // namespace

BOOST_AUTO_TEST_SUITE(SharedViewsTests)

BOOST_FIXTURE_TEST_CASE(SharedViewsGetDistinctSeatColours, SharedViewsFixture)
{
    setUpSharedViews();
    PlayerView& v0 = dsk->GetPlayerView(0);
    PlayerView& v1 = dsk->GetPlayerView(1);
    BOOST_TEST(dsk->SeatColor(v0) == PLAYER_COLORS[0]);
    BOOST_TEST(dsk->SeatColor(v1) == PLAYER_COLORS[1]);
    BOOST_TEST(dsk->SeatColor(v0) != dsk->SeatColor(v1));

    // The focus ring of a window entered by the second seat carries that seat's colour.
    IngameWindow* wnd;
    {
        const dskGameInterface::ViewScope scope(1);
        wnd = &WINDOWMANAGER.Show(std::make_unique<ButtonWnd>());
    }
    BOOST_TEST_REQUIRE(dsk->EnterWindow(v1, wnd));
    BOOST_TEST_REQUIRE(wnd->GetFocusRingColor(v1.GetFocus()).has_value());
    BOOST_TEST(*wnd->GetFocusRingColor(v1.GetFocus()) == PLAYER_COLORS[1]);
    BOOST_TEST(!wnd->GetFocusRingColor(v0.GetFocus()).has_value());
    wnd->Close();
    WINDOWMANAGER.Draw();
    tearDownDesktop();
}

BOOST_FIXTURE_TEST_CASE(DistinctPlayersKeepTheirPlayerColour, PadGameFixture)
{
    setUpTwoLocalPlayers();
    for(unsigned i = 0; i < 2u; ++i)
        BOOST_TEST(dsk->SeatColor(dsk->GetPlayerView(i)) == world().GetPlayer(i).color);
    tearDownDesktop();
}

BOOST_FIXTURE_TEST_CASE(RoadFromOneViewStopsTheOtherViewsCrossingPreview, SharedViewsFixture)
{
    setUpSharedViews();
    const RoadSpot spot = findRoadSpotFromHQ(dsk->GetPlayerView(0).GetViewer(), 2, 4);
    BOOST_TEST_REQUIRE(spot.isValid());
    PlayerView& v1 = dsk->GetPlayerView(1);
    const auto beyond = freeNeighbour(v1.GetViewer(), spot.end, roadPoints(world(), spot.start, spot.route));
    BOOST_TEST_REQUIRE(beyond.first.isValid());
    // Neither viewer sees the other's preview, so both can plan through the same nodes. The
    // second one runs on past the first one's end, so part of it is preview only.
    preview(0, spot);
    preview(1, spot);
    extend(1, beyond.first);
    BOOST_TEST_REQUIRE(viewerDrawsRoad(v1.GetViewer(), spot.end, {beyond.second}));

    buildFromView0(spot);

    BOOST_TEST((v1.GetRoad().mode == RoadBuildMode::Disabled));
    BOOST_TEST(v1.GetRoad().route.empty());
    // The edge that was only ever a preview is gone; the rest is the real road, in both viewers.
    BOOST_TEST(!viewerDrawsAnyOf(v1.GetViewer(), spot.end, {beyond.second}));
    BOOST_TEST(viewerDrawsRoad(v1.GetViewer(), spot.start, spot.route));
    BOOST_TEST(viewerDrawsRoad(dsk->GetPlayerView(0).GetViewer(), spot.start, spot.route));
    BOOST_TEST(ci().numErrors == 0u);
    BOOST_TEST(ci().numAsync == 0u);
    tearDownDesktop();
}

BOOST_FIXTURE_TEST_CASE(NewEndFlagInsideAnotherPreviewStopsIt, SharedViewsFixture)
{
    setUpSharedViews();
    const RoadSpot spot = findRoadSpotFromHQ(dsk->GetPlayerView(0).GetViewer(), 2, 4);
    BOOST_TEST_REQUIRE(spot.isValid());
    PlayerView& v1 = dsk->GetPlayerView(1);
    // The second view reaches the first view's end node another way and runs on through it:
    // after the build that node is a flag in the middle of its preview.
    const std::vector<MapPoint> waypoints = approachEndFlag(v1.GetViewer(), spot);
    BOOST_TEST_REQUIRE(waypoints.size() == 2u);
    preview(0, spot);
    dsk->StartRoadBuilding(v1, spot.start, false);
    for(const MapPoint pt : waypoints)
        extend(1, pt);
    extend(1, spot.end);
    std::vector<MapPoint> taken = roadPoints(world(), spot.start, spot.route);
    for(const MapPoint pt : roadPoints(world(), spot.start, v1.GetRoad().route))
        taken.push_back(pt);
    const auto beyond = freeNeighbour(v1.GetViewer(), spot.end, taken);
    BOOST_TEST_REQUIRE(beyond.first.isValid());
    extend(1, beyond.first);
    const std::vector<Direction> previewRoute = v1.GetRoad().route;
    const std::vector<MapPoint> previewPts = roadPoints(world(), spot.start, previewRoute);
    // Precondition of this case: only the end node is shared, not the road between the flags.
    const std::vector<MapPoint> roadPts = roadPoints(world(), spot.start, spot.route);
    for(unsigned i = 1; i + 1u < roadPts.size(); ++i)
        BOOST_TEST_REQUIRE(!contains(previewPts, roadPts[i]));
    BOOST_TEST_REQUIRE(contains(previewPts, spot.end));
    BOOST_TEST_REQUIRE(previewPts.back() != spot.end);

    buildFromView0(spot);

    BOOST_TEST((v1.GetRoad().mode == RoadBuildMode::Disabled));
    BOOST_TEST(!viewerDrawsAnyOf(v1.GetViewer(), spot.start, previewRoute));
    BOOST_TEST(ci().numErrors == 0u);
    BOOST_TEST(ci().numAsync == 0u);
    tearDownDesktop();
}

BOOST_FIXTURE_TEST_CASE(RoadFromOneViewKeepsTheOtherViewsSeparatePreview, SharedViewsFixture)
{
    setUpSharedViews();
    const RoadSpot spot = findRoadSpotFromHQ(dsk->GetPlayerView(0).GetViewer(), 2, 4);
    BOOST_TEST_REQUIRE(spot.isValid());
    const RoadSpot other = findDisjointSpot(dsk->GetPlayerView(1).GetViewer(), spot);
    BOOST_TEST_REQUIRE(other.isValid());
    preview(0, spot);
    preview(1, other);

    buildFromView0(spot);

    PlayerView& v1 = dsk->GetPlayerView(1);
    BOOST_TEST_REQUIRE((v1.GetRoad().mode == RoadBuildMode::Normal));
    BOOST_TEST((v1.GetRoad().route == other.route), boost::test_tools::per_element());
    BOOST_TEST(viewerDrawsRoad(v1.GetViewer(), other.start, other.route));

    // ... and that preview can still be built.
    BOOST_TEST_REQUIRE(dsk->CommitRoad(v1));
    pumpUntil([&] { return worldHasRoad(world(), other.start, other.route); }, "road from view 1 to be built");
    BOOST_TEST(ci().numErrors == 0u);
    BOOST_TEST(ci().numAsync == 0u);
    tearDownDesktop();
}

/// Step 4: every economy window kind, open in both views on the shared player. A change sent from
/// view 0 shows up in view 1's window without reopening it.
BOOST_FIXTURE_TEST_CASE(EconomyWindowsFollowTheOtherSharedViewsTransmission, SharedViewsFixture)
{
    setUpSharedViews();
    // Ware icons (transport), building pictures (build order) and the pig icon (distribution) are
    // not part of the dummy GUI files.
    LOADER.LoadDummyMapFiles();
    LOADER.LoadDummyBuildingFiles();
    const rttr::test::TmpFolder graphics;
    loadPigIcon(graphics);

    auto& mil0 = openFor<iwMilitary>(0);
    auto& mil1 = openFor<iwMilitary>(1);
    auto& dist0 = openFor<iwDistribution>(0);
    auto& dist1 = openFor<iwDistribution>(1);
    auto& tools0 = openFor<iwTools>(0);
    auto& tools1 = openFor<iwTools>(1);
    auto& trans0 = openFor<iwTransport>(0);
    auto& trans1 = openFor<iwTransport>(1);
    auto& order0 = openFor<iwBuildOrder>(0);
    auto& order1 = openFor<iwBuildOrder>(1);
    const std::vector<IngameWindow*> shownIn0{&mil0, &dist0, &tools0, &trans0, &order0};
    const std::vector<IngameWindow*> shownIn1{&mil1, &dist1, &tools1, &trans1, &order1};

    const unsigned milPos = shiftBar(mil0, militaryBar(mil0), militaryBar(mil0).GetID());
    // Distribution sliders sit in tab groups and report through the group handler.
    auto& distBar0 = distributionBar(dist0);
    const unsigned distPos = distBar0.GetPosition() == 0u ? 1u : distBar0.GetPosition() - 1u;
    distBar0.SetPosition(distPos);
    static_cast<Window&>(dist0).Msg_Group_ProgressChange(0, 0, distPos);
    BOOST_TEST_REQUIRE(unsigned(distributionBar(dist1).GetPosition()) != distPos);
    auto& toolBar0 = *tools0.GetCtrl<ctrlProgress>(0);
    const unsigned toolPos = shiftBar(tools0, toolBar0, 0);
    // Transport: move the second good to the top.
    trans0.GetCtrl<ctrlOptionGroup>(6)->SetSelection(1);
    const std::string topGood = trans0.GetCtrl<ctrlOptionGroup>(6)->GetCtrl<ctrlImageButton>(1)->GetTooltip();
    static_cast<Window&>(trans0).Msg_ButtonClick(3);
    BOOST_TEST_REQUIRE(firstTransportTooltip(trans0) == topGood);
    // Build order: move the second building up.
    order0.GetCtrl<ctrlList>(0)->SetSelection(1u);
    const std::string topBuilding = order0.GetCtrl<ctrlList>(0)->GetItemText(1);
    static_cast<Window&>(order0).Msg_ButtonClick(2);
    BOOST_TEST_REQUIRE(order0.GetCtrl<ctrlList>(0)->GetItemText(0) == topBuilding);

    // Before transmission view 1 still shows the old values: the refresh follows the transmission,
    // not the slider.
    BOOST_TEST_REQUIRE(unsigned(militaryBar(mil1).GetPosition()) != milPos);
    BOOST_TEST_REQUIRE(firstTransportTooltip(trans1) != topGood);
    BOOST_TEST_REQUIRE(order1.GetCtrl<ctrlList>(0)->GetItemText(0) != topBuilding);

    for(IngameWindow* w : shownIn0)
        PadGameFixture::fireTransmitTimer(*w);

    BOOST_TEST(unsigned(militaryBar(mil1).GetPosition()) == milPos);
    BOOST_TEST(unsigned(distributionBar(dist1).GetPosition()) == distPos);
    BOOST_TEST(unsigned(tools1.GetCtrl<ctrlProgress>(0)->GetPosition()) == toolPos);
    BOOST_TEST(firstTransportTooltip(trans1) == topGood);
    BOOST_TEST(order1.GetCtrl<ctrlList>(0)->GetItemText(0) == topBuilding);

    // The transmission itself reached the game for player 0 (checked for military; the commands
    // are the unchanged single-view ones).
    pumpUntilGF(GAMECLIENT.GetGFNumber() + 40);
    BOOST_TEST(unsigned(world().GetPlayer(0).GetMilitarySetting(0)) == milPos);
    BOOST_TEST(ci().numAsync == 0u);

    for(IngameWindow* w : shownIn0)
        WINDOWMANAGER.CloseNow(w);
    for(IngameWindow* w : shownIn1)
        WINDOWMANAGER.CloseNow(w);
    tearDownDesktop();
}

/// A window with its own unsent change is not overwritten by the other view: that change is sent
/// later and then wins in both windows.
BOOST_FIXTURE_TEST_CASE(APendingSharedEditIsKeptAndWinsWhenSent, SharedViewsFixture)
{
    setUpSharedViews();

    auto& mil0 = openFor<iwMilitary>(0);
    auto& mil1 = openFor<iwMilitary>(1);
    auto& bar0 = militaryBar(mil0);
    auto& bar1 = militaryBar(mil1);

    const unsigned pos0 = shiftBar(mil0, bar0, bar0.GetID());
    const unsigned pos1 = pos0 == 0u ? 2u : pos0 - 1u;
    bar1.SetPosition(pos1);
    static_cast<Window&>(mil1).Msg_ProgressChange(bar1.GetID(), pos1);
    BOOST_TEST_REQUIRE(unsigned(bar1.GetPosition()) == pos1);

    PadGameFixture::fireTransmitTimer(mil0);
    BOOST_TEST(unsigned(bar1.GetPosition()) == pos1);

    PadGameFixture::fireTransmitTimer(mil1);
    BOOST_TEST(unsigned(bar0.GetPosition()) == pos1);
    BOOST_TEST(unsigned(GAMECLIENT.GetVisualSettings(0).military_settings[0]) == pos1);

    // Closing a window with an unsent change sends it, and the other view follows that too.
    const unsigned closePos = shiftBar(mil0, bar0, bar0.GetID());
    BOOST_TEST_REQUIRE(closePos != pos1);
    mil0.Close();
    BOOST_TEST_REQUIRE(mil0.ShouldBeClosed());
    BOOST_TEST(unsigned(bar1.GetPosition()) == closePos);

    WINDOWMANAGER.CloseNow(&mil0);
    WINDOWMANAGER.CloseNow(&mil1);
    tearDownDesktop();
}

/// Two local players with their OWN tribes: one player's transmission leaves the other's window alone.
BOOST_FIXTURE_TEST_CASE(DistinctPlayersWindowsDoNotFollowEachOther, SharedViewsFixture)
{
    setUpTwoLocalPlayers();
    BOOST_TEST_REQUIRE(dsk->GetPlayerView(0).GetPlayerId() != dsk->GetPlayerView(1).GetPlayerId());

    auto& mil0 = openFor<iwMilitary>(0);
    auto& mil1 = openFor<iwMilitary>(1);
    const unsigned before1 = militaryBar(mil1).GetPosition();
    const unsigned pos0 = shiftBar(mil0, militaryBar(mil0), militaryBar(mil0).GetID());
    BOOST_TEST_REQUIRE(pos0 != before1);

    PadGameFixture::fireTransmitTimer(mil0);
    BOOST_TEST(unsigned(militaryBar(mil1).GetPosition()) == before1);

    WINDOWMANAGER.CloseNow(&mil0);
    WINDOWMANAGER.CloseNow(&mil1);
    tearDownDesktop();
}

BOOST_AUTO_TEST_SUITE_END()
