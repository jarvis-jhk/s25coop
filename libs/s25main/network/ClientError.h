// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later
//

#pragma once

/// Fehler, die vom Client gemeldet werden
enum class ClientError
{
    InvalidMessage,
    ServerFull,
    WrongPassword,
    ConnectionLost,
    InvalidServerType,
    MapTransmission,
    WrongVersion,
    InvalidMap,
    /// s25coop: asked to join a player as a co-player, but the host does not allow it (or that player cannot be joined)
    CoopRefused,
    /// s25coop: removed as a co-player by the server (GameMessage_Coop_Removed says why)
    CoopKicked,
    CoopLeaderLeft,
    CoopOutOfSync,
    CoopTooFarBehind
};

const char* ClientErrorToStr(ClientError error);
