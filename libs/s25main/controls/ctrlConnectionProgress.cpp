// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ctrlConnectionProgress.h"
#include "Loader.h"
#include "RttrLobbyClient.hpp"
#include "WindowManager.h"
#include "controls/ctrlPercent.h"
#include "controls/ctrlText.h"
#include "desktops/dskGameLobby.h"
#include "desktops/dskParty.h"
#include "frontend/PageLayout.h"
#include "mygettext/mygettext.h"
#include "network/GameClient.h"
#include "liblobby/LobbyClient.h"
#include "s25util/colors.h"
#include <algorithm>

ctrlConnectionProgress::ctrlConnectionProgress(Window* parent, const unsigned id,
                                               std::function<void(ClientError)> onError)
    : ctrlGroup(parent, id), onError_(std::move(onError))
{
    AddText(ID_Status, DrawPoint(0, 0), "", COLOR_YELLOW, FontStyle{}, NormalFont);
    AddPercent(ID_Progress, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, COLOR_YELLOW, NormalFont, &progress_);
    AddTextButton(ID_Abort, DrawPoint(0, 0), Extent(1, 1), TextureColor::Red1, _("Abort"), NormalFont);
    SetVisible(false);
}

ctrlConnectionProgress::~ctrlConnectionProgress()
{
    if(connecting_)
    {
        GAMECLIENT.Stop();
        WINDOWMANAGER.SetCursor();
    }
    GAMECLIENT.RemoveInterface(this);
}

void ctrlConnectionProgress::Start(const ServerType type)
{
    type_ = type;
    lobbyClient_.reset();
    if(type == ServerType::Lobby)
        lobbyClient_ = std::make_unique<RttrLobbyClient>(LOBBYCLIENT);
    progress_ = 0;
    connecting_ = true;
    GetCtrl<Window>(ID_Progress)->SetVisible(false);
    SetStatus(_("Waiting for Reply..."));
    SetVisible(true);
    GAMECLIENT.SetInterface(this);
    WINDOWMANAGER.SetCursor(Cursor::Moon);
}

void ctrlConnectionProgress::Resize(const Extent& size)
{
    ctrlGroup::Resize(size);
    const auto rows = frontend::LayoutList(Rect(DrawPoint(0, 0), size), 3, 48, 680);
    for(unsigned id = ID_Status; id <= ID_Abort; ++id)
    {
        auto* control = GetCtrl<Window>(id);
        control->SetPos(rows[id].getOrigin());
        control->Resize(rows[id].getSize());
    }
    GetCtrl<ctrlText>(ID_Status)->setMaxWidth(rows[0].getSize().x);
}

void ctrlConnectionProgress::Abort()
{
    if(!connecting_)
        return;
    GAMECLIENT.Stop();
    CI_Error(ClientError::ConnectionLost);
}

void ctrlConnectionProgress::Msg_ButtonClick(const unsigned id)
{
    if(id == ID_Abort)
        Abort();
}

void ctrlConnectionProgress::CI_Error(const ClientError error)
{
    if(!connecting_)
        return;
    connecting_ = false;
    SetVisible(false);
    GAMECLIENT.RemoveInterface(this);
    WINDOWMANAGER.SetCursor();
    onError_(error);
}

void ctrlConnectionProgress::SetStatus(const std::string& text)
{
    GetCtrl<ctrlText>(ID_Status)->SetText(text);
}

void ctrlConnectionProgress::CI_NextConnectState(const ConnectState state)
{
    GetCtrl<Window>(ID_Progress)->SetVisible(false);
    switch(state)
    {
        case ConnectState::Initiated: SetStatus(_("Waiting for Reply...")); break;
        case ConnectState::VerifyServer: SetStatus(_("Verifying server info...")); break;
        case ConnectState::QueryPw: SetStatus(_("Checking Password...")); break;
        case ConnectState::QueryMapInfo: SetStatus(_("Checking Map...")); break;
        case ConnectState::ReceiveMap:
            SetStatus(_("Receiving Map..."));
            GetCtrl<Window>(ID_Progress)->SetVisible(true);
            break;
        case ConnectState::VerifyMap: SetStatus(_("Checking Map...")); break;
        case ConnectState::QueryServerName: SetStatus(_("Waiting for server info...")); break;
        case ConnectState::QueryPlayerList: SetStatus(_("Waiting for Playerinfo...")); break;
        case ConnectState::QuerySettings: SetStatus(_("Waiting for server info...")); break;
        case ConnectState::Finished:
            connecting_ = false;
            SetVisible(false);
            WINDOWMANAGER.SetCursor();
            GAMECLIENT.RemoveInterface(this);
            if(dskParty::IsFor(type_))
                WINDOWMANAGER.Switch(std::make_unique<dskParty>(GAMECLIENT.GetGameLobby(), GAMECLIENT.GetPlayerId()));
            else
                WINDOWMANAGER.Switch(std::make_unique<dskGameLobby>(type_, GAMECLIENT.GetGameLobby(),
                                                                    GAMECLIENT.GetPlayerId(), std::move(lobbyClient_)));
            break;
    }
}

void ctrlConnectionProgress::CI_MapPartReceived(const uint32_t received, const uint32_t total)
{
    if(total)
        progress_ = static_cast<unsigned short>(std::min<uint64_t>(100, static_cast<uint64_t>(received) * 100 / total));
}
