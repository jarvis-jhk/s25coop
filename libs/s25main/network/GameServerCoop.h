// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "GameMessage.h"
#include "GameServer.h"
#include "GameServerPlayer.h"
#include <string>

/// A connection that plays an existing world player (its leader) together with it. connection.playerId is the leader
/// once the join was accepted, NO_PLAYER_ID before; it is never a slot of its own (doc/coop/SharedPlayerSlot.md)
struct GameServer::CoopMember
{
    GameServerPlayer connection;
    std::string name;
    /// Handshake steps passed; the map is only sent after both, as to a player
    bool versionOk = false, passwordOk = false;

    explicit CoopMember(const Socket& socket) : connection(GameMessageWithPlayer::NO_PLAYER_ID, socket) {}
    bool hasJoined() const { return connection.playerId != GameMessageWithPlayer::NO_PLAYER_ID; }
};
