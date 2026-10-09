// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "SaveCatalog.h"
#include "BasePlayerInfo.h"
#include "ListDir.h"
#include "Savegame.h"
#include "helpers/containerUtils.h"
#include <algorithm>
#include <exception>

namespace frontend {

std::optional<SaveEntry> ReadSaveEntry(const boost::filesystem::path& path)
{
    try
    {
        Savegame save;
        if(!save.Load(path, SaveGameDataToLoad::HeaderAndSettings))
            return std::nullopt;
        SaveEntry entry;
        entry.path = path;
        entry.map = save.GetMapName();
        entry.savedAt = save.GetSaveTime();
        entry.gameFrame = save.start_gf;
        for(unsigned i = 0; i < save.GetNumPlayers(); ++i)
        {
            const BasePlayerInfo& player = save.GetPlayer(i);
            if(player.isUsed())
                entry.players.push_back(player.name);
            if(player.isHuman())
                ++entry.humans;
            else if(player.ps == PlayerState::AI)
                ++entry.ais;
        }
        return entry;
    } catch(const std::exception&)
    {
        // Some malformed metadata throws logic_error (e.g. too many players), outside Savegame's
        // runtime_error handler. One bad file must not prevent browsing the other saves.
        return std::nullopt;
    }
}

SaveCatalog ScanSaves(const boost::filesystem::path& folder)
{
    SaveCatalog catalog;
    for(const auto& path : ListDir(folder, "sav"))
    {
        if(auto entry = ReadSaveEntry(path))
            catalog.entries.push_back(std::move(*entry));
        else
            ++catalog.rejected;
    }
    helpers::sort(catalog.entries, [](const SaveEntry& a, const SaveEntry& b) {
        return a.savedAt != b.savedAt ? a.savedAt > b.savedAt : a.path < b.path;
    });
    return catalog;
}

} // namespace frontend
