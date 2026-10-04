// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "SteamDeckUi.h"
#include "TvDisplay.h"
#include "s25util/System.h"
#include <boost/nowide/fstream.hpp>
#include <string>

namespace deck {
bool Detect(std::string_view steamDeck, const boost::filesystem::path& dmiDirectory)
{
    if(steamDeck == "1")
        return true;
    if(steamDeck == "0")
        return false;

    // Desktop-mode launches need not inherit Steam's flag. Require both hardware IDs,
    // rather than treating every SteamOS installation or Valve controller as a Deck.
    std::string vendor, product;
    boost::nowide::ifstream vendorFile(dmiDirectory / "sys_vendor");
    boost::nowide::ifstream productFile(dmiDirectory / "product_name");
    std::getline(vendorFile, vendor);
    std::getline(productFile, product);
    return vendor == "Valve" && (product == "Jupiter" || product == "Galileo");
}

bool IsSteamDeck()
{
    return Detect(System::getEnvVar("SteamDeck"), "/sys/class/dmi/id");
}

unsigned UiReferenceHeight(bool tvMode, bool steamDeckUi)
{
    // 800 device pixels / 640 view units gives 125% on the handheld panel. The
    // driver's existing two-axis cap keeps resized or docked screens usable.
    return tvMode ? tv::UI_REFERENCE_HEIGHT : (steamDeckUi ? 640u : 0u);
}
} // namespace deck
