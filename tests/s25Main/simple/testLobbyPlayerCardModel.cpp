// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "input/LobbyPlayerCardModel.h"
#include "gameTypes/GameTypesOutput.h"
#include "s25util/colors.h"
#include <boost/test/unit_test.hpp>
#include <limits>

namespace {
using Card = LobbyPlayerCardModel;
using Row = Card::Row;

Card::Snapshot snapshot()
{
    return {{PLAYER_COLORS[0], Nation::Romans, Team::None, false}, {}, {}};
}

void requireValues(const Card::Values& actual, const Card::Values& expected)
{
    BOOST_TEST(actual.color == expected.color);
    BOOST_TEST(actual.nation == expected.nation);
    BOOST_TEST(actual.team == expected.team);
    BOOST_TEST(actual.sharedTribe == expected.sharedTribe);
}
} // namespace

BOOST_AUTO_TEST_SUITE(LobbyPlayerCards)

BOOST_AUTO_TEST_CASE(StableRowsClampAndRejectInvalidIds)
{
    Card card(3, snapshot());
    BOOST_TEST(card.GetSeatId() == 3u);
    BOOST_TEST(static_cast<unsigned>(card.GetRow()) == 0u);
    BOOST_TEST(!card.MoveRow(false));
    for(unsigned expected = 1; expected <= 3; ++expected)
    {
        BOOST_TEST_REQUIRE(card.MoveRow(true));
        BOOST_TEST(static_cast<unsigned>(card.GetRow()) == expected);
    }
    BOOST_TEST(!card.MoveRow(true));
    for(unsigned expected : {2u, 1u, 0u})
    {
        BOOST_TEST_REQUIRE(card.MoveRow(false));
        BOOST_TEST(static_cast<unsigned>(card.GetRow()) == expected);
    }
    BOOST_TEST_REQUIRE(card.SelectRow(Row::Team));
    BOOST_TEST(!card.SelectRow(static_cast<Row>(4)));
    BOOST_TEST(!card.SelectRow(static_cast<Row>(std::numeric_limits<unsigned>::max())));
    BOOST_TEST(static_cast<unsigned>(card.GetRow()) == 2u);
    requireValues(card.GetSnapshot().values, snapshot().values);
}

BOOST_AUTO_TEST_CASE(AllPaletteColorsWrapInBothDirectionsWithoutChangingOtherFields)
{
    for(size_t current = 0; current < PLAYER_COLORS.size(); ++current)
    {
        auto state = snapshot();
        state.values = {PLAYER_COLORS[current], Nation::Babylonians, Team::Random1To3, true};
        const Card card(0, state);
        for(const bool forward : {false, true})
        {
            const auto proposal = card.ProposeValue(forward);
            BOOST_TEST_REQUIRE(proposal.has_value());
            auto expected = state.values;
            expected.color = PLAYER_COLORS[(current + (forward ? 1 : PLAYER_COLORS.size() - 1)) % PLAYER_COLORS.size()];
            requireValues(*proposal, expected);
            requireValues(card.GetSnapshot().values, state.values);
        }
    }
}

BOOST_AUTO_TEST_CASE(SkipsOtherSlotsAndWrapsOverTakenColors)
{
    auto state = snapshot();
    state.takenColors = {PLAYER_COLORS[1], PLAYER_COLORS[2], PLAYER_COLORS.back(), PLAYER_COLORS[1], 0x12345678u};
    const Card card(0, state);
    const auto forward = card.ProposeValue(true);
    const auto backward = card.ProposeValue(false);
    BOOST_TEST_REQUIRE(forward.has_value());
    BOOST_TEST_REQUIRE(backward.has_value());
    BOOST_TEST(forward->color == PLAYER_COLORS[3]);
    BOOST_TEST(backward->color == PLAYER_COLORS[PLAYER_COLORS.size() - 2]);
    requireValues(card.GetSnapshot().values, state.values);
    // A broadcast freeing a neighbour is immediately reflected, without resetting focus or mutating proposals.
    Card refreshed(0, state);
    state.values.color = PLAYER_COLORS[2];
    state.takenColors = {PLAYER_COLORS[3], PLAYER_COLORS[4]};
    refreshed.UpdateSnapshot(state);
    const auto after = refreshed.ProposeValue(true);
    BOOST_TEST_REQUIRE(after.has_value());
    BOOST_TEST(after->color == PLAYER_COLORS[5]);
    BOOST_TEST(static_cast<unsigned>(refreshed.GetRow()) == 0u);
    state.takenColors.clear();
    refreshed.UpdateSnapshot(state);
    const auto freedForward = refreshed.ProposeValue(true);
    const auto freedBackward = refreshed.ProposeValue(false);
    BOOST_TEST_REQUIRE(freedForward.has_value());
    BOOST_TEST_REQUIRE(freedBackward.has_value());
    BOOST_TEST(freedForward->color == PLAYER_COLORS[3]);
    BOOST_TEST(freedBackward->color == PLAYER_COLORS[1]);
    requireValues(refreshed.GetSnapshot().values, state.values);
    state.takenColors = {PLAYER_COLORS[1], PLAYER_COLORS[3]};
    refreshed.UpdateSnapshot(state);
    const auto takenForward = refreshed.ProposeValue(true);
    const auto takenBackward = refreshed.ProposeValue(false);
    BOOST_TEST_REQUIRE(takenForward.has_value());
    BOOST_TEST_REQUIRE(takenBackward.has_value());
    BOOST_TEST(takenForward->color == PLAYER_COLORS[4]);
    BOOST_TEST(takenBackward->color == PLAYER_COLORS[0]);
}

