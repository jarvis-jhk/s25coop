// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Loader.h"
#include "MenuPadFixture.h"
#include "controls/ctrlScrollBar.h"
#include "controls/ctrlTable.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "helpers/optional_io.h"
#include "ogl/glFont.h"
#include <boost/test/unit_test.hpp>
#include <memory>
#include <vector>

namespace {
constexpr PadDeviceId pad = 94;

struct TableDesktop : Desktop
{
    ctrlTable& table;
    std::vector<std::optional<unsigned>> selections;
    std::vector<unsigned> chosen;

    TableDesktop()
        : Desktop(nullptr), table(*AddTable(1, DrawPoint(20, 20), Extent(300, 120), TextureColor::Grey, NormalFont,
                                            {{"Map", 1, TableSortType::String}}))
    {}

    bool WantsPadInput() const override { return true; }
    void Msg_TableSelectItem(unsigned, const std::optional<unsigned>& selection) override
    {
        selections.push_back(selection);
    }
    void Msg_TableChooseItem(unsigned, const unsigned selection) override { chosen.push_back(selection); }
};

struct TableSelectionFixture : rttr::test::MenuPadFixture
{
    TableDesktop& enter(const unsigned rows)
    {
        padInput().Reset();
        auto next = std::make_unique<TableDesktop>();
        auto& result = *next;
        populate(result.table, rows);
        WINDOWMANAGER.Switch(std::move(next));
        frame();
        pickUp(pad);
        return result;
    }

    static void populate(ctrlTable& table, const unsigned rows)
    {
        for(unsigned i = 0; i < rows; ++i)
            table.AddRow({"Map " + std::to_string(i)});
    }

    void move(const bool controller, const bool down)
    {
        if(controller)
            press(pad, down ? PadButton::DpadDown : PadButton::DpadUp);
        else
        {
            WINDOWMANAGER.Msg_KeyDown(KeyEvent(down ? KeyType::Down : KeyType::Up));
            frame();
        }
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(MenuPadTableSelectionTests)

BOOST_FIXTURE_TEST_CASE(FreshTablesHaveNoSelectionOrActivation, TableSelectionFixture)
{
    for(const unsigned rows : {0u, 1u, 3u})
    {
        auto& dsk = enter(rows);
        BOOST_TEST(!dsk.table.GetSelection());
        BOOST_TEST(!dsk.table.CanActivate());
        BOOST_TEST(dsk.selections.empty());
        press(pad, PadButton::A);
        BOOST_TEST(dsk.chosen.empty());
    }
}

BOOST_FIXTURE_TEST_CASE(FirstArrowSelectsRowZeroForKeyboardAndController, TableSelectionFixture)
{
    for(const bool controller : {false, true})
    {
        for(const bool down : {false, true})
        {
            auto& dsk = enter(3);
            move(controller, down);
            BOOST_TEST_REQUIRE(dsk.table.GetSelection());
            BOOST_TEST(*dsk.table.GetSelection() == 0u);
            BOOST_TEST_REQUIRE(dsk.selections.size() == 1u);
            BOOST_TEST(dsk.selections.front() == 0u);
            press(pad, PadButton::A);
            BOOST_TEST_REQUIRE(dsk.chosen.size() == 1u);
            BOOST_TEST(dsk.chosen.front() == 0u);
        }
    }
}

BOOST_FIXTURE_TEST_CASE(ClearedAndRefilledTablesStartAtTheFirstRow, TableSelectionFixture)
{
    for(const bool controller : {false, true})
    {
        for(const bool down : {false, true})
        {
            auto& dsk = enter(3);
            move(controller, true);
            move(controller, true);
            dsk.table.DeleteAllItems();
            BOOST_TEST(!dsk.table.GetSelection());
            populate(dsk.table, 3);
            frame();
            move(controller, down);
            BOOST_TEST_REQUIRE(dsk.table.GetSelection());
            BOOST_TEST(*dsk.table.GetSelection() == 0u);
        }
    }
}

BOOST_FIXTURE_TEST_CASE(EmptyTablesStayUnselectedUntilTheyHaveRows, TableSelectionFixture)
{
    for(const bool controller : {false, true})
    {
        auto& dsk = enter(0);
        for(const bool down : {false, true})
            move(controller, down);
        press(pad, PadButton::A);
        BOOST_TEST(!dsk.table.GetSelection());
        BOOST_TEST(dsk.selections.empty());
        BOOST_TEST(dsk.chosen.empty());
        populate(dsk.table, 1);
        frame();
        if(controller)
            press(pad, PadButton::RightShoulder); // The formerly empty table can now acquire focus.
        move(controller, true);
        BOOST_TEST_REQUIRE(dsk.table.GetSelection());
        BOOST_TEST(*dsk.table.GetSelection() == 0u);
        for(const bool down : {false, true})
            move(controller, down);
        BOOST_TEST(dsk.table.GetSelection() == 0u);
    }
}

BOOST_FIXTURE_TEST_CASE(MixedInputsClampAtTheEdgesAndScrollIntoView, TableSelectionFixture)
{
    auto& dsk = enter(40);
    move(true, true);
    BOOST_TEST_REQUIRE(dsk.table.GetSelection() == 0u);
    move(false, false);
    BOOST_TEST(dsk.table.GetSelection() == 0u);
    for(unsigned i = 1; i < 40; ++i)
    {
        move(i % 2 == 0, true);
        BOOST_TEST_REQUIRE(dsk.table.GetSelection() == i);
    }
    const auto& scrollbar = *dsk.table.GetCtrl<ctrlScrollBar>(0);
    BOOST_TEST(scrollbar.GetScrollPos() > 0u);
    BOOST_TEST(39u >= scrollbar.GetScrollPos());
    BOOST_TEST(39u < scrollbar.GetScrollPos() + scrollbar.GetPageSize());
    const auto callbacks = dsk.selections.size();
    move(true, true);
    move(false, true);
    BOOST_TEST(dsk.table.GetSelection() == 39u);
    BOOST_TEST(dsk.selections.size() == callbacks);
    for(unsigned i = 39; i > 0; --i)
        move(i % 2 == 0, false);
    BOOST_TEST(dsk.table.GetSelection() == 0u);
    BOOST_TEST(scrollbar.GetScrollPos() == 0u);
}

BOOST_FIXTURE_TEST_CASE(MouseSelectionStillFeedsKeyboardAndControllerNavigation, TableSelectionFixture)
{
    auto& dsk = enter(3);
    // Header height is font height plus 10; select the centre of the second content row.
    const auto fontHeight = NormalFont->getHeight();
    MouseCoords mc(dsk.table.GetDrawPos() + DrawPoint(20, fontHeight + 10 + fontHeight + fontHeight / 2));
    mc.ldown = true;
    WINDOWMANAGER.Msg_LeftDown(mc);
    mc.ldown = false;
    WINDOWMANAGER.Msg_LeftUp(mc);
    BOOST_TEST_REQUIRE(dsk.table.GetSelection() == 1u);
    move(false, false);
    BOOST_TEST(dsk.table.GetSelection() == 0u);
    move(true, true);
    BOOST_TEST(dsk.table.GetSelection() == 1u);
    press(pad, PadButton::A);
    BOOST_TEST_REQUIRE(dsk.chosen.size() == 1u);
    BOOST_TEST(dsk.chosen.front() == 1u);
}

BOOST_AUTO_TEST_SUITE_END()
