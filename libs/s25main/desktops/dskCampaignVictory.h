// Copyright (C) 2024 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Desktop.h"

struct MouseCoords;

class dskCampaignVictory : public Desktop
{
public:
    dskCampaignVictory();

    bool WantsPadInput() const override { return true; }
    bool Msg_PadCommand(unsigned slot, PadButton button) override;

private:
    void Msg_ButtonClick(unsigned ctrl_id) override;
    bool Msg_LeftDown(const MouseCoords&) override;
    bool Msg_KeyDown(const KeyEvent&) override;

    bool ShowMenu();
};
