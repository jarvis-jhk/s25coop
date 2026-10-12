// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "CampaignCatalog.h"
#include <optional>

namespace frontend {

/// A scenario is explicitly registered through campaign.lua and has exactly one mission.
/// A neighboring map script never creates a campaign identity or unlock state.
CampaignEntry ReadScenario(const boost::filesystem::path& folder);
/// Same sources, identity, metadata-only validation and isolation as the campaign catalog.
/// Multi-mission campaigns and maps outside the exact tribe filter are omitted, not failures.
CampaignCatalog ScanScenarios(const std::vector<boost::filesystem::path>& sources,
                              std::optional<unsigned> players = std::nullopt);

} // namespace frontend
