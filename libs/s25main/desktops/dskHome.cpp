// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dskHome.h"
#include "GlobalVars.h"
#include "RTTR_Version.h"
#include "Settings.h"
#include "WindowManager.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlTimer.h"
#include "desktops/dskCredits.h"
#include "desktops/dskMainMenu.h"
#include "desktops/dskMultiPlayer.h"
#include "desktops/dskOptions.h"
#include "desktops/dskSinglePlayer.h"
#include "desktops/dskTitle.h"
#include "frontend/MenuRoutes.h"
#include "ingameWindows/iwChangelog.h"
#include "ingameWindows/iwMsgbox.h"
#include "mygettext/mygettext.h"
#include <chrono>

unsigned dskHome::lastChoice_ = 0;

namespace {
constexpr unsigned debugDataMsgbox = 100;
}

dskHome::dskHome() : dskFrontEndPage(rttr::version::GetTitle())
{
    // Wide tiles: they carry one or two words until F4/F11 give them artwork.
    UseTiles(Extent(5, 2), Extent(300, 120));
    newestSave_ = dskSinglePlayer::FindNewestSave();
    AddItem(ID_Continue, _("Resume last game"))->SetEnabled(!newestSave_.empty());
    AddItem(ID_Campaigns, _("Campaigns"));
    AddItem(ID_Maps, _("Maps & scenarios"));
    AddItem(ID_Load, _("Load game"));
    AddItem(ID_Online, _("Play online"));
    AddItem(ID_Options, _("Options"));
    AddItem(ID_WhatsNew, _("What's new"));
    AddItem(ID_Credits, _("Credits"));
    AddItem(ID_Classic, _("Classic menus"), TextureColor::Grey);
    AddItem(ID_Quit, _("Quit program"), TextureColor::Red1);
    if(lastChoice_)
        SetEntryFocus(lastChoice_);

    using namespace std::chrono_literals;
    if(SETTINGS.global.submitDebugData == SubmitDebugData::AskAtStart)
        AddTimer(ID_tmrDebugData, 250ms);
    // The same rule as dskMainMenu: shown once per new version, marked seen only when really shown.
    pendingChangelog_ = coop::changelog::newSince(coop::changelog::loadInstalled(), SETTINGS.global.coopChangelogSeen,
                                                  coop::changelog::runningVersion());
}

std::unique_ptr<Desktop> dskHome::Create()
{
    return dskFrontEndPage::Create([] { return std::make_unique<dskHome>(); });
}

void dskHome::SetActive(bool activate)
{
    dskFrontEndPage::SetActive(activate);
    // On activation, not construction: a page that is built but replaced before it is shown must not
    // change where the upstream desktops return to.
    if(activate && WINDOWMANAGER.GetCurrentDesktop() == this)
        frontend::SetMenuStyle(frontend::MenuStyle::FrontEnd);
    // After the desktop switch, not in the constructor: the switch closes every window (see dskMainMenu).
    if(!activate || pendingChangelog_.empty() || WINDOWMANAGER.GetCurrentDesktop() != this)
        return;
    const auto sections = std::move(pendingChangelog_);
    pendingChangelog_.clear();
    WINDOWMANAGER.Show(std::make_unique<iwChangelog>(sections));
    SETTINGS.global.coopChangelogSeen = coop::changelog::runningVersion();
    SETTINGS.Save();
}

void dskHome::Msg_Timer(const unsigned ctrl_id)
{
    GetCtrl<ctrlTimer>(ctrl_id)->Stop();
    WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(
      _("Submit debug data?"),
      _("RttR now supports sending debug data. Would you like to help us improving this game by sending debug data?"),
      this, MsgboxButton::YesNo, MsgboxIcon::QuestionRed, debugDataMsgbox));
}

void dskHome::Msg_MsgBoxResult(const unsigned msgbox_id, const MsgboxResult mbr)
{
    if(msgbox_id != debugDataMsgbox)
        return;
    SETTINGS.global.submitDebugData = mbr == MsgboxResult::Yes ? SubmitDebugData::Yes : SubmitDebugData::AlwaysAsk;
    SETTINGS.Save();
}

bool dskHome::OnBackAtRoot()
{
    WINDOWMANAGER.Switch(dskTitle::Create());
    return true;
}

void dskHome::OnChoose(const unsigned ctrl_id)
{
    // Only tiles that leave Home; What's new is a window on top and Quit leaves nothing to come back to.
    if(ctrl_id != ID_WhatsNew && ctrl_id != ID_Quit)
        lastChoice_ = ctrl_id;
    switch(ctrl_id)
    {
        case ID_Continue:
            // A save deleted since Home was built ends in upstream's "couldn't be loaded" box (ResumeSave).
            if(!newestSave_.empty())
                dskSinglePlayer::ResumeSave(newestSave_);
            break;
        case ID_Campaigns: dskSinglePlayer::OpenCampaigns(); break;
        case ID_Maps: dskSinglePlayer::PrepareSinglePlayerServer(); break;
        case ID_Load: dskSinglePlayer::PrepareLoadGame(); break;
        case ID_Online: WINDOWMANAGER.Switch(std::make_unique<dskMultiPlayer>()); break;
        case ID_Options: WINDOWMANAGER.Switch(std::make_unique<dskOptions>()); break;
        case ID_WhatsNew: WINDOWMANAGER.ToggleWindow(std::make_unique<iwChangelog>()); break;
        case ID_Credits: WINDOWMANAGER.Switch(std::make_unique<dskCredits>()); break;
        case ID_Classic:
            lastChoice_ = 0;
            WINDOWMANAGER.Switch(std::make_unique<dskMainMenu>());
            break;
        case ID_Quit: GLOBALVARS.notdone = false; break;
        default: break;
    }
}
