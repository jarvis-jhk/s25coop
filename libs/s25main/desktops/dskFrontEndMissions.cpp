// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dskFrontEndMissions.h"
#include "Loader.h"
#include "Settings.h"
#include "WindowManager.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlMultiline.h"
#include "controls/ctrlPreviewMinimap.h"
#include "controls/ctrlTable.h"
#include "controls/ctrlText.h"
#include "driver/KeyEvent.h"
#include "frontend/MapCatalog.h"
#include "helpers/format.hpp"
#include "ingameWindows/iwConnecting.h"
#include "ingameWindows/iwMsgbox.h"
#include "network/CreateServerInfo.h"
#include "network/GameClient.h"
#include "ogl/glFont.h"
#include "libsiedler2/ArchivItem_Map.h"
#include "libsiedler2/ErrorCodes.h"
#include "libsiedler2/prototypen.h"
#include <stdexcept>

dskFrontEndMissions::dskFrontEndMissions(boost::filesystem::path folder)
    : dskFrontEndPage(_("Choose mission")), folder_(std::move(folder))
{
    auto* table =
      AddTable(ID_Missions, DrawPoint(0, 0), Extent(300, 200), TextureColor::Green2, NormalFont,
               {{_("Mission"), 70, ctrlTable::SortType::String}, {_("Progress"), 30, ctrlTable::SortType::String}, {}});
    for(unsigned id = 1; id <= 3; ++id)
        table->GetCtrl<ctrlButton>(id)->SetEnabled(false);
    table->GetCtrl<ctrlButton>(3)->SetVisible(false);
    AddTextButton(ID_Start, DrawPoint(0, 0), Extent(1, 1), TextureColor::Green2, _("Continue"), LargeFont);
    AddTextButton(ID_Refresh, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, _("Refresh"), NormalFont);
    AddMultiline(ID_Description, DrawPoint(0, 0), Extent(300, 100), TextureColor::Grey, NormalFont);
    AddPreviewMinimap(ID_Preview, DrawPoint(0, 0), Extent(1, 1), nullptr);
    AddText(ID_Info, DrawPoint(0, 0), "", COLOR_YELLOW, FontStyle{}, NormalFont);
    AddText(ID_State, DrawPoint(0, 0), "", COLOR_YELLOW, FontStyle{}, NormalFont);
    SetEntryFocus(ID_Missions);
    Refresh();
    Layout();
}

boost::filesystem::path dskFrontEndMissions::GetSelectedPath() const
{
    const auto* table = GetCtrl<ctrlTable>(ID_Missions);
    const auto selection = table->GetSelection();
    return selection ? boost::filesystem::path(table->GetItemText(*selection, 2)) : boost::filesystem::path{};
}

void dskFrontEndMissions::Refresh()
{
    const auto previous = GetSelectedPath();
    auto* table = GetCtrl<ctrlTable>(ID_Missions);
    table->DeleteAllItems();
    campaign_.reset();
    failure_.clear();
    auto* description = GetCtrl<ctrlMultiline>(ID_Description);
    description->Clear();
    try
    {
        campaign_ = frontend::ReadCampaign(folder_);
        const auto& campaign = campaign_->description;
        SetTitle(campaign.name);
        description->AddString(campaign.author + " — " + _(campaign.difficulty), COLOR_YELLOW);
        description->AddString(campaign.longDescription, COLOR_YELLOW);
        const auto status = SETTINGS.campaigns.getMissionsStatus(campaign);
        std::optional<unsigned> chosen, firstPlayable;
        for(unsigned i = 0; i < campaign.getNumMaps(); ++i)
        {
            const auto map = frontend::ReadMapEntry(campaign.getMapFilePath(i));
            table->AddRow({helpers::format("%1%. %2%", i + 1, map.name),
                           status[i].conquered ? _("Completed") : (status[i].playable ? _("Available") : _("Locked")),
                           map.path.string()});
            if(map.path == previous)
                chosen = i;
            if(!firstPlayable && status[i].playable)
                firstPlayable = i;
        }
        table->SetSelection(chosen ? chosen : (firstPlayable ? firstPlayable : std::optional<unsigned>(0)));
    } catch(const std::runtime_error& error)
    {
        campaign_.reset();
        table->DeleteAllItems();
        failure_ = helpers::format(_("Campaign unavailable: %1%"), error.what());
    }
    UpdateDetails();
}

void dskFrontEndMissions::UpdateDetails()
{
    previewReady_ = false;
    GetCtrl<ctrlButton>(ID_Start)->SetEnabled(false);
    GetCtrl<ctrlPreviewMinimap>(ID_Preview)->SetMap(nullptr);
    GetCtrl<ctrlText>(ID_Info)->SetText("");
    auto* state = GetCtrl<ctrlText>(ID_State);
    state->SetText(failure_);
    const auto selection = GetCtrl<ctrlTable>(ID_Missions)->GetSelection();
    if(!campaign_ || !selection)
        return;
    try
    {
        const auto& campaign = campaign_->description;
        const auto map = frontend::ReadMapEntry(GetSelectedPath());
        libsiedler2::Archiv archive;
        if(const int error = libsiedler2::loader::LoadMAP(map.path, archive))
            throw std::runtime_error(libsiedler2::getErrorString(error));
        const auto* data = dynamic_cast<const libsiedler2::ArchivItem_Map*>(archive[0]);
        if(!data)
            throw std::runtime_error(_("Unexpected dynamic type of map"));
        GetCtrl<ctrlPreviewMinimap>(ID_Preview)->SetMap(data);
        GetCtrl<ctrlText>(ID_Info)->SetText(
          helpers::format(_("%1% × %2% — %3% tribes"), map.width, map.height, map.players));
        previewReady_ = true;
        const auto status = SETTINGS.campaigns.getMissionsStatus(campaign).at(*selection);
        state->SetText(status.conquered ? _("Completed") : (status.playable ? _("Available") : _("Locked")));
        GetCtrl<ctrlButton>(ID_Start)->SetEnabled(status.playable);
    } catch(const std::runtime_error& error)
    {
        state->SetText(helpers::format(_("Mission unavailable: %1%"), error.what()));
    }
}

