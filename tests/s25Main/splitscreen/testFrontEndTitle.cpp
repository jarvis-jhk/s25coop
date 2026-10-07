// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Loader.h"
#include "MenuPadFixture.h"
#include "controls/ctrlButton.h"
#include "desktops/dskHome.h"
#include "desktops/dskTitle.h"
#include "driver/MouseCoords.h"
#include "input/Party.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <memory>

// F2: the title page and the party. Pads only through the mock driver, the mouse through the WindowManager.

namespace {
constexpr PadDeviceId padA = 41, padB = 42, padC = 43, padD = 44, padE = 45;

struct TitleFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder userData;
    rttr::test::ConfigOverride userDataOverride{"USERDATA", userData};
    rttr::test::ConfigOverride gameOverride{"GAME", userData};

    TitleFixture()
    {
        dskHome::ForgetLastChoice();
        WINDOWMANAGER.Switch(dskTitle::Create());
        frame();
    }
    // NOLINTNEXTLINE(bugprone-exception-escape)
    ~TitleFixture() override
    {
        dskHome::ForgetLastChoice();
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        WINDOWMANAGER.Draw();
    }
    static const Party& party() { return padInput().GetParty(); }
    static std::vector<PadDeviceId> members() { return party().Members(); }
    static dskTitle& title()
    {
        auto* t = desktopAs<dskTitle>();
        BOOST_TEST_REQUIRE(t != nullptr);
        return *t;
    }
    /// A controller is plugged in and its player presses A once.
    void joinWithA(PadDeviceId dev)
    {
        connect(dev);
        press(dev, PadButton::A);
    }
    static bool footerHas(PadButton button, brief::KeyAction action)
    {
        const auto& keys = title().GetFooterKeys();
        return std::find(keys.begin(), keys.end(), brief::KeyHint{button, action}) != keys.end();
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(FrontEndTitleTests)

BOOST_FIXTURE_TEST_CASE(EveryControllerJoinsWithOnePress, TitleFixture)
{
    BOOST_TEST(party().IsEmpty());
    BOOST_TEST(title().GetPlayerStrip().empty());
    joinWithA(padA);
    BOOST_TEST((members() == std::vector<PadDeviceId>{padA}));
    // The joining press is not also a click on Start.
    BOOST_TEST(desktopAs<dskTitle>() != nullptr);
    joinWithA(padB);
    joinWithA(padC);
    BOOST_TEST((members() == std::vector<PadDeviceId>{padA, padB, padC}));
    frame();
    BOOST_TEST(title().GetPlayerStrip().size() == 3u);
    BOOST_TEST(footerHas(PadButton::A, brief::KeyAction::Choose));
    BOOST_TEST(footerHas(PadButton::B, brief::KeyAction::LeaveParty));
    // Four cards in one row, P1 left of P2..., all inside the content area.
    BOOST_TEST_REQUIRE(title().GetCards().size() == 4u);
    for(unsigned i = 1; i < 4; ++i)
    {
        BOOST_TEST(title().GetCards()[i].top == title().GetCards()[0].top);
        BOOST_TEST(title().GetCards()[i].left > title().GetCards()[i - 1].right);
    }
    for(const Rect& card : title().GetCards())
    {
        const Rect& c = title().GetFrame().content;
        BOOST_TEST((card.left >= c.left && card.right <= c.right && card.top >= c.top && card.bottom <= c.bottom));
    }
    // A fifth controller finds the party full and gets nothing.
    joinWithA(padD);
    joinWithA(padE);
    BOOST_TEST(members().size() == 4u);
    BOOST_TEST(!party().Contains(padE));
}

BOOST_FIXTURE_TEST_CASE(BLeavesARejoinsUnplugLeaves, TitleFixture)
{
    joinWithA(padA);
    joinWithA(padB);
    joinWithA(padC);
    press(padB, PadButton::B);
    BOOST_TEST((members() == std::vector<PadDeviceId>{padA, padC}));
    // Leaving sticks: padB keeps its slot, but that alone does not rejoin it.
    frame();
    BOOST_TEST(!party().Contains(padB));
    // A again is a join, not a Start.
    press(padB, PadButton::A);
    BOOST_TEST((members() == std::vector<PadDeviceId>{padA, padC, padB}));
    BOOST_TEST(desktopAs<dskTitle>() != nullptr);
    // Unplugged controllers leave the party, on any page.
    disconnect(padC);
    frame();
    BOOST_TEST((members() == std::vector<PadDeviceId>{padA, padB}));
}

BOOST_FIXTURE_TEST_CASE(AMemberStartsAndThePartyStaysOnEveryPage, TitleFixture)
{
    joinWithA(padA);
    joinWithA(padB);
    press(padB, PadButton::A);
    frame();
    auto* home = desktopAs<dskHome>();
    BOOST_TEST_REQUIRE(home != nullptr);
    BOOST_TEST((members() == std::vector<PadDeviceId>{padA, padB}));
    BOOST_TEST(home->GetPlayerStrip().size() == 2u);
    // Back on the title page (B on Home) the party is still there and one more can join.
    press(padA, PadButton::B);
    frame();
    BOOST_TEST_REQUIRE(desktopAs<dskTitle>() != nullptr);
    BOOST_TEST(members().size() == 2u);
    joinWithA(padC);
    BOOST_TEST(members().size() == 3u);
    // Start from a member continues as well.
    press(padC, PadButton::Start);
    frame();
    BOOST_TEST(desktopAs<dskHome>() != nullptr);
}

BOOST_FIXTURE_TEST_CASE(MouseStartsWithoutAnyController, TitleFixture)
{
    const Window& start = *title().GetCtrl<Window>(dskTitle::ID_Start);
    const Rect r = start.GetDrawRect();
    BOOST_TEST((r.bottom <= title().GetFrame().content.bottom));
    MouseCoords mc(Position((r.left + r.right) / 2, (r.top + r.bottom) / 2));
    mc.ldown = true;
    WINDOWMANAGER.Msg_LeftDown(mc);
    mc.ldown = false;
    WINDOWMANAGER.Msg_LeftUp(mc);
    frame();
    frame();
    BOOST_TEST(desktopAs<dskHome>() != nullptr);
    BOOST_TEST(party().IsEmpty());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(PartyModel)

BOOST_AUTO_TEST_CASE(JoinLeaveRetain)
{
    Party p;
    BOOST_TEST((p.Join(7) == 0u));
    BOOST_TEST((p.Join(8) == 1u));
    BOOST_TEST((p.Join(7) == 0u)); // already a member
    BOOST_TEST((p.Join(9) == 2u));
    BOOST_TEST((p.Join(10) == 3u));
    BOOST_TEST(!p.Join(11).has_value());
    BOOST_TEST(p.Leave(8));
    BOOST_TEST(!p.Leave(8));
    BOOST_TEST((p.IndexOf(9) == 1u));
    p.Retain([](PadDeviceId d) noexcept { return d != 7; });
    BOOST_TEST((p.Members() == std::vector<PadDeviceId>{9, 10}));
}

BOOST_AUTO_TEST_SUITE_END()
