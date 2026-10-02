// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "driver/GuiScale.h"
#include "input/ControllerPanelModel.h"
#include <boost/test/unit_test.hpp>
#include <array>

namespace {
using Navigation = ControllerPanelNavigation;
using Tab = Navigation::Tab;
using LayerKind = Navigation::LayerKind;
using BackResult = Navigation::BackResult;
using TabResult = Navigation::TabResult;

void requireInside(const Rect& child, const Rect& parent)
{
    BOOST_TEST(child.left >= parent.left);
    BOOST_TEST(child.top >= parent.top);
    BOOST_TEST(child.right >= child.left);
    BOOST_TEST(child.bottom >= child.top);
    BOOST_TEST(child.right <= parent.right);
    BOOST_TEST(child.bottom <= parent.bottom);
}

void requireLayoutInside(const Viewport& viewport)
{
    const auto layout = CalcControllerPanelLayout(viewport);
    const Rect owner(viewport.origin, viewport.size);
    requireInside(layout.panel, owner);
    requireInside(layout.tabs, layout.panel);
    requireInside(layout.content, layout.panel);
    requireInside(layout.footer, layout.panel);
    requireInside(layout.world, owner);
    BOOST_TEST(layout.tabs.bottom == layout.content.top);
    BOOST_TEST(layout.content.bottom == layout.footer.top);
    BOOST_TEST(layout.tabs.left == layout.content.left);
    BOOST_TEST(layout.footer.right == layout.content.right);
    if(layout.compact)
        BOOST_TEST((layout.world == owner));
    else
    {
        BOOST_TEST(layout.world.right + 8 == layout.panel.left);
        BOOST_TEST(layout.world.getSize().x >= (viewport.size.x >= 960 ? 480u : 456u));
    }
}
} // namespace

BOOST_AUTO_TEST_SUITE(ControllerPanelModel)

BOOST_AUTO_TEST_CASE(ReferenceSingleViewGeometry)
{
    const auto deck = CalcControllerPanelLayout({Position(0, 0), Extent(1280, 800)});
    BOOST_TEST(!deck.compact);
    BOOST_TEST((deck.panel == Rect(824, 8, 448, 784)));
    BOOST_TEST((deck.tabs == Rect(832, 16, 432, 32)));
    BOOST_TEST((deck.content == Rect(832, 48, 432, 704)));
    BOOST_TEST((deck.footer == Rect(832, 752, 432, 32)));
    BOOST_TEST((deck.world == Rect(0, 0, 816, 800)));

    const auto minimum = CalcControllerPanelLayout({Position(30, 50), Extent(800, 600)});
    BOOST_TEST(!minimum.compact);
    BOOST_TEST((minimum.panel == Rect(502, 58, 320, 584)));
    BOOST_TEST((minimum.content == Rect(510, 98, 304, 504)));
    BOOST_TEST((minimum.world == Rect(30, 50, 464, 600)));

    const auto fourWay = CalcControllerPanelLayout({Position(640, 400), Extent(640, 400)});
    BOOST_TEST(fourWay.compact);
    BOOST_TEST((fourWay.panel == Rect(952, 408, 320, 384)));
    BOOST_TEST((fourWay.world == Rect(640, 400, 640, 400)));
}

BOOST_AUTO_TEST_CASE(ThresholdsAndWidthClamps)
{
    struct Expected
    {
        Extent size;
        bool compact;
        unsigned width;
    };
    const std::array<Expected, 10> cases{{{Extent(799, 600), true, 320},
                                          {Extent(800, 479), true, 320},
                                          {Extent(800, 480), false, 320},
                                          {Extent(959, 600), false, 320},
                                          {Extent(960, 600), false, 336},
                                          {Extent(961, 600), false, 336},
                                          {Extent(1281, 801), false, 448},
                                          {Extent(1371, 800), false, 479},
                                          {Extent(1372, 800), false, 480},
                                          {Extent(1920, 1080), false, 480}}};
    for(const auto& expected : cases)
    {
        const Viewport viewport{Position(17, 23), expected.size};
        const auto layout = CalcControllerPanelLayout(viewport);
        BOOST_TEST(layout.compact == expected.compact);
        BOOST_TEST(layout.panel.getSize().x == expected.width);
        requireLayoutInside(viewport);
    }
}