void dskFrontEndMissions::OnLayout()
{
    auto* table = GetCtrl<ctrlTable>(ID_Missions);
    if(!table)
        return;
    const auto area = GetFrame().content;
    layoutUnavailable_ = area.getSize().x < 480 || area.getSize().y < 260;
    for(unsigned id = ID_Missions; id <= ID_Info; ++id)
        GetCtrl<Window>(id)->SetVisible(!layoutUnavailable_);
    auto* state = GetCtrl<ctrlText>(ID_State);
    state->setMaxWidth(area.getSize().x);
    if(layoutUnavailable_)
    {
        state->SetPos(area.getOrigin());
        state->SetText(_("Reduce the UI scale or enlarge the window."));
        return;
    }
    const unsigned leftWidth = (area.getSize().x - frontend::pageMargin) * 55 / 100;
    const unsigned rightWidth = area.getSize().x - leftWidth - frontend::pageMargin;
    const int right = area.left + static_cast<int>(leftWidth + frontend::pageMargin);
    table->SetPos(area.getOrigin());
    table->Resize(Extent(leftWidth, area.getSize().y - 56));
    for(unsigned id = ID_Start; id <= ID_Refresh; ++id)
    {
        GetCtrl<Window>(id)->SetPos(
          DrawPoint(area.left + static_cast<int>((id - ID_Start) * (leftWidth + 12) / 2), area.bottom - 36));
        GetCtrl<Window>(id)->Resize(Extent((leftWidth - 12) / 2, 36));
    }
    const unsigned previewHeight = std::min(180u, (area.getSize().y - 72) / 2);
    GetCtrl<Window>(ID_Preview)->SetPos(DrawPoint(right, area.top));
    GetCtrl<Window>(ID_Preview)->Resize(Extent(rightWidth, previewHeight));
    GetCtrl<ctrlText>(ID_Info)->SetPos(DrawPoint(right, area.top + static_cast<int>(previewHeight + 8)));
    GetCtrl<ctrlText>(ID_Info)->setMaxWidth(rightWidth);
    state->SetPos(DrawPoint(right, area.top + static_cast<int>(previewHeight + 32)));
    state->setMaxWidth(rightWidth);
    GetCtrl<Window>(ID_Description)->SetPos(DrawPoint(right, area.top + static_cast<int>(previewHeight + 64)));
    GetCtrl<Window>(ID_Description)->Resize(Extent(rightWidth, area.getSize().y - previewHeight - 64));
    UpdateDetails();
}

void dskFrontEndMissions::OnChoose(const unsigned id)
{
    if(id == ID_Start)
        StartSelected();
    else if(id == ID_Refresh)
        Refresh();
}

void dskFrontEndMissions::Msg_TableSelectItem(unsigned, const std::optional<unsigned>&)
{
    UpdateDetails();
}

void dskFrontEndMissions::Msg_TableChooseItem(unsigned, unsigned)
{
    StartSelected();
}

bool dskFrontEndMissions::Msg_KeyDown(const KeyEvent& ke)
{
    if(dskFrontEndPage::Msg_KeyDown(ke))
        return true;
    if(ke.kt == KeyType::Return)
    {
        StartSelected();
        return true;
    }
    return false;
}

void dskFrontEndMissions::StartSelected()
{
    if(layoutUnavailable_ || WINDOWMANAGER.IsSwitchPending() || WINDOWMANAGER.GetTopMostWindow() || !campaign_)
        return;
    const auto selection = GetCtrl<ctrlTable>(ID_Missions)->GetSelection();
    if(!selection)
        return;
    try
    {
        const auto fresh = frontend::ReadCampaign(folder_);
        const auto& old = campaign_->description;
        const auto& campaign = fresh.description;
        // A replaced/reordered description must not turn the highlighted mission into a different game.
        if(*selection >= campaign.getNumMaps() || campaign.uid != old.uid
           || campaign.getMapFilePath(*selection) != GetSelectedPath()
           || campaign.getLuaFilePath(*selection) != old.getLuaFilePath(*selection))
            throw std::runtime_error(_("Campaign changed. Refresh before choosing a mission."));
        UpdateDetails();
        if(!SETTINGS.campaigns.isChapterPlayable(campaign, *selection))
            return;
        if(!previewReady_)
            throw std::runtime_error(_("The specified file couldn't be loaded!"));
        const CreateServerInfo csi(ServerType::Local, SETTINGS.server.localPort, campaign.name);
        if(!GAMECLIENT.HostGame(csi, {GetSelectedPath(), MapType::OldMap, campaign.getLuaFilePath(*selection)}))
            throw std::runtime_error(_("Hosting of game not possible"));
        // The existing local-seat bridge shares a campaign's human tribe with the Title party.
        GAMECLIENT.SetHostingCampaign(true);
        auto& connecting = WINDOWMANAGER.Show(std::make_unique<iwConnecting>(csi.type, nullptr));
        onErrorConnection_ = connecting.onError.connect([this](ClientError error) {
            WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(_("Error"), ClientErrorToStr(error), this, MsgboxButton::Ok,
                                                          MsgboxIcon::ExclamationRed));
        });
    } catch(const std::runtime_error& error)
    {
        WINDOWMANAGER.Show(
          std::make_unique<iwMsgbox>(_("Error"), error.what(), this, MsgboxButton::Ok, MsgboxIcon::ExclamationRed));
    }
}
