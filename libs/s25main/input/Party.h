// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "driver/PadEvent.h"
#include "world/ViewportLayout.h"
#include <functional>
#include <optional>
#include <vector>

/// The controllers that joined on the title page, in join order (doc/coop/FrontEnd.md, F2).
///
/// Separate from PadRouter's slots on purpose: a menu page navigates with ONE slot (Desktop::GetNumPadSlots),
/// so the router forgets every other pad's slot on the next page. Who is playing must outlive that - the
/// joined-player strip shows it on every page, and the party screen (F7) seats exactly these players.
/// Member index = player number - 1 and colour index. Pure, like PadRouter: no driver, no window.
class Party
{
public:
    static constexpr unsigned MaxMembers = MAX_VIEWPORTS;

    /// The member index of `device`, joining it if needed. nullopt when the party is full.
    std::optional<unsigned> Join(PadDeviceId device);
    /// false if `device` was not a member. Later members move up one place.
    bool Leave(PadDeviceId device);
    bool Contains(PadDeviceId device) const;
    /// Member index or nullopt.
    std::optional<unsigned> IndexOf(PadDeviceId device) const;
    const std::vector<PadDeviceId>& Members() const { return members_; }
    bool IsEmpty() const { return members_.empty(); }
    void Clear() { members_.clear(); }
    /// Drop every member for which `present` is false (an unplugged controller leaves the party).
    void Retain(const std::function<bool(PadDeviceId)>& present);

private:
    std::vector<PadDeviceId> members_;
};
