// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "coop/Changelog.h"
#include "desktops/dskMenuBase.h"
#include <vector>

/// Klasse des Hauptmenü Desktops.
class dskMainMenu : public dskMenuBase
{
public:
    dskMainMenu();

    void SetActive(bool activate = true) override;
    void Msg_ButtonClick(unsigned ctrl_id) override;
    void Msg_Timer(unsigned ctrl_id) override;
    void Msg_MsgBoxResult(unsigned msgbox_id, MsgboxResult mbr) override;
    bool Msg_LeftUp(const MouseCoords& mc) override;

private:
    /// s25coop: changelog sections to show after an update
    std::vector<coop::changelog::Section> pendingChangelog_;
};
