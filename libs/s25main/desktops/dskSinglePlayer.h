// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "desktops/dskMenuBase.h"
#include <boost/filesystem/path.hpp>

/// Klasse des Einzelspieler Desktops.
class dskSinglePlayer : public dskMenuBase
{
public:
    dskSinglePlayer();
    bool Msg_PadCommand(unsigned slot, PadButton button) override;

    // The single-player entry points, shared with the front end's home page (dskHome) so both menus
    // start exactly the same thing.

    /// The savegame with the newest save time; empty if there is none that loads.
    static boost::filesystem::path FindNewestSave();
    /// Host the save locally and go to its lobby (shows an error box when it cannot be loaded).
    static void ResumeSave(const boost::filesystem::path& save);
    static void OpenCampaigns();
    static void PrepareSinglePlayerServer();
    static void PrepareLoadGame();

private:
    void Msg_ButtonClick(unsigned ctrl_id) override;
};
