// Copyright (C) 2024 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dskCampaignVictory.h"
#include "Loader.h"
#include "Settings.h"
#include "WindowManager.h"
#include "frontend/MenuRoutes.h"

namespace {
constexpr unsigned ID_btContinue = 0;
}

dskCampaignVictory::dskCampaignVictory()
    : Desktop(LOADER.GetImageN(ResourceId{SETTINGS.campaigns.getCompletedCampaign() ? "setup895" : "setup896"}, 0))
{
    // Chapters are indices, players count from 1
    if(!SETTINGS.campaigns.getCompletedCampaign())
        AddText(10, DrawPoint{800 / 2, 600 - 50},
                _("You have successfully completed chapter") + std::string{" "}
                  + std::to_string(*SETTINGS.campaigns.getCompletedChapter() + 1) + ".",
                COLOR_YELLOW, FontStyle::CENTER, LargeFont);
    SETTINGS.campaigns.resetCompletionStatus();
    // A needs a focusable control; use the same exit route as mouse and keyboard input.
    AddTextButton(ID_btContinue, DrawPoint(300, 575), Extent(200, 22), TextureColor::Green2, _("Continue"), NormalFont);
}

bool dskCampaignVictory::Msg_PadCommand(unsigned, const PadButton button)
{
    if(button == PadButton::B || button == PadButton::Start)
        return ShowMenu();
    return false;
}

void dskCampaignVictory::Msg_ButtonClick(const unsigned ctrl_id)
{
    if(ctrl_id == ID_btContinue)
        ShowMenu();
}

bool dskCampaignVictory::Msg_LeftDown(const MouseCoords&)
{
    return ShowMenu();
}

bool dskCampaignVictory::Msg_KeyDown(const KeyEvent&)
{
    return ShowMenu();
}

bool dskCampaignVictory::ShowMenu()
{
    WINDOWMANAGER.Switch(frontend::MainMenu());
    return true;
}
