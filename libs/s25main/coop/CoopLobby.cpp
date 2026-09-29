// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "coop/CoopLobby.h"
#include "GameLobby.h"
#include "JoinPlayerInfo.h"
#include "SavedFile.h"
#include "helpers/format.hpp"
#include <algorithm>
#include <mygettext/mygettext.h>

namespace coop::lobby {

unsigned countMembers(const std::vector<CoopMemberInfo>& members, unsigned leader)
{
    return static_cast<unsigned>(
      std::count_if(members.begin(), members.end(), [leader](const CoopMemberInfo& m) { return m.leader == leader; }));
}

std::string playerRowName(const std::string& name, const std::vector<CoopMemberInfo>& members, unsigned leader)
{
    const unsigned count = countMembers(members, leader);
    return count ? name + " +" + std::to_string(count) : name;
}

std::vector<unsigned> leaderCandidates(const GameLobby& lobby, unsigned localPlayer)
{
    std::vector<unsigned> result;
    result.reserve(lobby.getNumPlayers());
    for(unsigned i = 0; i < lobby.getNumPlayers(); i++)
    {
        if(i != localPlayer && lobby.getPlayer(i).ps == PlayerState::Occupied)
            result.push_back(i);
    }
    return result;
}

std::string memberLabel(const GameLobby& lobby, const CoopMemberInfo& member)
{
    if(member.leader >= lobby.getNumPlayers())
        return member.name;
    return helpers::format(_("%1% (with %2%)"), member.name, lobby.getPlayer(member.leader).name);
}

bool isSingleHumanSave(SavedFile& save)
{
    unsigned numHumans = 0;
    for(unsigned i = 0; i < save.GetNumPlayers(); i++)
        numHumans += save.GetPlayer(i).ps == PlayerState::Occupied ? 1 : 0;
    return numHumans == 1;
}

} // namespace coop::lobby