BOOST_AUTO_TEST_CASE(AllActualCellsAndGuiScalesStayContained)
{
    for(const Extent screen : {Extent(800, 600), Extent(1280, 720), Extent(1280, 800), Extent(1920, 1080),
                               Extent(1281, 801), Extent(601, 1001)})
    {
        for(const unsigned percent : {75u, 100u, 125u, 150u, 200u})
        {
            // This is the established screen->view conversion, before viewport splitting.
            const auto render = GuiScale(percent).screenToView<Extent>(screen);
            for(unsigned numViews = 1; numViews <= 4; ++numViews)
            {
                BOOST_TEST_CONTEXT(screen.x << "x" << screen.y << " scale " << percent << " views " << numViews)
                {
                    const auto cells = CalcViewports(render, numViews);
                    BOOST_TEST_REQUIRE(cells.size() == numViews);
                    for(const auto& cell : cells)
                        requireLayoutInside(cell);
                }
            }
        }
    }
    // The third player's larger actual cell must not inherit the top row's width.
    const auto cells = CalcViewports(Extent(1920, 1080), 3);
    BOOST_TEST(CalcControllerPanelLayout(cells[0]).panel.getSize().x == 336u);
    BOOST_TEST(CalcControllerPanelLayout(cells[2]).panel.getSize().x == 480u);
}

BOOST_AUTO_TEST_CASE(TransientTinyAndEmptyCellsNeverUnderflow)
{
    for(unsigned w = 0; w <= 40; ++w)
    {
        for(unsigned h = 0; h <= 100; ++h)
            requireLayoutInside({Position(13, 19), Extent(w, h)});
    }
}

BOOST_AUTO_TEST_CASE(TabsWrapBothWaysAndRememberTheirOwnRows)
{
    Navigation navigation;
    BOOST_TEST(!navigation.IsOpen());
    BOOST_TEST((navigation.CycleTab(true) == TabResult::Inactive));
    BOOST_TEST((navigation.CycleTab(false) == TabResult::Inactive));
    navigation.UpdateRows(Tab::Buildings, {10, 20, 30}, 1);
    navigation.UpdateRows(Tab::Stock, {110, 120}, 1);
    BOOST_TEST(navigation.SelectRow(Tab::Buildings, 30));
    BOOST_TEST(navigation.SelectRow(Tab::Stock, 120));
    navigation.Open();
    BOOST_TEST((navigation.GetTab() == Tab::Buildings));
    BOOST_TEST((navigation.CycleTab(false) == TabResult::Changed));
    BOOST_TEST((navigation.GetTab() == Tab::Settings));
    BOOST_TEST((navigation.CycleTab(true) == TabResult::Changed));
    BOOST_TEST((navigation.GetTab() == Tab::Buildings));
    BOOST_TEST((navigation.CycleTab(true) == TabResult::Changed));
    BOOST_TEST((navigation.GetTab() == Tab::Stock));
    for(const Tab expected : {Tab::Statistics, Tab::Post, Tab::Settings, Tab::Buildings})
    {
        BOOST_TEST((navigation.CycleTab(true) == TabResult::Changed));
        BOOST_TEST((navigation.GetTab() == expected));
    }
    BOOST_TEST((navigation.GetPage(Tab::Buildings).selectedId == 30u));
    BOOST_TEST(navigation.GetPage(Tab::Buildings).firstVisibleRow == 2u);
    BOOST_TEST((navigation.GetPage(Tab::Stock).selectedId == 120u));
    BOOST_TEST(navigation.GetPage(Tab::Stock).firstVisibleRow == 1u);
}

