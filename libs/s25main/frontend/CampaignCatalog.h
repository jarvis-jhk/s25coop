// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "MapCatalog.h"
#include "gameData/CampaignDescription.h"
#include <boost/filesystem/path.hpp>
#include <string>
#include <vector>

namespace frontend {

struct CampaignEntry
{
    boost::filesystem::path folder;
    CampaignDescription description;
    /// Validated headers in mission order; discovery consumers must not reread outside isolation.
    std::vector<MapEntry> maps;
};

struct CampaignFailure
{
    boost::filesystem::path path;
    std::string reason;
};

struct CampaignCatalog
{
    std::vector<CampaignEntry> entries;
    std::vector<CampaignFailure> failures;
};

/// Loads campaign.lua using the existing campaign interface; mission scripts are never executed.
/// Every mission needs a supported map header and a regular Lua file. Artwork is optional.
/// Throws runtime_error for an unavailable campaign; full map validation remains a start-time check.
CampaignEntry ReadCampaign(const boost::filesystem::path& folder);
/// Immediate subdirectories only, deduplicated by lexical folder identity, sorted by name then path.
/// A broken campaign or directory does not hide the other campaigns. Missing sources are empty.
CampaignCatalog ScanCampaigns(const std::vector<boost::filesystem::path>& sources);

} // namespace frontend
