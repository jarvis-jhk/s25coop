// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dskSinglePlayer.h"
#include "ListDir.h"
#include "Loader.h"
#include "RttrConfig.h"
#include "Savegame.h"
#include "Settings.h"
#include "WindowManager.h"
#include "controls/ctrlButton.h"
#include "dskCampaignSelection.h"
#include "dskSelectMap.h"
#include "files.h"
#include "frontend/MenuRoutes.h"
#include "ingameWindows/iwConnecting.h"
#include "ingameWindows/iwMsgbox.h"
#include "ingameWindows/iwPlayReplay.h"
#include "ingameWindows/iwSave.h"
#include "network/CreateServerInfo.h"
#include "network/GameClient.h"

namespace bfs = boost::filesystem;

static CreateServerInfo createLocalGameInfo(const std::string& name)
{
    return CreateServerInfo(ServerType::Local, SETTINGS.server.localPort, name);
}

/** @class dskSinglePlayer
 *
 *  Klasse des Einzelspieler Desktops.
 */

dskSinglePlayer::dskSinglePlayer()
{
    RTTR_Assert(dskMenuBase::ID_FIRST_FREE <= 3);

    AddTextButton(3, DrawPoint(115, 180), Extent(220, 22), TextureColor::Green2, _("Resume last game"), NormalFont);
    AddTextButton(7, DrawPoint(115, 210), Extent(220, 22), TextureColor::Green2, _("Load game"), NormalFont);

    AddTextButton(5, DrawPoint(115, 250), Extent(220, 22), TextureColor::Green2, std::string(_("Campaigns")),
                  NormalFont);
    AddTextButton(6, DrawPoint(115, 280), Extent(220, 22), TextureColor::Green2, _("Unlimited Play"), NormalFont);

    AddTextButton(4, DrawPoint(115, 320), Extent(220, 22), TextureColor::Green2, _("Play Replay"), NormalFont);

    AddTextButton(8, DrawPoint(115, 390), Extent(220, 22), TextureColor::Red1, _("Back"), NormalFont);

    AddImage(11, DrawPoint(20, 20), LOADER.GetImageN("logo", 0));
}

bool dskSinglePlayer::Msg_PadCommand(unsigned, const PadButton button)
{
    if(button != PadButton::B)
        return false;
    Msg_ButtonClick(8); // The existing Back action.
    return true;
}

void dskSinglePlayer::Msg_ButtonClick(const unsigned ctrl_id)
{
    switch(ctrl_id)
    {
        case 3: // "Letztes Spiel fortsetzen"
        {
            const bfs::path newest = FindNewestSave();
            if(newest.empty())
                WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(_("Error"), _("The specified file couldn't be loaded!"),
                                                              nullptr, MsgboxButton::Ok, MsgboxIcon::ExclamationRed));
            else
                ResumeSave(newest);
        }
        break;
        case 4: // "Replay abspielen"
        {
            WINDOWMANAGER.ToggleWindow(std::make_unique<iwPlayReplay>());
        }
        break;
        case 5: // "Kampagne"
        {
            OpenCampaigns();
        }
        break;
        case 6: // "Freies Spiel"
        {
            PrepareSinglePlayerServer();
        }
        break;
        case 7: // "Spiel laden"
        {
            PrepareLoadGame();
        }
        break;
        case 8: // "Zurück"
        {
            WINDOWMANAGER.Switch(frontend::MainMenu());
        }
        break;
    }
}

bfs::path dskSinglePlayer::FindNewestSave()
{
    bfs::path mostRecentFilepath;
    s25util::time64_t recent = 0;
    for(const auto& savFile : ListDir(RTTRCONFIG.ExpandPath(s25::folders::save), "sav"))
    {
        Savegame save;
        if(!save.Load(savFile, SaveGameDataToLoad::Header))
            continue;
        if(save.GetSaveTime() > recent)
        {
            recent = save.GetSaveTime();
            mostRecentFilepath = savFile;
        }
    }
    if(recent == 0 || !mostRecentFilepath.has_filename())
        return {};
    return mostRecentFilepath;
}

void dskSinglePlayer::ResumeSave(const bfs::path& save)
{
    const CreateServerInfo csi = createLocalGameInfo(save.stem().string());
    WINDOWMANAGER.Switch(std::make_unique<dskSelectMap>(csi));
    if(GAMECLIENT.HostGame(csi, {save, MapType::Savegame}))
        WINDOWMANAGER.ShowAfterSwitch(std::make_unique<iwConnecting>(csi.type, nullptr));
    else
    {
        WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(_("Error"), _("The specified file couldn't be loaded!"), nullptr,
                                                      MsgboxButton::Ok, MsgboxIcon::ExclamationRed));
    }
}

void dskSinglePlayer::OpenCampaigns()
{
    WINDOWMANAGER.Switch(std::make_unique<dskCampaignSelection>(createLocalGameInfo(_("Campaign"))));
}

void dskSinglePlayer::PrepareSinglePlayerServer()
{
    WINDOWMANAGER.Switch(std::make_unique<dskSelectMap>(createLocalGameInfo(_("Unlimited Play"))));
}

void dskSinglePlayer::PrepareLoadGame()
{
    CreateServerInfo csi = createLocalGameInfo(_("Unlimited Play"));

    WINDOWMANAGER.Switch(std::make_unique<dskSelectMap>(csi));
    WINDOWMANAGER.ShowAfterSwitch(std::make_unique<iwLoad>(csi));
}
