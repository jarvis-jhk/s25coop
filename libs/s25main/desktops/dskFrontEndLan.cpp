// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dskFrontEndLan.h"
#include "Loader.h"
#include "RTTR_Version.h"
#include "Settings.h"
#include "WindowManager.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlTable.h"
#include "controls/ctrlText.h"
#include "desktops/dskFrontEndConnect.h"
#include "helpers/format.hpp"
#include "ingameWindows/iwDirectIPCreate.h"
#include "ingameWindows/iwMsgbox.h"
#include "mygettext/mygettext.h"
#include "gameData/LanDiscoveryCfg.h"
#include "s25util/colors.h"
#include <algorithm>
#include <chrono>

dskFrontEndLan::dskFrontEndLan(std::shared_ptr<BrowseState> state)
    : dskFrontEndPage(_("Network/LAN")), discovery_(LAN_DISCOVERY_CFG), state_(std::move(state))
{
    using Sort = ctrlTable::SortType;
    auto* table = AddTable(ID_Servers, DrawPoint(0, 0), Extent(500, 200), TextureColor::Grey, NormalFont,
                           {{_("Server"), 35, Sort::String},
                            {_("Map"), 35, Sort::String},
                            {_("Player"), 12, Sort::String},
                            {_("Version"), 18, Sort::String}});
    // The deterministic discovery order also defines the endpoint index used on activation.
    for(unsigned id = 1; id <= table->GetNumColumns(); ++id)
        table->GetCtrl<ctrlButton>(id)->SetEnabled(false);
    AddTextButton(ID_Join, DrawPoint(0, 0), Extent(1, 1), TextureColor::Green2, _("Join Game"), NormalFont);
    AddTextButton(ID_Refresh, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, _("Refresh"), NormalFont);
    AddTextButton(ID_Host, DrawPoint(0, 0), Extent(1, 1), TextureColor::Green2, _("Create Game"), NormalFont);
    AddText(ID_Status, DrawPoint(0, 0), "", COLOR_YELLOW, FontStyle{}, NormalFont);
    AddTimer(ID_Timer, std::chrono::minutes(1));
    listening_ = discovery_.Start();
    UpdateServers(true);
    SetEntryFocus(ID_Servers);
    Layout();
}

Window* dskFrontEndLan::GetPadEntryCtrl(const unsigned slot)
{
    if(auto* entry = dskFrontEndPage::GetPadEntryCtrl(slot))
        return entry;
    return GetCtrl<Window>(servers_.empty() ? ID_Refresh : ID_Servers);
}

void dskFrontEndLan::Msg_PaintBefore()
{
    dskFrontEndPage::Msg_PaintBefore();
    discovery_.Run();
    UpdateServers();
}

void dskFrontEndLan::Msg_Timer(const unsigned id)
{
    if(id == ID_Timer)
        OnChoose(ID_Refresh);
}

void dskFrontEndLan::UpdateServers(const bool force)
{
    const auto& services = discovery_.GetServices();
    const bool same =
      services.size() == lastServices_.size()
      && std::equal(services.begin(), services.end(), lastServices_.begin(), [](const auto& a, const auto& b) {
             return a.first == b.first && a.second.info.data == b.second.info.data;
         });
    if(same && !force)
        return;
    const auto preferred = state_->selected;
    lastServices_ = services;
    servers_.clear();
    for(const auto& service : services)
    {
        if(auto server = frontend::ReadLanServer(service.second))
            servers_.push_back(std::move(*server));
    }
    auto* table = GetCtrl<ctrlTable>(ID_Servers);
    table->DeleteAllItems();
    std::optional<unsigned> chosen;
    for(const auto& server : servers_)
    {
        if(server.Key() == preferred)
            chosen = table->GetNumRows();
        table->AddRow({(server.info.hasPwd ? "(pwd) " : "") + server.info.name, server.info.map,
                       std::to_string(server.info.curNumPlayers) + "/" + std::to_string(server.info.maxNumPlayers),
                       server.info.version});
    }
    // A disappeared or changed endpoint must never silently become another server.
    if(preferred.empty() && !servers_.empty())
        chosen = 0;
    table->SetSelection(chosen);
    state_->selected = preferred.empty() && chosen ? servers_[*chosen].Key() : preferred;
    UpdateStatus();
}

