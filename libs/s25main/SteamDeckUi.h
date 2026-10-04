// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <boost/filesystem/path.hpp>
#include <string_view>

namespace deck {
/// Steam's explicit 0/1 flag takes precedence over the Linux hardware fallback.
bool Detect(std::string_view steamDeck, const boost::filesystem::path& dmiDirectory);
bool IsSteamDeck();
/// TV mode wins; fixed GUI scale still wins over either automatic recommendation.
unsigned UiReferenceHeight(bool tvMode, bool steamDeckUi);
} // namespace deck
