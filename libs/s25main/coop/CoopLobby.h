// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "network/CoopMemberInfo.h"
#include <string>
#include <vector>

class GameLobby;
class SavedFile;

/// What the game lobby shows about co-players (members, doc/coop/SharedPlayerSlot.md), kept apart from the desktop so
/// it can be tested without one.
namespace coop::lobby {

/// How many members play together with this player
unsigned countMembers(const std::vector<CoopMemberInfo>& members, unsigned leader);
/// The name in the player's row: its own, plus the number of co-players ("Jan +2")
std::string playerRowName(const std::string& name, const std::vector<CoopMemberInfo>& members, unsigned leader);
/// The players we may play together with: every human player but ourselves, as the server checks it
std::vector<unsigned> leaderCandidates(const GameLobby& lobby, unsigned localPlayer);
/// A member as the host's list shows it: "Anna (with Jan)"
std::string memberLabel(const GameLobby& lobby, const CoopMemberInfo& member);
/// A savegame with exactly one human player (a campaign, or a game played alone): whoever joins it can only be a
/// co-player of that player, so co-players are allowed from the start
bool isSingleHumanSave(SavedFile& save);

} // namespace coop::lobby