BOOST_AUTO_TEST_CASE(NestedCancellationAndShouldersStayWithTheChild)
{
    Navigation navigation;
    BOOST_TEST(!navigation.PushLayer(LayerKind::Detail, 1));
    BOOST_TEST((navigation.PeekBack() == BackResult::Inactive));
    BOOST_TEST((navigation.Back() == BackResult::Inactive));
    navigation.Open();
    BOOST_TEST((navigation.CycleTab(true) == TabResult::Changed));
    BOOST_TEST(navigation.PushLayer(LayerKind::Detail, 10));
    BOOST_TEST(navigation.PushLayer(LayerKind::Detail, 20));
    for(const LayerKind input : {LayerKind::Dropdown, LayerKind::Edit, LayerKind::Confirmation})
    {
        BOOST_TEST(navigation.PushLayer(input, 30));
        BOOST_TEST((navigation.PeekBack() == BackResult::CancelInput));
        BOOST_TEST_REQUIRE(navigation.GetLayers().size() == 3u);
        BOOST_TEST((navigation.GetLayers().back().kind == input));
        BOOST_TEST(navigation.GetLayers().back().id == 30u);
        // Asking about close or pressing a shoulder does not accept/discard pending input.
        BOOST_TEST((navigation.CycleTab(true) == TabResult::BlockedByChild));
        BOOST_TEST((navigation.CycleTab(false) == TabResult::BlockedByChild));
        BOOST_TEST(navigation.GetLayers().size() == 3u);
        BOOST_TEST((navigation.Back() == BackResult::CancelInput));
        BOOST_TEST(navigation.GetLayers().size() == 2u);
    }
    for(const unsigned id : {20u, 10u})
    {
        BOOST_TEST((navigation.PeekBack() == BackResult::ReturnFromDetail));
        BOOST_TEST_REQUIRE(navigation.GetLayers().size() == (id == 20 ? 2u : 1u));
        BOOST_TEST(navigation.GetLayers().back().id == id);
        BOOST_TEST((navigation.CycleTab(true) == TabResult::BlockedByChild));
        BOOST_TEST((navigation.CycleTab(false) == TabResult::BlockedByChild));
        BOOST_TEST((navigation.GetTab() == Tab::Stock));
        BOOST_TEST((navigation.Back() == BackResult::ReturnFromDetail));
        BOOST_TEST(navigation.IsOpen());
    }
    BOOST_TEST(navigation.GetLayers().empty());
    BOOST_TEST((navigation.PeekBack() == BackResult::Close));
    BOOST_TEST(navigation.IsOpen());
    BOOST_TEST((navigation.Back() == BackResult::Close));
    BOOST_TEST(!navigation.IsOpen());
    navigation.Open();
    BOOST_TEST((navigation.GetTab() == Tab::Stock));
    BOOST_TEST((navigation.CycleTab(false) == TabResult::Changed));
    BOOST_TEST((navigation.GetTab() == Tab::Buildings));
}

BOOST_AUTO_TEST_CASE(RowIdentityAndFocusSurviveReflowAndReplacement)
{
    Navigation navigation;
    navigation.UpdateRows(Tab::Post, {10, 20, 30, 40, 50}, 2);
    BOOST_TEST((navigation.GetPage(Tab::Post).selectedId == 10u));
    BOOST_TEST(navigation.SelectRow(Tab::Post, 50));
    BOOST_TEST(navigation.GetPage(Tab::Post).firstVisibleRow == 3u);
    BOOST_TEST(!navigation.SelectRow(Tab::Post, 60));
    BOOST_TEST((navigation.GetPage(Tab::Post).selectedId == 50u));
    BOOST_TEST(navigation.GetPage(Tab::Post).firstVisibleRow == 3u);

    navigation.UpdateRows(Tab::Post, {50, 40, 30, 20, 10}, 2);
    BOOST_TEST((navigation.GetPage(Tab::Post).selectedId == 50u));
    BOOST_TEST(navigation.GetPage(Tab::Post).firstVisibleRow == 0u);
    BOOST_TEST(navigation.SelectRow(Tab::Post, 10));
    navigation.UpdateRows(Tab::Post, {50, 40, 30, 20, 10}, 1);
    BOOST_TEST((navigation.GetPage(Tab::Post).selectedId == 10u));
    BOOST_TEST(navigation.GetPage(Tab::Post).firstVisibleRow == 4u);
    navigation.UpdateRows(Tab::Post, {50, 40, 30, 20, 10}, 20);
    BOOST_TEST(navigation.GetPage(Tab::Post).firstVisibleRow == 0u);

    navigation.UpdateRows(Tab::Post, {50, 40, 30, 20, 60}, 2);
    BOOST_TEST((navigation.GetPage(Tab::Post).selectedId == 50u));
    BOOST_TEST(navigation.GetPage(Tab::Post).firstVisibleRow == 0u);
    navigation.UpdateRows(Tab::Post, {}, 0);
    BOOST_TEST(!navigation.GetPage(Tab::Post).selectedId);
    BOOST_TEST(navigation.GetPage(Tab::Post).rows.empty());
    BOOST_TEST(navigation.GetPage(Tab::Post).firstVisibleRow == 0u);
    BOOST_TEST(navigation.GetPage(Tab::Post).visibleRows == 1u);
    BOOST_TEST(!navigation.SelectRow(Tab::Post, 50));
    navigation.UpdateRows(Tab::Post, {80, 90}, 0);
    BOOST_TEST((navigation.GetPage(Tab::Post).selectedId == 80u));
    BOOST_TEST(navigation.SelectRow(Tab::Post, 90));
    BOOST_TEST(navigation.GetPage(Tab::Post).firstVisibleRow == 1u);
}

