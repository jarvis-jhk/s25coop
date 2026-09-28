// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <string>

/// s25coop: a connection that plays an existing player together with it, as the lobby shows it
/// (doc/coop/SharedPlayerSlot.md)
struct CoopMemberInfo
{
    /// Given by the server, unique for the lifetime of the server; the host kicks a member by it
    uint32_t id = 0;
    /// The player it plays together with
    uint8_t leader = 0;
    std::string name;
};