void dskFrontEndLan::Msg_TableSelectItem(const unsigned id, const std::optional<unsigned>& /*selection*/)
{
    if(id != ID_Servers)
        return;
    const auto selected = GetCtrl<ctrlTable>(id)->GetSelection();
    if(selected && *selected < servers_.size())
        state_->selected = servers_[*selected].Key();
    UpdateStatus();
}

void dskFrontEndLan::UpdateStatus()
{
    const auto selected = GetCtrl<ctrlTable>(ID_Servers)->GetSelection();
    const bool compatible =
      selected && *selected < servers_.size() && servers_[*selected].info.revision == rttr::version::GetRevision();
    GetCtrl<ctrlButton>(ID_Join)->SetEnabled(compatible && !layoutUnavailable_);
    std::string status;
    if(layoutUnavailable_)
        status = _("Reduce the UI scale or enlarge the window.");
    else if(!listening_)
        status = _("Connection failed!");
    else if(selected && !compatible)
        status = _("You can't join that game with your version!");
    else
        status = helpers::format(_("%1% games"), servers_.size());
    GetCtrl<ctrlText>(ID_Status)->SetText(status);
}

void dskFrontEndLan::Msg_TableChooseItem(const unsigned id, unsigned /*selection*/)
{
    if(id == ID_Servers)
        OnChoose(ID_Join);
}

void dskFrontEndLan::OnChoose(const unsigned id)
{
    if(layoutUnavailable_)
        return;
    if(id == ID_Refresh)
    {
        listening_ = discovery_.Start();
        discovery_.Refresh();
        UpdateServers(true);
    } else if(id == ID_Join)
    {
        // Drain announcements before resolving the selected identity, including a withdrawal in this input frame.
        const bool drained = discovery_.Run();
        UpdateServers();
        if(!drained)
            return;
        const auto selected = GetCtrl<ctrlTable>(ID_Servers)->GetSelection();
        if(!selected || *selected >= servers_.size())
            return;
        const auto& server = servers_[*selected];
        if(server.info.revision != rttr::version::GetRevision())
            return;
        auto state = std::make_shared<dskFrontEndConnect::JoinState>();
        state->host = server.ip;
        state->port = std::to_string(server.info.port);
        state->ipv6 = server.info.isIPv6;
        state->serverType = ServerType::LAN;
        Open([state] { return std::make_unique<dskFrontEndConnect>(state); });
    } else if(id == ID_Host)
    {
        if(SETTINGS.proxy.type != ProxyType::None)
            WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(
              _("Sorry!"),
              _("You can't create a game while a proxy server is active\nDisable the use of a proxy server first!"),
              this, MsgboxButton::Ok, MsgboxIcon::ExclamationGreen, 1));
        else
            WINDOWMANAGER.ReplaceWindow(std::make_unique<iwDirectIPCreate>(ServerType::LAN));
    }
}

void dskFrontEndLan::OnLayout()
{
    const auto& area = GetFrame().content;
    layoutUnavailable_ = area.getSize().x < 480 || area.getSize().y < 180;
    for(unsigned id = ID_Servers; id <= ID_Host; ++id)
        GetCtrl<Window>(id)->SetVisible(!layoutUnavailable_);
    auto* status = GetCtrl<ctrlText>(ID_Status);
    status->SetPos(area.getOrigin());
    status->setMaxWidth(area.getSize().x);
    UpdateStatus();
    if(layoutUnavailable_)
        return;
    const unsigned tableHeight = area.getSize().y - 90;
    auto* table = GetCtrl<Window>(ID_Servers);
    table->SetPos(area.getOrigin());
    table->Resize(Extent(area.getSize().x, tableHeight));
    const unsigned gap = frontend::itemGap;
    const unsigned width = (area.getSize().x - 2 * gap) / 3;
    for(unsigned i = 0; i < 3; ++i)
    {
        auto* button = GetCtrl<Window>(ID_Join + i);
        button->SetPos(area.getOrigin() + DrawPoint(i * (width + gap), tableHeight + gap));
        button->Resize(Extent(width, 36));
    }
    status->SetPos(area.getOrigin() + DrawPoint(0, tableHeight + gap + 44));
}