BOOST_AUTO_TEST_CASE(RemovedChildTargetsRequireCallerCancellation)
{
    Navigation navigation;
    navigation.Open();
    navigation.UpdateRows(Tab::Buildings, {10, 20, 30}, 1);
    BOOST_TEST(navigation.SelectRow(Tab::Buildings, 20));
    BOOST_TEST(navigation.PushLayer(LayerKind::Detail, 20));
    BOOST_TEST(navigation.PushLayer(LayerKind::Edit, 100));
    navigation.UpdateRows(Tab::Buildings, {10, 40, 30}, 2);
    BOOST_TEST((navigation.GetPage(Tab::Buildings).selectedId == 10u));
    BOOST_TEST(navigation.GetPage(Tab::Buildings).firstVisibleRow == 0u);
    // Only the page adapter knows which child routes refer to the removed object.
    // Refresh must not silently accept/discard an edit; ids remain owned values.
    BOOST_TEST_REQUIRE(navigation.GetLayers().size() == 2u);
    BOOST_TEST(navigation.GetLayers()[0].id == 20u);
    BOOST_TEST(navigation.GetLayers()[1].id == 100u);
    BOOST_TEST((navigation.PeekBack() == BackResult::CancelInput));
    BOOST_TEST((navigation.CycleTab(true) == TabResult::BlockedByChild));
    BOOST_TEST((navigation.Back() == BackResult::CancelInput));
    BOOST_TEST((navigation.Back() == BackResult::ReturnFromDetail));
    BOOST_TEST(navigation.GetLayers().empty());
    BOOST_TEST(navigation.IsOpen());
    BOOST_TEST((navigation.GetPage(Tab::Buildings).selectedId == 10u));
    BOOST_TEST((navigation.CycleTab(true) == TabResult::Changed));
}

BOOST_AUTO_TEST_CASE(ViewsAreIndependentAndResizeDoesNotDiscardNavigation)
{
    Navigation left, right;
    left.Open();
    right.Open();
    BOOST_TEST((left.CycleTab(true) == TabResult::Changed));
    left.UpdateRows(Tab::Stock, {1, 2, 3, 4}, 1);
    right.UpdateRows(Tab::Stock, {1, 2, 3, 4}, 2);
    BOOST_TEST(left.SelectRow(Tab::Stock, 4));
    BOOST_TEST(left.PushLayer(LayerKind::Detail, 4));
    BOOST_TEST(left.PushLayer(LayerKind::Edit, 5));
    // Geometry changes independently of each view's navigation model and selected stable id.
    for(const Extent size : {Extent(1280, 800), Extent(640, 400), Extent(800, 600)})
    {
        const auto layout = CalcControllerPanelLayout({Position(0, 0), size});
        const unsigned rows = layout.content.getSize().y / 32;
        left.UpdateRows(Tab::Stock, {1, 2, 3, 4}, rows);
        BOOST_TEST((left.GetPage(Tab::Stock).selectedId == 4u));
        BOOST_TEST(left.GetLayers().size() == 2u);
        BOOST_TEST((left.GetTab() == Tab::Stock));
    }
    BOOST_TEST((right.GetTab() == Tab::Buildings));
    BOOST_TEST((right.GetPage(Tab::Stock).selectedId == 1u));
    BOOST_TEST(right.GetLayers().empty());
    left.Disconnect();
    BOOST_TEST(!left.IsOpen());
    BOOST_TEST(left.GetLayers().empty());
    BOOST_TEST((left.CycleTab(true) == TabResult::Inactive));
    BOOST_TEST((left.PeekBack() == BackResult::Inactive));
    BOOST_TEST(right.IsOpen());
    left.Open();
    BOOST_TEST((left.GetTab() == Tab::Stock));
    BOOST_TEST((left.GetPage(Tab::Stock).selectedId == 4u));
    BOOST_TEST((left.PeekBack() == BackResult::Close));
    left.Open(); // Opening an already open panel is idempotent.
    BOOST_TEST((left.GetTab() == Tab::Stock));
}

BOOST_AUTO_TEST_SUITE_END()
