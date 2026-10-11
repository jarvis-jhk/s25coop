// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dskGameLoader.h"
#include "Game.h"
#include "Loader.h"
#include "WindowManager.h"
#include "controls/ctrlMultiline.h"
#include "controls/ctrlText.h"
#include "controls/ctrlTimer.h"
#include "dskDirectIP.h"
#include "dskGameInterface.h"
#include "dskLobby.h"
#include "files.h"
#include "frontend/MenuRoutes.h"
#include "ingameWindows/iwMsgbox.h"
#include "network/GameClient.h"
#include "ogl/FontStyle.h"
#include "gameTypes/TextureColor.h"
#include "liblobby/LobbyClient.h"
#include <memory>
#include <utility>

/**
 *  Konstruktor von @p dskGameLoader.
 *  Startet das Spiel und lädt alles Notwendige.
 */
dskGameLoader::dskGameLoader(std::shared_ptr<Game> game)
    : dskFrontEndPage(_("Loading game"), frontend::GetMenuStyle() == frontend::MenuStyle::FrontEnd),
      frontEnd_(frontend::GetMenuStyle() == frontend::MenuStyle::FrontEnd), loader_(LOADER, std::move(game))
{
    WINDOWMANAGER.SetCursor(Cursor::None);

    using namespace std::chrono_literals;
    AddTimer(ID_Timer, 50ms);

    if(frontEnd_)
    {
        AddText(ID_MapTitle, DrawPoint(0, 0), GAMECLIENT.GetMapTitle(), COLOR_YELLOW, FontStyle{}, LargeFont);
        AddText(ID_Status, DrawPoint(0, 0), _("Preparing map..."), COLOR_GREEN, FontStyle{}, NormalFont);
        auto* tip = AddMultiline(ID_Tip, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, NormalFont);
        tip->ShowBackground(false);
        tip->SetScrollBarAllowed(false);
        tip->AddString(_("Tip: connect your headquarters to a woodcutter, sawmill and forester with roads to keep "
                         "building supplies coming."),
                       COLOR_WHITE);
        auto* controls = AddMultiline(ID_Controls, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, NormalFont);
        controls->ShowBackground(false);
        controls->SetScrollBarAllowed(false);
        controls->AddString(_("In game: left stick moves your cursor; right stick moves the camera. A opens the "
                              "object or actions at the cursor. Back opens your system menu."),
                            COLOR_WHITE);
        Layout();
    } else
    {
        // Classic keeps its original artwork and accumulating loading messages.
        GetCtrl<Window>(ID_txtTitle)->SetVisible(false);
        background = LOADER.GetImageN(ResourceId(LOAD_SCREENS[rand() % LOAD_SCREENS.size()]), 0);
        SetScale(true);
        AddText(ID_MapTitle, DrawPoint(800 / 2, 600 - 50), "", COLOR_YELLOW, FontStyle::CENTER, LargeFont);
        for(unsigned i = 0; i < 8; ++i)
            AddText(ID_Status + i, DrawPoint(30, 30 + i * 20), "", COLOR_GREEN, FontStyle{}, LargeFont);
    }

    LOBBYCLIENT.AddListener(this);
    GAMECLIENT.SetInterface(this);
}

dskGameLoader::~dskGameLoader()
{
    WINDOWMANAGER.SetCursor();
    LOBBYCLIENT.RemoveListener(this);
    GAMECLIENT.RemoveInterface(this);
}

void dskGameLoader::OnLayout()
{
    if(!frontEnd_ || !GetCtrl<Window>(ID_Controls))
        return;
    const Rect& area = GetFrame().content;
    const bool enoughRoom = area.getSize().x >= 240 && area.getSize().y >= 180;
    for(const unsigned id : {ID_MapTitle, ID_Status, ID_Tip, ID_Controls})
        GetCtrl<Window>(id)->SetVisible(enoughRoom);
    if(!enoughRoom)
        return;
    const auto width = static_cast<unsigned short>(area.getSize().x);
    auto* title = GetCtrl<ctrlText>(ID_MapTitle);
    title->SetPos(area.getOrigin());
    title->setMaxWidth(width);
    auto* status = GetCtrl<ctrlText>(ID_Status);
    status->SetPos(area.getOrigin() + DrawPoint(0, 36));
    status->setMaxWidth(width);
    const unsigned blockHeight = (area.getSize().y - 80) / 2;
    auto* tip = GetCtrl<ctrlMultiline>(ID_Tip);
    tip->SetPos(area.getOrigin() + DrawPoint(0, 80));
    tip->Resize(Extent(width, blockHeight));
    auto* controls = GetCtrl<ctrlMultiline>(ID_Controls);
    controls->SetPos(area.getOrigin() + DrawPoint(0, static_cast<int>(80 + blockHeight)));
    controls->Resize(Extent(width, blockHeight));
}

