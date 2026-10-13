// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dskFrontEndOnline.h"
#include "Settings.h"
#include "WindowManager.h"
#include "desktops/dskFrontEndConnect.h"
#include "desktops/dskFrontEndLan.h"
#include "ingameWindows/iwDirectIPCreate.h"
#include "ingameWindows/iwLobbyConnect.h"
#include "ingameWindows/iwMsgbox.h"
#include "mygettext/mygettext.h"

dskFrontEndOnline::dskFrontEndOnline() : dskFrontEndPage(_("Play online"))
{
    UseTiles(Extent(5, 2), Extent(340, 130));
    AddItem(ID_Join, _("Join Game"));
    AddItem(ID_Lan, _("Network/LAN"));
    AddItem(ID_Internet, _("Internet Lobby"));
    AddItem(ID_Host, _("Create Game"));
    Layout();
}

void dskFrontEndOnline::OnChoose(const unsigned id)
{
    switch(id)
    {
        case ID_Join:
            Open([state = std::make_shared<dskFrontEndConnect::JoinState>()] {
                return std::make_unique<dskFrontEndConnect>(state);
            });
            break;
        case ID_Lan:
            Open([state = std::make_shared<dskFrontEndLan::BrowseState>()] {
                return std::make_unique<dskFrontEndLan>(state);
            });
            break;
        case ID_Internet: WINDOWMANAGER.Show(std::make_unique<iwLobbyConnect>(), true); break;
        case ID_Host:
            if(SETTINGS.proxy.type != ProxyType::None)
                WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(
                  _("Sorry!"),
                  _("You can't create a game while a proxy server is active\nDisable the use of a proxy server first!"),
                  this, MsgboxButton::Ok, MsgboxIcon::ExclamationGreen, 1));
            else
                WINDOWMANAGER.ReplaceWindow(std::make_unique<iwDirectIPCreate>(ServerType::Direct));
            break;
        default: break;
    }
}
