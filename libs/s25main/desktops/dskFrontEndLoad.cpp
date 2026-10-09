// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dskFrontEndLoad.h"
#include "Loader.h"
#include "RttrConfig.h"
#include "Settings.h"
#include "WindowManager.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlTable.h"
#include "controls/ctrlText.h"
#include "driver/KeyEvent.h"
#include "files.h"
#include "helpers/containerUtils.h"
#include "helpers/format.hpp"
#include "ingameWindows/iwConnecting.h"
#include "ingameWindows/iwMsgbox.h"
#include "network/CreateServerInfo.h"
#include "network/GameClient.h"
#include "s25util/colors.h"
#include <algorithm>

namespace {
constexpr unsigned pathColumn = 2;
std::string saveDate(const s25util::time64_t time)
{
    return s25util::Time::FormatTime("%d.%m.%Y - %H:%i", time);
}
} // namespace

dskFrontEndLoad::dskFrontEndLoad(const boost::filesystem::path& preferred)
    : dskFrontEndPage(_("Load game")), resumeOnce_(preferred)
{
    AddTable(ID_Saves, DrawPoint(0, 0), Extent(300, 200), TextureColor::Green2, NormalFont,
             {{_("Filename"), 55, ctrlTable::SortType::String}, {_("Time"), 45, ctrlTable::SortType::Date}, {}});
    // Keep exact timestamp order from the catalog. Minute-formatted date strings lose that ordering;
    // the table's sort buttons also do not notify a parent when selection's identity changes.
    auto* table = GetCtrl<ctrlTable>(ID_Saves);
    table->GetCtrl<ctrlButton>(1)->SetEnabled(false);
    table->GetCtrl<ctrlButton>(2)->SetEnabled(false);
    table->GetCtrl<ctrlButton>(3)->SetVisible(false);
    AddTextButton(ID_Load, DrawPoint(0, 0), Extent(1, 1), TextureColor::Green2, _("Load game"), LargeFont);
    AddTextButton(ID_Refresh, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, _("Refresh"), NormalFont);
    for(unsigned id = ID_Status; id <= ID_Coop; ++id)
        AddText(id, DrawPoint(0, 0), "", COLOR_YELLOW, FontStyle{}, NormalFont);
    GetCtrl<ctrlText>(ID_Preview)->SetText(_("No preview stored"));
    GetCtrl<ctrlText>(ID_Coop)->SetText(_("Joined players share the saved human tribe."));
    SetEntryFocus(ID_Saves);
    Refresh(preferred);
    Layout();
}

void dskFrontEndLoad::SetActive(const bool activate)
{
    dskFrontEndPage::SetActive(activate);
    if(activate && WINDOWMANAGER.GetCurrentDesktop() == this && !resumeOnce_.empty())
    {
        const auto requested = std::move(resumeOnce_);
        resumeOnce_.clear();
        if(GetSelectedPath() == requested)
            LoadSelected();
        else
            WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(_("Error"), _("The specified file couldn't be loaded!"),
                                                          nullptr, MsgboxButton::Ok, MsgboxIcon::ExclamationRed));
    }
}

Window* dskFrontEndLoad::GetPadEntryCtrl(const unsigned slot)
{
    if(auto* entry = dskFrontEndPage::GetPadEntryCtrl(slot))
        return entry;
    return GetCtrl<Window>(ID_Refresh);
}

boost::filesystem::path dskFrontEndLoad::GetSelectedPath() const
{
    const auto* table = GetCtrl<ctrlTable>(ID_Saves);
    const auto& selection = table->GetSelection();
    return selection ? boost::filesystem::path(table->GetItemText(*selection, pathColumn)) : boost::filesystem::path{};
}

void dskFrontEndLoad::Refresh(const boost::filesystem::path& preferred)
{
    catalog_ = frontend::ScanSaves(RTTRCONFIG.ExpandPath(s25::folders::save));
    auto* table = GetCtrl<ctrlTable>(ID_Saves);
    table->DeleteAllItems();
    std::optional<unsigned> chosen;
    for(const auto& save : catalog_.entries)
    {
        if(save.path == preferred)
            chosen = table->GetNumRows();
        table->AddRow({save.path.stem().string(), saveDate(save.savedAt), save.path.string()});
    }
    if(!chosen && table->GetNumRows())
        chosen = 0;
    table->SetSelection(chosen);
    GetCtrl<ctrlText>(ID_Status)->SetText(
      catalog_.entries.empty() ? _("No saved games") : helpers::format(_("%1% saved games"), catalog_.entries.size()));
    if(catalog_.rejected)
        GetCtrl<ctrlText>(ID_Status)->SetText(
          helpers::format(_("%1% saved games; %2% unavailable files"), catalog_.entries.size(), catalog_.rejected));
    UpdateDetails();
}

