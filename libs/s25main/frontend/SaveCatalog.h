// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "s25util/MyTime.h"
#include <boost/filesystem/path.hpp>
#include <optional>
#include <string>
#include <vector>

namespace frontend {

/// Metadata only: listing saves must not deserialize a world or alter a running game's RNG.
struct SaveEntry
{
    boost::filesystem::path path;
    std::string map;
    s25util::time64_t savedAt = 0;
    unsigned gameFrame = 0;
    unsigned humans = 0;
    unsigned ais = 0;
    std::vector<std::string> players;
};

struct SaveCatalog
{
    std::vector<SaveEntry> entries;
    unsigned rejected = 0;
};

/// Unreadable, incompatible or truncated metadata is an ordinary unavailable save.
std::optional<SaveEntry> ReadSaveEntry(const boost::filesystem::path& path);
/// Newest first (full save timestamp), path order for ties. Missing folders are empty.
SaveCatalog ScanSaves(const boost::filesystem::path& folder);

} // namespace frontend
