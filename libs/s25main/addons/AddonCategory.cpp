// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AddonCategory.h"
#include "Addon.h"

std::optional<AddonCategory> GetAddonCategory(const AddonId id)
{
    switch(id)
    {
        case AddonId::MANUAL_ROAD_ENLARGEMENT:
        case AddonId::CATAPULT_GRAPHICS:
        case AddonId::METALWORKSBEHAVIORONZERO:
        case AddonId::CUSTOM_BUILD_SEQUENCE:
        case AddonId::NO_COINS_DEFAULT:
        case AddonId::TOOL_ORDERING:
        case AddonId::MILITARY_AID:
        case AddonId::NO_ALLIED_PUSH:
        case AddonId::MILITARY_CONTROL:
        case AddonId::MILITARY_HITPOINTS:
        case AddonId::FRONTIER_DISTANCE_REACHABLE:
        case AddonId::COINS_CAPTURED_BLD:
        case AddonId::DEMOLISH_BLD_WO_RES:
        case AddonId::DURABLE_GEOLOGIST_SIGNS:
        case AddonId::AUTOFLAGS:
        case AddonId::NO_ARMOR_DEFAULT:
        case AddonId::ARMOR_CAPTURED_BLD:
        case AddonId::FORESTER_FARM_FIELD_AVOIDANCE:
        case AddonId::STRANDED_SOLDIER_RETURN_SEARCH: return AddonCategory::Comfort;
        case AddonId::CHARBURNER:
        case AddonId::TRADE:
        case AddonId::WINE:
        case AddonId::LEATHER: return AddonCategory::Content;
        case AddonId::EXHAUSTIBLE_WATER:
        case AddonId::CHANGE_GOLD_DEPOSITS:
        case AddonId::MAX_WATERWAY_LENGTH:
        case AddonId::BURN_DURATION:
        case AddonId::SHIP_SPEED:
        case AddonId::NUM_SCOUTS_EXPLORATION:
        case AddonId::ECONOMY_MODE_GAME_LENGTH:
        case AddonId::GRANITEMINE_RESOURCE_BEHAVIOR:
        case AddonId::COALMINE_RESOURCE_BEHAVIOR:
        case AddonId::IRONMINE_RESOURCE_BEHAVIOR:
        case AddonId::GOLDMINE_RESOURCE_BEHAVIOR:
        case AddonId::MINE_NO_OUTPUT_FALLBACK: return AddonCategory::Economy;
        case AddonId::LIMIT_CATAPULTS:
        case AddonId::DEMOLITION_PROHIBITION:
        case AddonId::STATISTICS_VISIBILITY:
        case AddonId::DEFENDER_BEHAVIOR:
        case AddonId::ADJUST_MILITARY_STRENGTH:
        case AddonId::MAX_RANK:
        case AddonId::SEA_ATTACK:
        case AddonId::BATTLEFIELD_PROMOTION:
        case AddonId::SINGLE_SOLDIER_COIN_TRAINING: return AddonCategory::Combat;
        case AddonId::INEXHAUSTIBLE_MINES:
        case AddonId::REFUND_MATERIALS:
        case AddonId::REFUND_ON_EMERGENCY:
        case AddonId::INEXHAUSTIBLE_FISH:
        case AddonId::MORE_ANIMALS:
        case AddonId::HALF_COST_MIL_EQUIP:
        case AddonId::PEACEFULMODE:
        case AddonId::FORESTER_REACH_RADIUS:
        case AddonId::WOODCUTTER_REACH_RADIUS:
        case AddonId::STONEMASON_REACH_RADIUS: return AddonCategory::Easier;
        case AddonId::AI_DEBUG_WINDOW: return AddonCategory::Developer;
    }
    return std::nullopt;
}

AddonDifficulty GetAddonDifficulty(const Addon& addon, const unsigned status)
{
    if(status >= addon.getNumOptions() || status == addon.getDefaultStatus())
        return AddonDifficulty::Neutral;
    switch(addon.getId())
    {
        case AddonId::EXHAUSTIBLE_WATER: return status == 1 ? AddonDifficulty::Easier : AddonDifficulty::Harder;
        case AddonId::NUM_SCOUTS_EXPLORATION:
            return status < addon.getDefaultStatus() ? AddonDifficulty::Easier : AddonDifficulty::Harder;
        case AddonId::SHIP_SPEED:
        case AddonId::MAX_WATERWAY_LENGTH:
            return status > addon.getDefaultStatus() ? AddonDifficulty::Easier : AddonDifficulty::Harder;
        case AddonId::BURN_DURATION: return status <= 4 ? AddonDifficulty::Easier : AddonDifficulty::Harder;
        // Default / Inexhaustible / S4-like exhaustion / Work everywhere
        case AddonId::GRANITEMINE_RESOURCE_BEHAVIOR:
        case AddonId::COALMINE_RESOURCE_BEHAVIOR:
        case AddonId::IRONMINE_RESOURCE_BEHAVIOR:
        case AddonId::GOLDMINE_RESOURCE_BEHAVIOR:
            return status == 2 ? AddonDifficulty::Harder : AddonDifficulty::Easier;
        case AddonId::SINGLE_SOLDIER_COIN_TRAINING: return AddonDifficulty::Harder;
        case AddonId::INEXHAUSTIBLE_MINES:
        case AddonId::REFUND_MATERIALS:
        case AddonId::REFUND_ON_EMERGENCY:
        case AddonId::MINE_NO_OUTPUT_FALLBACK:
        case AddonId::INEXHAUSTIBLE_FISH:
        case AddonId::MORE_ANIMALS:
        case AddonId::HALF_COST_MIL_EQUIP:
        case AddonId::PEACEFULMODE:
        case AddonId::FORESTER_REACH_RADIUS:
        case AddonId::WOODCUTTER_REACH_RADIUS:
        case AddonId::STONEMASON_REACH_RADIUS: return AddonDifficulty::Easier;
        default: return AddonDifficulty::Neutral;
    }
}