BOOST_AUTO_TEST_CASE(ExhaustedPaletteTerminatesAndCustomColorRecovers)
{
    auto state = snapshot();
    state.takenColors.assign(PLAYER_COLORS.begin(), PLAYER_COLORS.end());
    for(const unsigned current : {PLAYER_COLORS[0], 0x12345678u})
    {
        state.values.color = current;
        const Card card(1, state);
        BOOST_TEST(!card.ProposeValue(true));
        BOOST_TEST(!card.ProposeValue(false));
        requireValues(card.GetSnapshot().values, state.values);
    }
    state.values.color = PLAYER_COLORS[0];
    state.takenColors.erase(state.takenColors.begin());
    const Card onlyOwnColor(1, state);
    BOOST_TEST(!onlyOwnColor.ProposeValue(true));
    BOOST_TEST(!onlyOwnColor.ProposeValue(false));

    state.values.color = 0x12345678u;
    state.takenColors.clear();
    const Card custom(1, state);
    const auto forward = custom.ProposeValue(true);
    const auto backward = custom.ProposeValue(false);
    BOOST_TEST_REQUIRE(forward.has_value());
    BOOST_TEST_REQUIRE(backward.has_value());
    BOOST_TEST(forward->color == PLAYER_COLORS.front());
    BOOST_TEST(backward->color == PLAYER_COLORS.back());
}

