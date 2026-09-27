// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "iwChangelog.h"
#include "Loader.h"
#include "controls/ctrlMultiline.h"
#include "gameData/const_gui_ids.h"
#include <mygettext/mygettext.h>

iwChangelog::iwChangelog(const std::vector<coop::changelog::Section>& sections)
    : IngameWindow(CGI_CHANGELOG, IngameWindow::posCenter, Extent(560, 400), _("What's new"),
                   LOADER.GetImageN("resource", 41))
{
    ctrlMultiline* text =
      AddMultiline(0, DrawPoint(10, 20), Extent(GetSize().x - 20, 370), TextureColor::Green1, NormalFont);
    const auto shown = sections.empty() ? coop::changelog::loadInstalled() : sections;
    if(shown.empty())
    {
        text->AddString(_("The file was not found!"), COLOR_RED, false);
        return;
    }
    for(const auto& section : shown)
    {
        text->AddString("Version " + section.version, COLOR_YELLOW, false);
        for(const std::string& entry : section.entries)
            text->AddString("- " + entry, COLOR_WHITE, false);
        text->AddString("", COLOR_WHITE, false);
    }
}
