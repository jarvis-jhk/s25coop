// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "IngameWindow.h"
#include "coop/Changelog.h"
#include <vector>

/// s25coop: shows CHANGELOG.md sections, after an update or from the main menu
class iwChangelog : public IngameWindow
{
public:
    /// Shows @p sections, or the whole changelog if empty
    explicit iwChangelog(const std::vector<coop::changelog::Section>& sections = {});
};
