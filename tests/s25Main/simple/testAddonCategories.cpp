// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GlobalGameSettings.h"
#include "addons/Addon.h"
#include "addons/AddonCategory.h"
#include <boost/test/unit_test.hpp>
#include <array>
#include <set>
#include <vector>

BOOST_AUTO_TEST_SUITE(AddonCategoryTests)

BOOST_AUTO_TEST_CASE(EveryRegisteredAddonHasOneCategoryAndDefaultIsNeutral)
{
    GlobalGameSettings settings;
    std::set<AddonId> ids;
    std::array<unsigned, 7> counts{};
    for(unsigned i = 0; i < settings.getNumAddons(); ++i)
    {
        const auto& addon = *settings.getAddon(i);
        BOOST_TEST(ids.insert(addon.getId()).second);
        const auto category = GetAddonCategory(addon.getId());
        BOOST_TEST_REQUIRE(category.has_value());
        ++counts[static_cast<unsigned>(*category)];
        BOOST_TEST(static_cast<unsigned>(GetAddonDifficulty(addon, addon.getDefaultStatus())) == 0u);
        BOOST_TEST(static_cast<unsigned>(GetAddonDifficulty(addon, addon.getNumOptions())) == 0u);
        for(unsigned value = 0; value < addon.getNumOptions(); ++value)
            BOOST_TEST(static_cast<unsigned>(GetAddonDifficulty(addon, value)) <= 2u);
    }
    BOOST_TEST(settings.getNumAddons() == rttrEnum::size<AddonId>);
    for(const auto id : rttrEnum::values<AddonId>)
        BOOST_TEST(ids.count(id) == 1u);
    BOOST_TEST(counts[0] == 0u);
    for(unsigned i = 1; i < counts.size(); ++i)
        BOOST_TEST(counts[i] > 0u);
    BOOST_TEST(!GetAddonCategory(static_cast<AddonId>(0xFFFFFFFF)).has_value());
}

BOOST_AUTO_TEST_CASE(EffectsFollowTheActualResourceAndTravelOptionOrder)
{
    GlobalGameSettings settings;
    const auto check = [&](AddonId id, const std::vector<AddonDifficulty>& expected) {
        bool found = false;
        for(unsigned i = 0; i < settings.getNumAddons(); ++i)
        {
            const auto& addon = *settings.getAddon(i);
            if(addon.getId() != id)
                continue;
            found = true;
            BOOST_TEST_REQUIRE(addon.getNumOptions() == expected.size());
            for(unsigned value = 0; value < expected.size(); ++value)
                BOOST_TEST(static_cast<unsigned>(GetAddonDifficulty(addon, value))
                           == static_cast<unsigned>(expected[value]));
        }
        BOOST_TEST_REQUIRE(found);
    };
    using D = AddonDifficulty;
    check(AddonId::EXHAUSTIBLE_WATER, {D::Neutral, D::Easier, D::Harder});
    check(AddonId::NUM_SCOUTS_EXPLORATION, {D::Easier, D::Easier, D::Neutral, D::Harder, D::Harder});
    check(AddonId::SHIP_SPEED, {D::Harder, D::Harder, D::Neutral, D::Easier, D::Easier});
    check(AddonId::MAX_WATERWAY_LENGTH, {D::Harder, D::Neutral, D::Easier, D::Easier, D::Easier, D::Easier});
    check(AddonId::BURN_DURATION, {D::Neutral, D::Easier, D::Easier, D::Easier, D::Easier, D::Harder, D::Harder});
    check(AddonId::SEA_ATTACK, {D::Neutral, D::Neutral, D::Neutral});
    check(AddonId::ADJUST_MILITARY_STRENGTH, {D::Neutral, D::Neutral, D::Neutral});
    check(AddonId::AI_DEBUG_WINDOW, {D::Neutral, D::Neutral});
    for(const auto id : {AddonId::INEXHAUSTIBLE_MINES, AddonId::INEXHAUSTIBLE_GRANITEMINES, AddonId::INEXHAUSTIBLE_FISH,
                         AddonId::REFUND_ON_EMERGENCY, AddonId::HALF_COST_MIL_EQUIP, AddonId::PEACEFULMODE})
        check(id, {D::Neutral, D::Easier});
    check(AddonId::REFUND_MATERIALS, {D::Neutral, D::Easier, D::Easier, D::Easier, D::Easier});
    for(const auto id : {AddonId::MORE_ANIMALS, AddonId::FORESTER_REACH_RADIUS, AddonId::WOODCUTTER_REACH_RADIUS})
        check(id, {D::Neutral, D::Easier, D::Easier, D::Easier, D::Easier, D::Easier});
    check(AddonId::STONEMASON_REACH_RADIUS,
          {D::Neutral, D::Easier, D::Easier, D::Easier, D::Easier, D::Easier, D::Easier});
}

BOOST_AUTO_TEST_SUITE_END()
