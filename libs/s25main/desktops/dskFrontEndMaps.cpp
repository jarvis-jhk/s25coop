// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dskFrontEndMaps.h"
#include "Loader.h"
#include "RttrConfig.h"
#include "Settings.h"
#include "WindowManager.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlComboBox.h"
#include "controls/ctrlList.h"
#include "controls/ctrlPreviewMinimap.h"
#include "controls/ctrlTable.h"
#include "controls/ctrlText.h"
#include "driver/KeyEvent.h"
#include "dskFrontEndMissions.h"
#include "files.h"
#include "helpers/containerUtils.h"
#include "helpers/format.hpp"
#include "ingameWindows/iwConnecting.h"
#include "ingameWindows/iwMsgbox.h"
#include "network/CreateServerInfo.h"
#include "network/GameClient.h"
#include "gameData/MaxPlayers.h"
#include "libsiedler2/ArchivItem_Map.h"
#include "libsiedler2/ErrorCodes.h"
#include "libsiedler2/prototypen.h"
#include "s25util/colors.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace {
constexpr unsigned pathColumn = 2;
// Same nine categories as Classic; All also includes the installation's optional WORLDS directory.
const std::array<const char*, 9> folders = {{s25::folders::mapsOld, s25::folders::mapsNew, s25::folders::mapsOwn,
                                             s25::folders::mapsContinents, s25::folders::mapsCampaign,
                                             s25::folders::mapsRttr, s25::folders::mapsOther, s25::folders::mapsSea,
                                             s25::folders::mapsPlayed}};
} // namespace

dskFrontEndMaps::dskFrontEndMaps(std::shared_ptr<BrowseState> state)
    : dskFrontEndPage(_("Maps & scenarios")), state_(std::move(state))
{
    auto* category = AddComboBox(ID_Category, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, NormalFont, 150);
    for(const auto* label : {_("All maps"), _("Old maps"), _("New maps"), _("Own maps"), _("Continents"), _("Campaign"),
                             _("RTTR"), _("Other"), _("Sea"), _("Played"), _("Scenarios")})
        category->AddItem(label);
    category->SetSelection(state_->category);
    auto* players = AddComboBox(ID_PlayerFilter, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, NormalFont, 150);
    players->AddItem(_("All tribe counts"));
    for(unsigned count = 1; count <= MAX_PLAYERS; ++count)
        players->AddItem(helpers::format(_("%1% tribes"), count));
    players->SetSelection(state_->players);
    AddTable(ID_Maps, DrawPoint(0, 0), Extent(300, 200), TextureColor::Green2, NormalFont,
             {{_("Name"), 80, ctrlTable::SortType::String}, {_("Player"), 20, ctrlTable::SortType::Number}, {}});
    auto* table = GetCtrl<ctrlTable>(ID_Maps);
    // Identity is stored in a hidden column; preserve the catalog's deterministic name/path ordering.
    table->GetCtrl<ctrlButton>(1)->SetEnabled(false);
    table->GetCtrl<ctrlButton>(2)->SetEnabled(false);
    table->GetCtrl<ctrlButton>(3)->SetVisible(false);
    AddTextButton(ID_Continue, DrawPoint(0, 0), Extent(1, 1), TextureColor::Green2, _("Continue"), LargeFont);
    AddTextButton(ID_Refresh, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, _("Refresh"), NormalFont);
    AddText(ID_CategoryLabel, DrawPoint(0, 0), _("Category"), COLOR_YELLOW, FontStyle{}, NormalFont);
    AddText(ID_PlayerLabel, DrawPoint(0, 0), _("Map tribes (not controllers)"), COLOR_YELLOW, FontStyle{}, NormalFont);
    AddText(ID_Status, DrawPoint(0, 0), "", COLOR_YELLOW, FontStyle{}, NormalFont);
    AddPreviewMinimap(ID_Preview, DrawPoint(0, 0), Extent(1, 1), nullptr);
    for(unsigned id = ID_MapName; id <= ID_MapState; ++id)
        AddText(id, DrawPoint(0, 0), "", COLOR_YELLOW, FontStyle{}, NormalFont);
    SetEntryFocus(ID_Maps);
    // Clearing the table notifies selection handlers, which can reset the shared browser state.
    const auto preferredMap = state_->map;
    Refresh(preferredMap);
    Layout();
}