BOOST_AUTO_TEST_CASE(EverySingleFreeColorIsReachableFromEveryPaletteEntry)
{
    for(const unsigned current : PLAYER_COLORS)
    {
        for(const unsigned available : PLAYER_COLORS)
        {
            auto state = snapshot();
            state.values.color = current;
            for(const unsigned color : PLAYER_COLORS)
            {
                if(color != current && color != available)
                    state.takenColors.push_back(color);
            }
            const Card card(0, state);
            for(const bool forward : {false, true})
            {
                const auto proposal = card.ProposeValue(forward);
                if(current == available)
                {
                    BOOST_TEST(!proposal);
                } else
                {
                    BOOST_TEST_REQUIRE(proposal.has_value());
                    BOOST_TEST(proposal->color == available);
                }
                requireValues(card.GetSnapshot().values, state.values);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(NationsUseLobbyPresentationOrderInBothDirections)
{
    const std::array order = {Nation::Romans, Nation::Vikings, Nation::Japanese, Nation::Africans, Nation::Babylonians};
    for(size_t current = 0; current < order.size(); ++current)
    {
        auto state = snapshot();
        state.values = {PLAYER_COLORS[4], order[current], Team::Random1To4, true};
        Card card(2, state);
        BOOST_TEST_REQUIRE(card.SelectRow(Row::Nation));
        for(const bool forward : {false, true})
        {
            const auto proposal = card.ProposeValue(forward);
            BOOST_TEST_REQUIRE(proposal.has_value());
            auto expected = state.values;
            expected.nation = order[(current + (forward ? 1 : order.size() - 1)) % order.size()];
            requireValues(*proposal, expected);
            requireValues(card.GetSnapshot().values, state.values);
        }
    }
}

BOOST_AUTO_TEST_CASE(TeamsIncludeRandomPoliciesAndWrapInBothDirections)
{
    const std::array order = {Team::None,  Team::Random,     Team::Team1,      Team::Team2,     Team::Team3,
                              Team::Team4, Team::Random1To2, Team::Random1To3, Team::Random1To4};
    for(size_t current = 0; current < order.size(); ++current)
    {
        auto state = snapshot();
        state.values = {PLAYER_COLORS[6], Nation::Japanese, order[current], true};
        Card card(0, state);
        BOOST_TEST_REQUIRE(card.SelectRow(Row::Team));
        for(const bool forward : {false, true})
        {
            const auto proposal = card.ProposeValue(forward);
            BOOST_TEST_REQUIRE(proposal.has_value());
            auto expected = state.values;
            expected.team = order[(current + (forward ? 1 : order.size() - 1)) % order.size()];
            requireValues(*proposal, expected);
            requireValues(card.GetSnapshot().values, state.values);
        }
    }
    auto invalid = snapshot();
    invalid.values.team = static_cast<Team>(255);
    invalid.values.nation = static_cast<Nation>(255);
    Card card(0, invalid);
    BOOST_TEST_REQUIRE(card.SelectRow(Row::Team));
    const auto firstTeam = card.ProposeValue(true);
    const auto lastTeam = card.ProposeValue(false);
    BOOST_TEST_REQUIRE(firstTeam.has_value());
    BOOST_TEST_REQUIRE(lastTeam.has_value());
    BOOST_TEST(firstTeam->team == Team::None);
    BOOST_TEST(lastTeam->team == Team::Random1To4);
    BOOST_TEST_REQUIRE(card.SelectRow(Row::Nation));
    const auto firstNation = card.ProposeValue(true);
    const auto lastNation = card.ProposeValue(false);
    BOOST_TEST_REQUIRE(firstNation.has_value());
    BOOST_TEST_REQUIRE(lastNation.has_value());
    BOOST_TEST(firstNation->nation == Nation::Romans);
    BOOST_TEST(lastNation->nation == Nation::Babylonians);
    requireValues(card.GetSnapshot().values, invalid.values);
}

BOOST_AUTO_TEST_CASE(SharedTribeProposalsDoNotChangeSlotsOrOtherValues)
{
    for(const bool shared : {false, true})
    {
        auto state = snapshot();
        state.values.sharedTribe = shared;
        Card card(3, state);
        BOOST_TEST_REQUIRE(card.SelectRow(Row::SharedTribe));
        for(const bool forward : {false, true})
        {
            const auto proposal = card.ProposeValue(forward);
            BOOST_TEST_REQUIRE(proposal.has_value());
            auto expected = state.values;
            expected.sharedTribe = !shared;
            requireValues(*proposal, expected);
            requireValues(card.GetSnapshot().values, state.values);
            BOOST_TEST(card.GetSeatId() == 3u);
        }
    }
}

BOOST_AUTO_TEST_CASE(CampaignAndReadOnlyLocksRemainFocusableAndBlockAllProposals)
{
    auto state = snapshot();
    state.lockReasons = {"Campaign colour", "Campaign nation", "Campaign team", "Campaign requires one tribe"};
    Card card(1, state);
    for(unsigned index = 0; index < state.lockReasons.size(); ++index)
    {
        BOOST_TEST_REQUIRE(card.SelectRow(static_cast<Row>(index)));
        BOOST_TEST(card.GetLockReason() == state.lockReasons[index]);
        BOOST_TEST(!card.ProposeValue(true));
        BOOST_TEST(!card.ProposeValue(false));
        requireValues(card.GetSnapshot().values, state.values);
    }
    state.lockReasons.fill("Remote player: read only");
    card.UpdateSnapshot(state);
    BOOST_TEST(card.GetLockReason() == "Remote player: read only");
    BOOST_TEST_REQUIRE(card.MoveRow(false));
    BOOST_TEST(card.GetLockReason() == "Remote player: read only");
    BOOST_TEST(!card.ProposeValue(true));
    BOOST_TEST_REQUIRE(card.MoveRow(true));
    state.lockReasons[3].clear();
    card.UpdateSnapshot(state);
    BOOST_TEST(card.GetLockReason().empty());
    BOOST_TEST_REQUIRE(card.ProposeValue(false).has_value());
}

BOOST_AUTO_TEST_CASE(RefreshRetainsSeatFocusAndIndependentSharedViewCards)
{
    auto state = snapshot();
    Card first(0, state);
    Card second(1, state);
    BOOST_TEST_REQUIRE(first.SelectRow(Row::Nation));
    BOOST_TEST_REQUIRE(second.SelectRow(Row::Team));
    const auto pending = first.ProposeValue(true);
    BOOST_TEST_REQUIRE(pending.has_value());
    state.values = {PLAYER_COLORS[8], Nation::Africans, Team::Team3, true};
    state.lockReasons[1] = "Campaign fixes this nation";
    first.UpdateSnapshot(state);
    requireValues(first.GetSnapshot().values, state.values);
    BOOST_TEST(first.GetSeatId() == 0u);
    BOOST_TEST(static_cast<unsigned>(first.GetRow()) == 1u);
    BOOST_TEST(first.GetLockReason() == "Campaign fixes this nation");
    BOOST_TEST(!first.ProposeValue(true));
    // Previously returned proposals are owned copies; refresh does not accept them or change another seat.
    BOOST_TEST(pending->nation == Nation::Vikings);
    BOOST_TEST(second.GetSeatId() == 1u);
    BOOST_TEST(static_cast<unsigned>(second.GetRow()) == 2u);
    requireValues(second.GetSnapshot().values, snapshot().values);
    state.lockReasons[1].clear();
    second.UpdateSnapshot(state);
    const auto proposal = second.ProposeValue(true);
    BOOST_TEST_REQUIRE(proposal.has_value());
    BOOST_TEST(proposal->team == Team::Team4);
    requireValues(first.GetSnapshot().values, state.values);
    BOOST_TEST(first.GetLockReason() == "Campaign fixes this nation");
}

BOOST_AUTO_TEST_SUITE_END()
