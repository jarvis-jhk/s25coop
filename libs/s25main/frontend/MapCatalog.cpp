// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MapCatalog.h"
#include "helpers/containerUtils.h"
#include "gameData/MapConsts.h"
#include "gameData/MaxPlayers.h"
#include "libsiedler2/ArchivItem_Map.h"
#include "libsiedler2/ArchivItem_Map_Header.h"
#include "libsiedler2/ErrorCodes.h"
#include "libsiedler2/prototypen.h"
#include "s25util/strAlgos.h"
#include "s25util/utf8.h"
#include <boost/filesystem/operations.hpp>
#include <set>
#include <stdexcept>

namespace frontend {

MapEntry ReadMapEntry(const boost::filesystem::path& path)
{
    libsiedler2::Archiv archive;
    if(const int error = libsiedler2::loader::LoadMAP(path, archive, true))
        throw std::runtime_error(libsiedler2::getErrorString(error));
    const auto* map = dynamic_cast<const libsiedler2::ArchivItem_Map*>(archive[0]);
    if(!map)
        throw std::runtime_error("Unexpected dynamic type of map");
    const auto& header = map->getHeader();
    if(header.getWidth() == 0 || header.getHeight() == 0 || header.getWidth() > MAX_MAP_SIZE
       || header.getHeight() > MAX_MAP_SIZE)
        throw std::runtime_error("Unsupported map dimensions");
    if(header.getNumPlayers() == 0 || header.getNumPlayers() > MAX_PLAYERS)
        throw std::runtime_error("Unsupported map player count");
    const auto script = boost::filesystem::path(path).replace_extension("lua");
    boost::system::error_code scriptError;
    const bool hasScript = boost::filesystem::is_regular_file(script, scriptError);
    return {path,
            s25util::ansiToUTF8(header.getName()),
            s25util::ansiToUTF8(header.getAuthor()),
            header.getNumPlayers(),
            header.getWidth(),
            header.getHeight(),
            header.getGfxSet(),
            hasScript};
}

MapCatalog ScanMaps(const std::vector<boost::filesystem::path>& folders, const std::optional<unsigned> players)
{
    MapCatalog catalog;
    std::set<boost::filesystem::path> seen;
    for(const auto& folder : folders)
    {
        std::vector<boost::filesystem::path> paths;
        boost::system::error_code error;
        boost::filesystem::directory_iterator it(folder, error), end;
        if(error)
        {
            if(error != boost::system::errc::no_such_file_or_directory)
                catalog.failures.push_back({folder, error.message()});
            continue;
        }
        for(; it != end; it.increment(error))
        {
            if(error)
                break;
            const auto extension = s25util::toLower(it->path().extension().string());
            if(extension != ".swd" && extension != ".wld")
                continue;
            // Inspect only candidate maps: a broken unrelated symlink must not hide this folder.
            boost::system::error_code statusError;
            if(!boost::filesystem::is_directory(it->path(), statusError))
                paths.push_back(it->path());
        }
        if(error)
            catalog.failures.push_back({folder, error.message()});
        for(const auto& path : paths)
        {
            const auto identity = path.lexically_normal();
            if(!seen.insert(identity).second)
                continue;
            try
            {
                auto entry = ReadMapEntry(identity);
                if(!players || entry.players == *players)
                    catalog.entries.push_back(std::move(entry));
            } catch(const std::runtime_error& error)
            {
                catalog.failures.push_back({identity, error.what()});
            }
        }
    }
    helpers::sort(catalog.entries, [](const MapEntry& a, const MapEntry& b) {
        return a.name != b.name ? a.name < b.name : a.path < b.path;
    });
    return catalog;
}

} // namespace frontend