Window* dskFrontEndMaps::GetPadEntryCtrl(const unsigned slot)
{
    if(auto* entry = dskFrontEndPage::GetPadEntryCtrl(slot))
        return entry;
    return GetCtrl<Window>(ID_Category);
}

boost::filesystem::path dskFrontEndMaps::GetSelectedPath() const
{
    const auto* table = GetCtrl<ctrlTable>(ID_Maps);
    const auto& selection = table->GetSelection();
    return selection ? boost::filesystem::path(table->GetItemText(*selection, pathColumn)) : boost::filesystem::path{};
}

boost::filesystem::path dskFrontEndMaps::GetSelectedScenarioFolder() const
{
    const auto selection = GetCtrl<ctrlTable>(ID_Maps)->GetSelection();
    return selection && *selection < scenarios_.entries.size() ? scenarios_.entries[*selection].folder :
                                                                 boost::filesystem::path{};
}

void dskFrontEndMaps::Refresh(const boost::filesystem::path& preferred)
{
    const auto preferredScenario = state_->scenario;
    const unsigned category = *GetCtrl<ctrlComboBox>(ID_Category)->GetSelection();
    const unsigned count = *GetCtrl<ctrlComboBox>(ID_PlayerFilter)->GetSelection();
    std::vector<boost::filesystem::path> sources;
    for(unsigned i = 0; i < folders.size(); ++i)
    {
        if(category == 0 || category == i + 1)
            sources.push_back(RTTRCONFIG.ExpandPath(folders[i]));
    }
    if(category == 0 || category == 3)
        sources.push_back(RTTRCONFIG.ExpandPath("WORLDS"));
    const auto filter = count ? std::optional<unsigned>(count) : std::nullopt;
    scenarios_ = {};
    if(category == scenarioCategory)
    {
        scenarios_ = frontend::ScanScenarios(
          {RTTRCONFIG.ExpandPath(s25::folders::campaignsBuiltin), RTTRCONFIG.ExpandPath(s25::folders::campaignsUser)},
          filter);
        catalog_ = {};
        for(const auto& scenario : scenarios_.entries)
            catalog_.entries.push_back(scenario.maps.front());
        for(const auto& failure : scenarios_.failures)
            catalog_.failures.push_back({failure.path, failure.reason});
    } else
        catalog_ = frontend::ScanMaps(sources, filter);
    state_->category = category;
    state_->players = count;
    auto* table = GetCtrl<ctrlTable>(ID_Maps);
    table->DeleteAllItems();
    std::optional<unsigned> chosen;
    for(const auto& map : catalog_.entries)
    {
        const unsigned row = table->GetNumRows();
        const bool scenario = row < scenarios_.entries.size();
        if(map.path == preferred && (!scenario || scenarios_.entries[row].folder == preferredScenario))
            chosen = row;
        table->AddRow({scenario ? scenarios_.entries[row].description.name : map.name, std::to_string(map.players),
                       map.path.string()});
    }
    if(!chosen && table->GetNumRows())
        chosen = 0;
    table->SetSelection(chosen);
    UpdateStatus();
    UpdateDetails();
}

void dskFrontEndMaps::UpdateStatus()
{
    const bool scenarios = state_->category == scenarioCategory;
    GetCtrl<ctrlText>(ID_Status)->SetText(
      catalog_.entries.empty() ?
        (scenarios ? _("No matching scenarios") : _("No matching maps")) :
        helpers::format(scenarios ? _("%1% scenarios") : _("%1% maps"), catalog_.entries.size()));
    if(!catalog_.failures.empty())
        GetCtrl<ctrlText>(ID_Status)->SetText(
          helpers::format(scenarios ? _("%1% scenarios; %2% unavailable files") : _("%1% maps; %2% unavailable files"),
                          catalog_.entries.size(), catalog_.failures.size()));
}

