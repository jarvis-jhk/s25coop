// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "factories/GameCommandFactory.h"
#include "lua/SafeEnum.h"
#include "gameTypes/BuildingType.h"
#include <string>
#include <tuple>
#include <vector>

class GameWorld;
namespace kaguya {
class State;
}

/// s25coop test mode (ROADMAP M0.5): lets a test script issue player commands, as the global `test`.
/// Commands are queued per player and handed to HeadlessGame at the next network frame, where they run through
/// the same path as an AI's or a network player's commands (executed with them, recorded in a replay).
class TestInput
{
public:
    explicit TestInput(const GameWorld& world);

    static void Register(kaguya::State& state);

    /// Commands queued for this player since the last call; the queue is empty afterwards
    std::vector<gc::GameCommandPtr> FetchGameCommands(unsigned playerId);

    // Lua API. Coordinates are map coordinates, as in the map editor and rttr:GetPlayer(i):GetHQPos().
    // Every command returns true when it was queued; whether the game accepts it is decided when it runs.
    bool SetBuildingSite(unsigned player, unsigned x, unsigned y, lua::SafeEnum<BuildingType> bld);
    bool DestroyBuilding(unsigned player, unsigned x, unsigned y);
    bool SetFlag(unsigned player, unsigned x, unsigned y);
    bool DestroyFlag(unsigned player, unsigned x, unsigned y);
    /// Road from the flag at x,y along a route of direction digits (0 = west, 1 = north-west, 2 = north-east,
    /// 3 = east, 4 = south-east, 5 = south-west), e.g. "3334"
    bool BuildRoad(unsigned player, unsigned x, unsigned y, const std::string& route);
    /// Road between two flags along a path found now, on the current map; false if there is none.
    /// A flag or building site queued in the same frame does not exist yet: connect it one frame later.
    bool ConnectFlags(unsigned player, unsigned x1, unsigned y1, unsigned x2, unsigned y2);
    bool Attack(unsigned player, unsigned x, unsigned y, unsigned soldiers, bool strong);

    // Queries that commands need, answered from the current map
    /// Map position of the flag in front of a building spot
    std::tuple<unsigned, unsigned> GetFlagPos(unsigned x, unsigned y) const;
    /// Nearest spot to x,y (within radius) where the player may place this building now, or -1, -1
    std::tuple<int, int> FindBuildingSpot(unsigned player, lua::SafeEnum<BuildingType> bld, unsigned x, unsigned y,
                                          unsigned radius) const;

private:
    struct Queue : GameCommandFactory
    {
        std::vector<gc::GameCommandPtr> gcs;
        bool AddGC(gc::GameCommandPtr gc) override
        {
            gcs.push_back(std::move(gc));
            return true;
        }
    };

    Queue& queueFor(unsigned player);
    MapPoint toPoint(unsigned x, unsigned y) const;

    const GameWorld& world_;
    std::vector<Queue> queues_;
};
