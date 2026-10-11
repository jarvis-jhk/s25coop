// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "CampaignCatalog.h"
#include "MapCatalog.h"
#include "helpers/containerUtils.h"
#include "lua/CampaignDataLoader.h"
#include <boost/filesystem/directory.hpp>
#include <boost/filesystem/operations.hpp>
#include <set>
#include <stdexcept>

namespace frontend {

CampaignEntry ReadCampaign(const boost::filesystem::path& folder)
{
    CampaignEntry entry{folder.lexically_normal(), {}};
    if(!CampaignDataLoader(entry.description, entry.folder).Load())
        throw std::runtime_error("Invalid campaign description");
    if(entry.description.getNumMaps() == 0)
        throw std::runtime_error("Campaign has no missions");
    for(unsigned i = 0; i < entry.description.getNumMaps(); ++i)
    {
        ReadMapEntry(entry.description.getMapFilePath(i));
        boost::system::error_code error;
        const auto script = entry.description.getLuaFilePath(i);
        if(!boost::filesystem::is_regular_file(script, error))
            throw std::runtime_error("Mission script unavailable: " + script.string());
    }
    return entry;
}

CampaignCatalog ScanCampaigns(const std::vector<boost::filesystem::path>& sources)
{
    CampaignCatalog catalog;
    std::set<boost::filesystem::path> seen;
    for(const auto& source : sources)
    {
        boost::system::error_code error;
        boost::filesystem::directory_iterator it(source, error), end;
        if(error)
        {
            if(error != boost::system::errc::no_such_file_or_directory)
                catalog.failures.push_back({source, error.message()});
            continue;
        }
        for(; it != end; it.increment(error))
        {
            if(error)
                break;
            boost::system::error_code statusError;
            const bool directory = boost::filesystem::is_directory(it->path(), statusError);
            if(statusError)
            {
                catalog.failures.push_back({it->path(), statusError.message()});
                continue;
            }
            if(!directory || !seen.insert(it->path().lexically_normal()).second)
                continue;
            try
            {
                catalog.entries.push_back(ReadCampaign(it->path()));
            } catch(const std::runtime_error& e)
            {
                catalog.failures.push_back({it->path(), e.what()});
            }
        }
        if(error)
            catalog.failures.push_back({source, error.message()});
    }
    helpers::sort(catalog.entries, [](const CampaignEntry& a, const CampaignEntry& b) {
        return a.description.name != b.description.name ? a.description.name < b.description.name : a.folder < b.folder;
    });
    return catalog;
}

} // namespace frontend