void dskFrontEndMaps::UpdateDetails()
{
    previewReady_ = false;
    GetCtrl<ctrlButton>(ID_Continue)->SetEnabled(false);
    GetCtrl<ctrlPreviewMinimap>(ID_Preview)->SetMap(nullptr);
    const auto selected = GetSelectedPath();
    state_->map = selected;
    state_->scenario = GetSelectedScenarioFolder();
    const auto it =
      helpers::find_if(catalog_.entries, [&selected](const frontend::MapEntry& map) { return map.path == selected; });
    const bool found = it != catalog_.entries.end();
    for(unsigned id = ID_Preview; id <= ID_MapState; ++id)
        GetCtrl<Window>(id)->SetVisible(found);
    if(!found)
        return;
    GetCtrl<ctrlText>(ID_MapName)->SetText(it->name);
    GetCtrl<ctrlText>(ID_Author)->SetText(it->author);
    GetCtrl<ctrlText>(ID_Info)->SetText(
      helpers::format(_("%1% tribes — %2% × %3%"), it->players, it->width, it->height));
    GetCtrl<ctrlText>(ID_Script)->SetText(it->hasScript ? _("Map script present") : _("No map script"));
    try
    {
        // Browsing loads layers only for the selected map. Re-read even on Continue: a file may have
        // changed since the preview. A failed map stays selectable so Refresh can recover a repaired file.
        const auto fresh = frontend::ReadMapEntry(selected);
        const unsigned count = *GetCtrl<ctrlComboBox>(ID_PlayerFilter)->GetSelection();
        if(count && fresh.players != count)
            throw std::runtime_error(_("Map no longer matches the tribe filter"));
        libsiedler2::Archiv archive;
        if(const int error = libsiedler2::loader::LoadMAP(selected, archive))
            throw std::runtime_error(libsiedler2::getErrorString(error));
        const auto* map = dynamic_cast<const libsiedler2::ArchivItem_Map*>(archive[0]);
        if(!map)
            throw std::runtime_error(_("Unexpected dynamic type of map"));
        GetCtrl<ctrlPreviewMinimap>(ID_Preview)->SetMap(map);
        GetCtrl<ctrlText>(ID_MapName)->SetText(fresh.name);
        GetCtrl<ctrlText>(ID_Author)->SetText(fresh.author);
        GetCtrl<ctrlText>(ID_Info)->SetText(
          helpers::format(_("%1% tribes — %2% × %3%"), fresh.players, fresh.width, fresh.height));
        GetCtrl<ctrlText>(ID_Script)->SetText(fresh.hasScript ? _("Map script present") : _("No map script"));
        GetCtrl<ctrlText>(ID_MapState)->SetText("");
        if(!state_->scenario.empty())
        {
            const auto& campaign = scenarios_.entries.at(*GetCtrl<ctrlTable>(ID_Maps)->GetSelection()).description;
            GetCtrl<ctrlText>(ID_MapName)->SetText(campaign.name);
            GetCtrl<ctrlText>(ID_Author)->SetText(campaign.author);
            GetCtrl<ctrlText>(ID_Script)->SetText(campaign.shortDescription);
            const auto status = SETTINGS.campaigns.getMissionsStatus(campaign).front();
            GetCtrl<ctrlText>(ID_MapState)
              ->SetText(status.conquered ? _("Completed") : (status.playable ? _("Available") : _("Locked")));
        }
        previewReady_ = true;
        GetCtrl<ctrlButton>(ID_Continue)->SetEnabled(true);
    } catch(const std::runtime_error& error)
    {
        GetCtrl<ctrlText>(ID_MapState)->SetText(helpers::format(_("Map unavailable: %1%"), error.what()));
    }
}

