// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ScenarioCatalog.h"
#include "helpers/containerUtils.h"
#include <stdexcept>

namespace frontend {

CampaignEntry ReadScenario(const boost::filesystem::path& folder)
{
    auto entry = ReadCampaign(folder);
    if(entry.description.getNumMaps() != 1)
        throw std::runtime_error("Scenario must contain exactly one mission");
    return entry;
}

CampaignCatalog ScanScenarios(const std::vector<boost::filesystem::path>& sources,
                              const std::optional<unsigned> players)
{
    auto catalog = ScanCampaigns(sources);
    helpers::erase_if(catalog.entries, [players](const CampaignEntry& entry) {
        return entry.description.getNumMaps() != 1 || (players && entry.maps.front().players != *players);
    });
    return catalog;
}

} // namespace frontend
