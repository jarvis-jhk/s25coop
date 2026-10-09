// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <boost/filesystem/path.hpp>
#include <optional>
#include <string>
#include <vector>

namespace frontend {

/// Header metadata only. A neighboring script does not establish campaign membership or unlocks.
struct MapEntry
{
    boost::filesystem::path path;
    std::string name;
    std::string author;
    unsigned players = 0;
    unsigned width = 0;
    unsigned height = 0;
    unsigned landscape = 0;
    bool hasScript = false;
};

struct MapCatalogFailure
{
    boost::filesystem::path path;
    std::string reason;
};

struct MapCatalog
{
    std::vector<MapEntry> entries;
    std::vector<MapCatalogFailure> failures;
};

/// Throws runtime_error for unreadable headers or unsupported sizes/player counts.
/// Full map validation remains necessary on selection; no layers or Lua are loaded here.
MapEntry ReadMapEntry(const boost::filesystem::path& path);
/// Nonrecursive SWD/WLD discovery (case insensitive), name then path order. Missing folders are empty.
/// Lexically identical paths across folders are read once. A player filter is an exact tribe count,
/// independent of the number of controllers sharing a tribe. Filtered maps are not failures.
MapCatalog ScanMaps(const std::vector<boost::filesystem::path>& folders,
                    std::optional<unsigned> players = std::nullopt);

} // namespace frontend
