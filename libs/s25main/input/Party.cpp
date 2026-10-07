// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "input/Party.h"
#include <algorithm>

std::optional<unsigned> Party::Join(const PadDeviceId device)
{
    if(const auto index = IndexOf(device))
        return index;
    if(members_.size() >= MaxMembers)
        return std::nullopt;
    members_.push_back(device);
    return static_cast<unsigned>(members_.size() - 1);
}

bool Party::Leave(const PadDeviceId device)
{
    const auto it = std::find(members_.begin(), members_.end(), device);
    if(it == members_.end())
        return false;
    members_.erase(it);
    return true;
}

bool Party::Contains(const PadDeviceId device) const
{
    return IndexOf(device).has_value();
}

std::optional<unsigned> Party::IndexOf(const PadDeviceId device) const
{
    const auto it = std::find(members_.begin(), members_.end(), device);
    if(it == members_.end())
        return std::nullopt;
    return static_cast<unsigned>(it - members_.begin());
}

void Party::Retain(const std::function<bool(PadDeviceId)>& present)
{
    members_.erase(std::remove_if(members_.begin(), members_.end(), [&present](PadDeviceId d) { return !present(d); }),
                   members_.end());
}
