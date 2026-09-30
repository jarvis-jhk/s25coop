// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ClientError.h"
#include "mygettext/mygettext.h"

const char* ClientErrorToStr(ClientError error)
{
    switch(error)
    {
        case ClientError::InvalidMessage: return _("Server sent an invalid message!");
        case ClientError::ServerFull: return _("This Server is full!");
        case ClientError::WrongPassword: return _("Wrong Password!");
        case ClientError::ConnectionLost: return _("Lost connection to server!");
        case ClientError::InvalidServerType: return _("Wrong Server Type!");
        case ClientError::MapTransmission: return _("Map transmission was corrupt!");
        case ClientError::WrongVersion: return _("Wrong client version");
        case ClientError::InvalidMap: return _("Map is invalid or failed to load properly!");
        case ClientError::CoopRefused: return _("Co-players are not allowed.");
        case ClientError::CoopKicked: return _("The host removed you from the game.");
        case ClientError::CoopLeaderLeft: return _("The player you played together with left the game.");
        case ClientError::CoopOutOfSync:
            return _("Your game went out of sync with the others (async), so you were removed from it.");
        case ClientError::CoopTooFarBehind:
            return _("Your computer fell too far behind the others, so you were removed from the game.");
        case ClientError::LocalPlayerSetup: return _("Could not set up the requested additional local players!");
        default: return _("Unknown error!");
    }
}
