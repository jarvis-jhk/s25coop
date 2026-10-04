// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "gameTypes/Nation.h"
#include "gameTypes/TeamTypes.h"
#include <array>
#include <optional>
#include <string>
#include <vector>

/// Pure per-seat navigation and value proposals for MenuRedesign slice 1a.
/// No slot ownership, campaign policy, windows, controller devices or network commands live here.
class LobbyPlayerCardModel
{
public:
    enum class Row : unsigned
    {
        Color,
        Nation,
        Team,
        SharedTribe
    };
    struct Values
    {
        unsigned color; ///< Actual ARGB player colour, not a palette index or a cursor colour.
        ::Nation nation;
        ::Team team;
        bool sharedTribe;
    };
    struct Snapshot
    {
        Values values;
        /// Colours used by other occupied/AI slots; exclude this seat's slot (and its shared views).
        std::vector<unsigned> takenColors;
        /// Empty = editable. Otherwise the shell supplies a displayable campaign/ownership reason.
        /// Locked rows remain focusable so the player can read their reason.
        std::array<std::string, 4> lockReasons;
    };

    LobbyPlayerCardModel(unsigned seatId, Snapshot snapshot);
    unsigned GetSeatId() const { return seatId_; }
    Row GetRow() const { return row_; }
    const Snapshot& GetSnapshot() const { return snapshot_; }
    const std::string& GetLockReason() const;
    /// Stable row identity; reject invalid ids. Vertical movement clamps at the ends.
    bool SelectRow(Row row);
    bool MoveRow(bool forward);
    /// Replace authoritative values/policy, retaining this seat's focus, including a newly locked row.
    /// The caller must resolve any pending UI edit before supplying a replacement snapshot.
    void UpdateSnapshot(Snapshot snapshot);
    /// A horizontal step proposes one changed value, never mutating the authoritative snapshot.
    /// Null means locked or no free colour. The shell owns acceptance, server authority and refresh.
    /// On acceptance revalidate current permissions and apply only the selected row's field;
    /// a retained proposal must never overwrite unrelated values from a newer broadcast.
    std::optional<Values> ProposeValue(bool forward) const;

private:
    unsigned seatId_;
    Row row_ = Row::Color;
    Snapshot snapshot_;
};
