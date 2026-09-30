// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "desktops/dskMenuBase.h"

/// Klasse des Einzelspieler Desktops.
class dskSinglePlayer : public dskMenuBase
{
public:
    dskSinglePlayer();
    bool Msg_PadCommand(unsigned slot, PadButton button) override;

private:
    void Msg_ButtonClick(unsigned ctrl_id) override;
    void PrepareSinglePlayerServer();
    void PrepareLoadGame();
};
