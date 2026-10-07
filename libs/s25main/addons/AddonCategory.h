// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "const_addons.h"
#include <optional>

class Addon;

// UI metadata, independent of the serialized AddonGroup bits and addon selections.
enum class AddonCategory
{
    All,
    Comfort,
    Content,
    Economy,
    Combat,
    Easier,
    Developer
};

enum class AddonDifficulty
{
    Neutral,
    Easier,
    Harder
};

std::optional<AddonCategory> GetAddonCategory(AddonId id);
/// Only unambiguous resource/travel constraints are ranked, relative to the addon's default.
/// Combat balance, new mechanics and information changes have no universal difficulty ranking.
AddonDifficulty GetAddonDifficulty(const Addon& addon, unsigned status);
