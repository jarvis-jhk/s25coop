// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <string>

/// s25coop: a connection that plays an existing player together with it, as the lobby shows it
/// (doc/coop/SharedPlayerSlot.md)
/// As the leader of a join request: whichever player the host plays. A joiner does not know the slots of a lobby it
/// has not seen yet, and in a campaign the host's player is the only human one.
constexpr uint8_t COOP_LEADER_HOST = 0xFE;

/// Why the server removed a member, told to that member just before its connection is closed
enum class CoopRemovedReason : uint8_t
{
    KickedByHost,
    LeaderLeft,
    OutOfSync,
    TooFarBehind
};
constexpr auto maxEnumValue(CoopRemovedReason)
{
    return CoopRemovedReason::TooFarBehind;
}

struct CoopMemberInfo
{
    /// Given by the server, unique for the lifetime of the server; the host kicks a member by it
    uint32_t id = 0;
    /// The player it plays together with
    uint8_t leader = 0;
    std::string name;
};
