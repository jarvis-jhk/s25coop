// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <iosfwd>
#include <string>
#include <vector>

/// The player-facing CHANGELOG.md, shown in-game after an update.
namespace coop::changelog {

struct Section
{
    std::string version;
    /// Entries of this version, one string per bullet (continuation lines joined, markdown stripped)
    std::vector<std::string> entries;
};

/// True for release versions ("0.1.1"); false for dev builds, whose version is a date stamp.
bool isReleaseVersion(const std::string& version);
/// <0, 0, >0 like strcmp, comparing dotted numbers ("0.10.0" > "0.9.3"). Both must be release versions.
int compareVersions(const std::string& lhs, const std::string& rhs);

/// Parses "## <version>" sections, newest first as in the file. Text before the first section is ignored.
std::vector<Section> parse(std::istream& in);

/// The sections a player who last saw @p lastSeen should see after updating to @p current:
/// everything newer than lastSeen up to current. On first run (lastSeen empty) only the newest
/// section up to current. Nothing for dev builds or when nothing changed.
std::vector<Section> newSince(const std::vector<Section>& sections, const std::string& lastSeen,
                              const std::string& current);

/// Reads the installed CHANGELOG.md (next to the readme). Empty if it is missing.
std::vector<Section> loadInstalled();

} // namespace coop::changelog