bool dskGameLoader::LoadTextures()
{
    loader_.initTextures();
    return loader_.loadTextures();
}

std::unique_ptr<dskGameInterface> dskGameLoader::CreateGameInterface()
{
    return std::make_unique<dskGameInterface>(loader_.getGame(), GAMECLIENT.GetNWFInfo(), GAMECLIENT.GetPlayerId());
}

void dskGameLoader::SetStatus(const std::string& text)
{
    if(failed_)
        return;
    GetCtrl<ctrlText>(frontEnd_ ? static_cast<unsigned>(ID_Status) : ID_MapTitle + position)->SetText(text);
}

void dskGameLoader::Msg_MsgBoxResult(const unsigned msgbox_id, const MsgboxResult /*mbr*/)
{
    if(msgbox_id == 0) // Verbindung zu Server verloren?
    {
        GAMECLIENT.Stop();

        if(LOBBYCLIENT.IsLoggedIn()) // steht die Lobbyverbindung noch?
            WINDOWMANAGER.Switch(std::make_unique<dskLobby>());
        else if(loader_.getGame()->world_.IsSinglePlayer())
            WINDOWMANAGER.Switch(frontend::SinglePlayerMenu());
        else
            WINDOWMANAGER.Switch(std::make_unique<dskDirectIP>());
    }
}

void dskGameLoader::Msg_Timer(const unsigned /*ctrl_id*/)
{
    auto* timer = GetCtrl<ctrlTimer>(ID_Timer);
    timer->Stop();
    if(failed_)
        return;
    try
    {
        AdvanceLoading();
    } catch(const std::runtime_error& e)
    {
        ShowErrorMsg(e.what());
    }
}

void dskGameLoader::AdvanceLoading()
{
    using namespace std::chrono_literals;
    const auto interval = 50ms;

    switch(position)
    {
        case 0: // Kartename anzeigen
            GetCtrl<ctrlText>(ID_MapTitle)->SetText(GAMECLIENT.GetMapTitle());
            if(frontEnd_)
                SetStatus(_("Map loaded"));
            break;

        case 1: // Karte geladen
            SetStatus(frontEnd_ ? _("Preparing tribes...") : _("Map was loaded and pinned at the wall..."));
            break;

        case 2: // Nationen ermitteln
            loader_.initNations();

            SetStatus(frontEnd_ ? _("Loading game resources...") : _("Tribal chiefs assembled around the table..."));
            break;

        case 3: // Objekte laden
        {
            if(!LoadTextures())
            {
                ShowErrorMsg(_("Failed to load game resources"));
                return; // Don't restart timer!
            }

            SetStatus(frontEnd_ ? _("Creating game views...") : _("Game crate was picked and spread out..."));
            break;
        }
        case 4: // Welt erstellen
            // Views (and postboxes) must exist before GameLoaded permits mission onStart.
            gameInterface = CreateGameInterface();
            if(failed_)
                return;
            // TODO: richtige Messages senden, um das zu laden /*GetMap()->GenerateOpenGL();*/

            // We are done, wait for others
            GAMECLIENT.GameLoaded();

            SetStatus(frontEnd_ ? _("Waiting for the other players...") : _("World was put together and glued..."));
            break;

        case 5: // nochmal text anzeigen
            if(!frontEnd_)
            {
                SetStatus(_("And let's go!"));
                GetCtrl<ctrlText>(ID_MapTitle + position)->SetTextColor(COLOR_RED);
            }
            return;
    }

    if(failed_)
        return;
    ++position;
    GetCtrl<ctrlTimer>(ID_Timer)->Start(interval);
}

void dskGameLoader::ShowErrorMsg(const std::string& error)
{
    if(failed_)
        return;
    failed_ = true;
    if(frontEnd_)
        GetCtrl<ctrlText>(ID_Status)->SetText(_("Loading failed"));
    WINDOWMANAGER.SetCursor();
    auto wnd = std::make_unique<iwMsgbox>(_("Error"), error, this, MsgboxButton::Ok, MsgboxIcon::ExclamationRed, 0);
    if(IsActive())
        WINDOWMANAGER.Show(std::move(wnd));
    else
        WINDOWMANAGER.ShowAfterSwitch(std::move(wnd));
    GetCtrl<ctrlTimer>(ID_Timer)->Stop();
}

/**
 *  (Lobby-)Status: Benutzerdefinierter Fehler (kann auch Conn-Loss o.ä sein)
 */
void dskGameLoader::LC_Status_Error(const std::string& error)
{
    ShowErrorMsg(error);
}

void dskGameLoader::CI_GameStarted()
{
    if(failed_)
        return;
    RTTR_Assert(gameInterface);
    WINDOWMANAGER.Switch(std::move(gameInterface));
}

void dskGameLoader::CI_Error(const ClientError ce)
{
    ShowErrorMsg(ClientErrorToStr(ce));
}