void dskFrontEndMaps::OnLayout()
{
    auto* table = GetCtrl<ctrlTable>(ID_Maps);
    if(!table)
        return;
    const Rect& area = GetFrame().content;
    layoutUnavailable_ = area.getSize().x < 480 || area.getSize().y < 240;
    const bool found = !GetSelectedPath().empty();
    // Release each popup's region lock even when its control is about to become hidden.
    for(const unsigned id : {ID_Category, ID_PlayerFilter})
        GetCtrl<ctrlComboBox>(id)->CancelInput();
    for(unsigned id = ID_Category; id <= ID_MapState; ++id)
        GetCtrl<Window>(id)->SetVisible(!layoutUnavailable_ && (id < ID_Preview || found));
    GetCtrl<Window>(ID_Status)->SetVisible(true);
    if(layoutUnavailable_)
    {
        GetCtrl<ctrlText>(ID_Status)->SetPos(area.getOrigin());
        GetCtrl<ctrlText>(ID_Status)->setMaxWidth(area.getSize().x);
        GetCtrl<ctrlText>(ID_Status)->SetText(_("Reduce the UI scale or enlarge the window."));
        return;
    }
    UpdateStatus();
    const unsigned gap = frontend::pageMargin;
    const unsigned tableWidth = (area.getSize().x - gap) * 55 / 100;
    const unsigned detailWidth = area.getSize().x - tableWidth - gap;
    const int detailLeft = area.left + static_cast<int>(tableWidth + gap);
    const unsigned filterHeight = 60;
    const unsigned actionHeight = 36;
    const unsigned statusHeight = 28;
    const unsigned tableHeight = area.getSize().y - filterHeight - actionHeight - gap - statusHeight;
    for(const unsigned id : {ID_Category, ID_PlayerFilter})
    {
        auto* combo = GetCtrl<ctrlComboBox>(id);
        combo->SetPos(DrawPoint(id == ID_Category ? area.left : detailLeft, area.top + 24));
        combo->Resize(Extent(id == ID_Category ? tableWidth : detailWidth, 28));
        auto* list = combo->GetCtrl<ctrlList>(0);
        const auto room = static_cast<unsigned>(std::max(0, area.bottom - combo->GetDrawRect().bottom));
        list->Resize(Extent(list->GetSize().x, std::min(list->GetSize().y, room)));
    }
    GetCtrl<ctrlText>(ID_CategoryLabel)->SetPos(area.getOrigin());
    GetCtrl<ctrlText>(ID_CategoryLabel)->setMaxWidth(tableWidth);
    GetCtrl<ctrlText>(ID_PlayerLabel)->SetPos(DrawPoint(detailLeft, area.top));
    GetCtrl<ctrlText>(ID_PlayerLabel)->setMaxWidth(detailWidth);
    table->SetPos(DrawPoint(area.left, area.top + static_cast<int>(filterHeight)));
    table->Resize(Extent(tableWidth, tableHeight));
    const int actionsY = table->GetPos().y + static_cast<int>(tableHeight + gap);
    GetCtrl<Window>(ID_Continue)->SetPos(DrawPoint(area.left, actionsY));
    GetCtrl<Window>(ID_Continue)->Resize(Extent((tableWidth - gap) / 2, actionHeight));
    GetCtrl<Window>(ID_Refresh)->SetPos(DrawPoint(area.left + static_cast<int>((tableWidth + gap) / 2), actionsY));
    GetCtrl<Window>(ID_Refresh)->Resize(Extent((tableWidth - gap) / 2, actionHeight));
    GetCtrl<ctrlText>(ID_Status)->SetPos(DrawPoint(area.left, actionsY + static_cast<int>(actionHeight + 8)));
    GetCtrl<ctrlText>(ID_Status)->setMaxWidth(tableWidth);
    const unsigned previewHeight = std::min(200u, area.getSize().y - filterHeight - 5 * 24u - 8u);
    GetCtrl<Window>(ID_Preview)->SetPos(DrawPoint(detailLeft, area.top + static_cast<int>(filterHeight)));
    GetCtrl<Window>(ID_Preview)->Resize(Extent(detailWidth, previewHeight));
    for(unsigned id = ID_MapName; id <= ID_MapState; ++id)
    {
        auto* text = GetCtrl<ctrlText>(id);
        text->SetPos(DrawPoint(detailLeft,
                               area.top + static_cast<int>(filterHeight + previewHeight + 8 + (id - ID_MapName) * 24)));
        text->setMaxWidth(detailWidth);
    }
}