void dskFrontEndLoad::UpdateDetails()
{
    const auto selected = GetSelectedPath();
    const auto it = helpers::find_if(catalog_.entries,
                                     [&selected](const frontend::SaveEntry& save) { return save.path == selected; });
    const bool found = it != catalog_.entries.end();
    GetCtrl<ctrlButton>(ID_Load)->SetEnabled(found);
    for(unsigned id = ID_Map; id <= ID_Coop; ++id)
        GetCtrl<Window>(id)->SetVisible(found);
    if(!found)
        return;
    GetCtrl<ctrlText>(ID_Map)->SetText(it->map);
    GetCtrl<ctrlText>(ID_Date)->SetText(saveDate(it->savedAt));
    GetCtrl<ctrlText>(ID_GameTime)
      ->SetText(helpers::format(_("Game Time: %1%"), GAMECLIENT.FormatGFTime(it->gameFrame)));
    GetCtrl<ctrlText>(ID_Players)->SetText(helpers::format(_("%1% human tribes, %2% AI tribes"), it->humans, it->ais));
    std::string names;
    for(const auto& player : it->players)
    {
        if(!names.empty())
            names += ", ";
        names += player;
    }
    GetCtrl<ctrlText>(ID_Names)->SetText(names);
    GetCtrl<ctrlText>(ID_Coop)->SetVisible(it->humans == 1);
}

void dskFrontEndLoad::OnLayout()
{
    auto* table = GetCtrl<ctrlTable>(ID_Saves);
    if(!table)
        return;
    const Rect& area = GetFrame().content;
    const unsigned gap = frontend::pageMargin;
    const unsigned tableWidth = (area.getSize().x - gap) * 55 / 100;
    const unsigned actionHeight = 36;
    const unsigned statusHeight = 28;
    const unsigned tableHeight = area.getSize().y - actionHeight - gap - statusHeight;
    table->SetPos(area.getOrigin());
    table->Resize(Extent(tableWidth, tableHeight));
    const int actionsY = area.top + static_cast<int>(tableHeight + gap);
    GetCtrl<Window>(ID_Load)->SetPos(DrawPoint(area.left, actionsY));
    GetCtrl<Window>(ID_Load)->Resize(Extent((tableWidth - gap) / 2, actionHeight));
    GetCtrl<Window>(ID_Refresh)->SetPos(DrawPoint(area.left + static_cast<int>((tableWidth + gap) / 2), actionsY));
    GetCtrl<Window>(ID_Refresh)->Resize(Extent((tableWidth - gap) / 2, actionHeight));
    GetCtrl<Window>(ID_Status)->SetPos(DrawPoint(area.left, actionsY + static_cast<int>(actionHeight + 8)));
    GetCtrl<ctrlText>(ID_Status)->setMaxWidth(tableWidth);
    const int detailLeft = area.left + static_cast<int>(tableWidth + gap);
    const unsigned detailWidth = area.getSize().x - tableWidth - gap;
    for(unsigned id = ID_Map; id <= ID_Coop; ++id)
    {
        auto* text = GetCtrl<ctrlText>(id);
        text->SetPos(DrawPoint(detailLeft, area.top + static_cast<int>((id - ID_Map) * 36)));
        text->setMaxWidth(detailWidth);
    }
}

void dskFrontEndLoad::OnChoose(const unsigned ctrl_id)
{
    if(ctrl_id == ID_Load)
        LoadSelected();
    else if(ctrl_id == ID_Refresh)
        Refresh(GetSelectedPath());
}

void dskFrontEndLoad::Msg_TableSelectItem(unsigned, const std::optional<unsigned>&)
{
    UpdateDetails();
}

void dskFrontEndLoad::Msg_TableChooseItem(unsigned, unsigned)
{
    LoadSelected();
}

bool dskFrontEndLoad::Msg_KeyDown(const KeyEvent& ke)
{
    if(dskFrontEndPage::Msg_KeyDown(ke))
        return true;
    if(ke.kt == KeyType::Return)
    {
        LoadSelected();
        return true;
    }
    return false;
}

void dskFrontEndLoad::LoadSelected()
{
    if(WINDOWMANAGER.IsSwitchPending() || WINDOWMANAGER.GetTopMostWindow())
        return;
    const auto selected = GetSelectedPath();
    if(selected.empty())
        return;
    // Metadata may have disappeared or changed while the page was open. Failure stays on this page;
    // no map-picker switch, and a second A in this input batch cannot start another connection.
    const bool readable = frontend::ReadSaveEntry(selected).has_value();
    const CreateServerInfo csi(ServerType::Local, SETTINGS.server.localPort, selected.stem().string());
    if(readable && GAMECLIENT.HostGame(csi, {selected, MapType::Savegame}))
    {
        WINDOWMANAGER.Show(std::make_unique<iwConnecting>(csi.type, nullptr));
    } else
    {
        Refresh(selected);
        WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(_("Error"), _("The specified file couldn't be loaded!"), nullptr,
                                                      MsgboxButton::Ok, MsgboxIcon::ExclamationRed));
    }
}
