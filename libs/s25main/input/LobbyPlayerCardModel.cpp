// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "input/LobbyPlayerCardModel.h"
#include "helpers/containerUtils.h"
#include "s25util/colors.h"
#include <algorithm>
#include <utility>

namespace {
constexpr std::array nationOrder = {Nation::Romans, Nation::Vikings, Nation::Japanese, Nation::Africans,
                                    Nation::Babylonians};
constexpr std::array teamOrder = {Team::None,  Team::Random,     Team::Team1,      Team::Team2,     Team::Team3,
                                  Team::Team4, Team::Random1To2, Team::Random1To3, Team::Random1To4};

template<typename T, size_t N>
size_t nextIndex(const std::array<T, N>& values, T value, bool forward)
{
    const auto* const it = std::find(values.begin(), values.end(), value);
    if(it == values.end())
        return forward ? 0 : N - 1;
    const auto index = static_cast<size_t>(it - values.begin());
    return (index + (forward ? 1 : N - 1)) % N;
}
} // namespace

LobbyPlayerCardModel::LobbyPlayerCardModel(const unsigned seatId, Snapshot snapshot)
    : seatId_(seatId), snapshot_(std::move(snapshot))
{}

const std::string& LobbyPlayerCardModel::GetLockReason() const
{
    return snapshot_.lockReasons[static_cast<unsigned>(row_)];
}

bool LobbyPlayerCardModel::SelectRow(const Row row)
{
    if(static_cast<unsigned>(row) >= snapshot_.lockReasons.size())
        return false;
    row_ = row;
    return true;
}

bool LobbyPlayerCardModel::MoveRow(const bool forward)
{
    const auto index = static_cast<unsigned>(row_);
    if((forward && row_ == Row::SharedTribe) || (!forward && row_ == Row::Color))
        return false;
    return SelectRow(static_cast<Row>(forward ? index + 1 : index - 1));
}

void LobbyPlayerCardModel::UpdateSnapshot(Snapshot snapshot)
{
    snapshot_ = std::move(snapshot);
}

std::optional<LobbyPlayerCardModel::Values> LobbyPlayerCardModel::ProposeValue(const bool forward) const
{
    if(!GetLockReason().empty())
        return std::nullopt;
    Values proposal = snapshot_.values;
    if(row_ == Row::Color)
    {
        auto index = nextIndex(PLAYER_COLORS, proposal.color, forward);
        // Bound the search even when every palette colour is occupied or only our current one is free.
        for(size_t tried = 0; tried < PLAYER_COLORS.size(); ++tried)
        {
            const unsigned color = PLAYER_COLORS[index];
            if(color != proposal.color && !helpers::contains(snapshot_.takenColors, color))
            {
                proposal.color = color;
                return proposal;
            }
            index = (index + (forward ? 1 : PLAYER_COLORS.size() - 1)) % PLAYER_COLORS.size();
        }
        return std::nullopt;
    }
    if(row_ == Row::Nation)
        proposal.nation = nationOrder[nextIndex(nationOrder, proposal.nation, forward)];
    else if(row_ == Row::Team)
        proposal.team = teamOrder[nextIndex(teamOrder, proposal.team, forward)];
    else
        proposal.sharedTribe = !proposal.sharedTribe;
    return proposal;
}