void dskFrontEndMaps::OnChoose(const unsigned ctrl_id)
{
    if(ctrl_id == ID_Continue || ctrl_id == ID_Maps)
        StartSelected();
    else if(ctrl_id == ID_Refresh)
        Refresh(GetSelectedPath());
}

void dskFrontEndMaps::Msg_ComboSelectItem(unsigned, unsigned)
{
    Refresh(GetSelectedPath());
}

void dskFrontEndMaps::Msg_TableSelectItem(unsigned, const std::optional<unsigned>&)
{
    UpdateDetails();
}

void dskFrontEndMaps::Msg_TableChooseItem(unsigned, unsigned)
{
    Msg_ButtonClick(ID_Maps);
}

bool dskFrontEndMaps::Msg_PadCommand(const unsigned slot, const PadButton button)
{
    if(button == PadButton::B)
    {
        for(const unsigned id : {ID_Category, ID_PlayerFilter})
        {
            if(GetCtrl<ctrlComboBox>(id)->CancelInput())
                return true;
        }
    }
    return dskFrontEndPage::Msg_PadCommand(slot, button);
}

bool dskFrontEndMaps::Msg_KeyDown(const KeyEvent& ke)
{
    for(const unsigned id : {ID_Category, ID_PlayerFilter})
    {
        auto* combo = GetCtrl<ctrlComboBox>(id);
        if(!combo->IsListOpen())
            continue;
        if(ke.kt == KeyType::Escape)
            return combo->CancelInput();
        if(ke.kt == KeyType::Return)
            return combo->Activate();
        if(ke.kt == KeyType::Up || ke.kt == KeyType::Down)
        {
            combo->DoStepValue(Position(0, ke.kt == KeyType::Up ? -1 : 1));
            return true;
        }
    }
    if(dskFrontEndPage::Msg_KeyDown(ke))
        return true;
    if(ke.kt == KeyType::Return)
    {
        Msg_ButtonClick(ID_Maps);
        return true;
    }
    return false;
}

void dskFrontEndMaps::StartSelected()
{
    if(layoutUnavailable_ || WINDOWMANAGER.IsSwitchPending() || WINDOWMANAGER.GetTopMostWindow()
       || GetSelectedPath().empty() || GetCtrl<ctrlComboBox>(ID_Category)->IsListOpen()
       || GetCtrl<ctrlComboBox>(ID_PlayerFilter)->IsListOpen())
        return;
    if(!GetSelectedScenarioFolder().empty())
    {
        try
        {
            const auto folder = GetSelectedScenarioFolder();
            const auto fresh = frontend::ReadScenario(folder);
            const auto& old = scenarios_.entries.at(*GetCtrl<ctrlTable>(ID_Maps)->GetSelection()).description;
            if(fresh.description.uid != old.uid || fresh.description.getMapFilePath(0) != GetSelectedPath()
               || fresh.description.getLuaFilePath(0) != old.getLuaFilePath(0))
                throw std::runtime_error(_("Scenario changed. Refresh before choosing a mission."));
            Open([folder] { return std::make_unique<dskFrontEndMissions>(folder); });
        } catch(const std::runtime_error& error)
        {
            WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(_("Error"), error.what(), nullptr, MsgboxButton::Ok,
                                                          MsgboxIcon::ExclamationRed));
        }
        return;
    }
    UpdateDetails();
    const CreateServerInfo csi(ServerType::Local, SETTINGS.server.localPort, _("Unlimited Play"));
    if(previewReady_ && GAMECLIENT.HostGame(csi, {GetSelectedPath(), MapType::OldMap}))
        WINDOWMANAGER.Show(std::make_unique<iwConnecting>(csi.type, nullptr));
    else
        WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(_("Error"), _("The specified file couldn't be loaded!"), nullptr,
                                                      MsgboxButton::Ok, MsgboxIcon::ExclamationRed));
}
