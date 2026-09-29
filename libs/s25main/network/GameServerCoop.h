// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "AsyncChecksum.h"
#include "GameMessage.h"
#include "GameServer.h"
#include "GameServerPlayer.h"
#include <deque>
#include <string>

/// A connection that plays an existing world player (its leader) together with it. connection.playerId is the leader
/// once the join was accepted, NO_PLAYER_ID before; it is never a slot of its own (doc/coop/SharedPlayerSlot.md)
struct GameServer::CoopMember
{
    GameServerPlayer connection;
    /// What the lobby shows and the host kicks by (CoopMemberInfo::id)
    uint32_t id;
    std::string name;
    /// Handshake steps passed; the map is only sent after both, as to a player
    bool versionOk = false, passwordOk = false;
    /// Checksums the member sent with its command sets (one per NWF, like every client) that were not compared with its
    /// leader's yet; the first one is that of NWF number nextChecksumIdx, counted from the game's start
    std::deque<AsyncChecksum> checksums;
    unsigned nextChecksumIdx = 0;

    CoopMember(const Socket& socket, uint32_t id) : connection(GameMessageWithPlayer::NO_PLAYER_ID, socket), id(id) {}
    // Held by unique_ptr and never moved (see GameServer::coopMembers_)
    CoopMember(const CoopMember&) = delete;
    CoopMember& operator=(const CoopMember&) = delete;
    bool hasJoined() const { return connection.playerId != GameMessageWithPlayer::NO_PLAYER_ID; }
};
