// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TestInput.h"
#include "GamePlayer.h"
#include "lua/LuaHelpers.h"
#include "pathfinding/FindPathForRoad.h"
#include "world/GameWorld.h"
#include "gameData/BuildingConsts.h"
#include <kaguya/kaguya.hpp>

TestInput::TestInput(const GameWorld& world) : world_(world), queues_(world.GetNumPlayers()) {}

void TestInput::Register(kaguya::State& state)
{
    state["TestInput"].setClass(kaguya::UserdataMetatable<TestInput>()
                                  .addFunction("SetBuildingSite", &TestInput::SetBuildingSite)
                                  .addFunction("DestroyBuilding", &TestInput::DestroyBuilding)
                                  .addFunction("SetFlag", &TestInput::SetFlag)
                                  .addFunction("DestroyFlag", &TestInput::DestroyFlag)
                                  .addFunction("BuildRoad", &TestInput::BuildRoad)
                                  .addFunction("ConnectFlags", &TestInput::ConnectFlags)
                                  .addFunction("Attack", &TestInput::Attack)
                                  .addFunction("GetFlagPos", &TestInput::GetFlagPos)
                                  .addFunction("FindBuildingSpot", &TestInput::FindBuildingSpot));
}

std::vector<gc::GameCommandPtr> TestInput::FetchGameCommands(unsigned playerId)
{
    std::vector<gc::GameCommandPtr> result;
    std::swap(result, queues_.at(playerId).gcs);
    return result;
}

TestInput::Queue& TestInput::queueFor(unsigned player)
{
    lua::assertTrue(player < queues_.size(), "test: invalid player " + std::to_string(player));
    return queues_[player];
}

MapPoint TestInput::toPoint(unsigned x, unsigned y) const
{
    lua::assertTrue(x < world_.GetWidth() && y < world_.GetHeight(),
                    "test: point " + std::to_string(x) + "," + std::to_string(y) + " is outside the map");
    return MapPoint(x, y);
}

bool TestInput::SetBuildingSite(unsigned player, unsigned x, unsigned y, lua::SafeEnum<BuildingType> bld)
{
    return queueFor(player).SetBuildingSite(toPoint(x, y), bld);
}

bool TestInput::DestroyBuilding(unsigned player, unsigned x, unsigned y)
{
    return queueFor(player).DestroyBuilding(toPoint(x, y));
}

bool TestInput::SetFlag(unsigned player, unsigned x, unsigned y)
{
    return queueFor(player).SetFlag(toPoint(x, y));
}

bool TestInput::DestroyFlag(unsigned player, unsigned x, unsigned y)
{
    return queueFor(player).DestroyFlag(toPoint(x, y));
}

bool TestInput::BuildRoad(unsigned player, unsigned x, unsigned y, const std::string& route)
{
    Queue& queue = queueFor(player);
    const MapPoint start = toPoint(x, y);
    lua::assertTrue(!route.empty(), "test: empty road route");
    std::vector<Direction> dirs;
    dirs.reserve(route.size());
    for(const char c : route)
    {
        lua::assertTrue(c >= '0' && c <= '5', "test: road route must be digits 0-5, got '" + route + "'");
        dirs.push_back(static_cast<Direction>(c - '0'));
    }
    return queue.BuildRoad(start, false, dirs);
}

bool TestInput::ConnectFlags(unsigned player, unsigned x1, unsigned y1, unsigned x2, unsigned y2)
{
    Queue& queue = queueFor(player);
    const MapPoint start = toPoint(x1, y1);
    const std::vector<Direction> route = FindPathForRoad(world_, start, toPoint(x2, y2), false);
    if(route.empty())
        return false;
    return queue.BuildRoad(start, false, route);
}

bool TestInput::Attack(unsigned player, unsigned x, unsigned y, unsigned soldiers, bool strong)
{
    return queueFor(player).Attack(toPoint(x, y), soldiers, strong);
}

std::tuple<unsigned, unsigned> TestInput::GetFlagPos(unsigned x, unsigned y) const
{
    const MapPoint flag = world_.GetNeighbour(toPoint(x, y), Direction::SouthEast);
    return {flag.x, flag.y};
}

std::tuple<int, int> TestInput::FindBuildingSpot(unsigned player, lua::SafeEnum<BuildingType> bld, unsigned x,
                                                 unsigned y, unsigned radius) const
{
    lua::assertTrue(player < queues_.size(), "test: invalid player " + std::to_string(player));
    const MapPoint center = toPoint(x, y);
    const BuildingQuality size = BUILDING_SIZE[bld];
    std::tuple<int, int> best{-1, -1};
    unsigned bestDistance = radius + 1;
    // Nearest by map distance; among equally near spots the first in walk order, so a map always gives the same one
    for(const MapPoint pt : world_.GetPointsInRadiusWithCenter(center, radius))
    {
        if(!canUseBq(world_.GetBQ(pt, player), size))
            continue;
        const unsigned distance = world_.CalcDistance(center, pt);
        if(distance < bestDistance)
        {
            bestDistance = distance;
            best = {pt.x, pt.y};
        }
    }
    return best;
}
