// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "iwAddons.h"
#include "GlobalGameSettings.h"
#include "Loader.h"
#include "WindowManager.h"
#include "addons/Addon.h"
#include "commonDefines.h"
#include "controls/ctrlCheck.h"
#include "controls/ctrlComboBox.h"
#include "controls/ctrlOptionGroup.h"
#include "controls/ctrlScrollBar.h"
#include "controls/ctrlText.h"
#include "helpers/containerUtils.h"
#include "iwAddonPresets.h"
#include "gameData/const_gui_ids.h"
#include "s25util/colors.h"
#include <algorithm>
#include <array>
#include <utility>

namespace {
enum
{
    ID_txtAddFeatures,
    ID_btApply,
    ID_btAbort,
    ID_btS2Defaults,
    ID_btSavePreset,
    ID_btLoadPreset,
    ID_grpAddonGroup,
    ID_scroll,
    ID_grpAddonsStart = 8,
    ID_changedOnly = 1000,
    ID_showDeveloper,
    ID_empty,
    ID_tabHint
};
/// Breite der Scrollbar
constexpr unsigned SCROLLBAR_WIDTH = 20;
constexpr unsigned AddonGuiLineHeight = 30;

} // namespace

iwAddons::iwAddons(GlobalGameSettings& ggs, Window* parent, AddonChangeAllowed policy,
                   std::vector<AddonId> whitelistedAddons)
    : IngameWindow(CGI_ADDONS, IngameWindow::posLastOrCenter, Extent(700, 530), _("Addon Settings"),
                   LOADER.GetImageN("resource", 41), true, CloseBehavior::Custom, parent),
      ggs(ggs), policy_(policy), whitelistedAddons_(std::move(whitelistedAddons))
{
    AddText(ID_txtAddFeatures, DrawPoint(20, 30), _("Additional features:"), COLOR_YELLOW, FontStyle{}, NormalFont);

    Extent btSize(200, 22);
    if(policy != AddonChangeAllowed::None)
    {
        AddTextButton(ID_btSavePreset, DrawPoint(20, GetSize().y - 70), btSize, TextureColor::Green2, _("Save"),
                      NormalFont, _("Save Addon Preset"));
        AddTextButton(ID_btLoadPreset, DrawPoint(250, GetSize().y - 70), btSize, TextureColor::Green2, _("Load"),
                      NormalFont, _("Load Addon Preset"));
        AddTextButton(ID_btS2Defaults, DrawPoint(480, GetSize().y - 70), btSize, TextureColor::Grey, _("Default"),
                      NormalFont, _("Use S2 Defaults"));
        AddTextButton(ID_btApply, DrawPoint(20, GetSize().y - 40), btSize, TextureColor::Green2, _("Apply"), NormalFont,
                      _("Apply Changes"));
    }

    AddTextButton(ID_btAbort, DrawPoint(250, GetSize().y - 40), btSize, TextureColor::Red1, _("Abort"), NormalFont,
                  _("Close Without Saving"));

    auto* optiongroup = AddOptionGroup(ID_grpAddonGroup, GroupSelectType::Check);
    const std::array<const char*, 6> labels = {
      {_("All"), _("Comfort"), _("New content"), _("World & economy"), _("Combat"), _("Easier")}};
    const std::array<unsigned, 6> widths = {{55, 90, 120, 165, 90, 100}};
    int x = 20;
    for(unsigned i = 0; i < labels.size(); ++i)
    {
        optiongroup->AddTextButton(i, DrawPoint(x, 50), Extent(widths[i], 22), TextureColor::Green2, labels[i],
                                   NormalFont);
        x += widths[i] + 5;
    }
    optiongroup
      ->AddTextButton(static_cast<unsigned>(AddonCategory::Developer), DrawPoint(520, 78), Extent(160, 22),
                      TextureColor::Grey, _("Developer"), NormalFont)
      ->SetVisible(false);
    AddCheckBox(ID_changedOnly, DrawPoint(20, 78), Extent(210, 22), TextureColor::Grey, _("Changed only"), NormalFont,
                false);
    AddCheckBox(ID_showDeveloper, DrawPoint(240, 78), Extent(265, 22), TextureColor::Grey, _("Show developer addons"),
                NormalFont, false);
    AddText(ID_empty, DrawPoint(40, 125), _("No matching addons"), COLOR_YELLOW, FontStyle{}, NormalFont)
      ->SetVisible(false);
    AddText(ID_tabHint, DrawPoint(20, GetSize().y - 96), _("LB / RB: category"), COLOR_YELLOW, FontStyle{}, NormalFont);

    ctrlScrollBar* scrollbar =
      AddScrollBar(ID_scroll, DrawPoint(GetSize().x - SCROLLBAR_WIDTH - 20, 110),
                   Extent(SCROLLBAR_WIDTH, GetSize().y - 210), SCROLLBAR_WIDTH, TextureColor::Green2, 1);
    scrollbar->SetPageSize(scrollbar->GetSize().y / AddonGuiLineHeight);

    for(unsigned i = 0; i < ggs.getNumAddons(); ++i)
    {
        const unsigned id = ID_grpAddonsStart + i;
        const Addon& addon = assertNonNull(ggs.getAddon(i));
        addonGuis_.emplace_back(addon.createGui(*AddGroup(id), isReadOnly(addon.getId())));
        const unsigned status = ggs.getSelection(addon.getId());
        stagedStatuses_.push_back(status < addon.getNumOptions() ? status : addon.getDefaultStatus());
        addonGuis_.back()->setStatus(stagedStatuses_.back());
        addonGuis_.back()->getWindow().GetCtrl<ctrlText>(0)->setMaxWidth(290);
        addonGuis_.back()
          ->getWindow()
          .AddText(4, DrawPoint(345, 4), "", COLOR_YELLOW, FontStyle{}, NormalFont)
          ->setMaxWidth(75);
    }

    optiongroup->SetSelection(static_cast<unsigned>(AddonCategory::All), true);
}

iwAddons::~iwAddons() = default;

void iwAddons::Close()
{
    // Close an open save/load preset window: the load window holds a callback into this window,
    // so it must not outlive it
    WINDOWMANAGER.Close(CGI_ADDON_PRESETS, GetOwner());
    IngameWindow::Close();
}

void iwAddons::Msg_ButtonClick(const unsigned ctrl_id)
{
    switch(ctrl_id)
    {
        default: break;

        case ID_btApply:
        {
            if(policy_ != AddonChangeAllowed::None)
            {
                for(unsigned i = 0; i < addonGuis_.size(); ++i)
                    ggs.setSelection(addonGuis_[i]->getAddon().getId(), stagedStatuses_[i]);

                switch(policy_)
                {
                    default: break;
                    case AddonChangeAllowed::AllAndSaveToConfig: ggs.SaveSettings(); break;
                    case AddonChangeAllowed::All:
                    case AddonChangeAllowed::WhitelistOnly:
                        // send message via msgboxresult
                        GetParent()->Msg_MsgBoxResult(GetID(), MsgboxResult::Yes);
                        break;
                }
            }
            Close();
        }
        break;

        case ID_btAbort: // Discard changes
            Close();
            break;

        case ID_btSavePreset:
        {
            std::map<unsigned, unsigned> states;
            for(unsigned i = 0; i < addonGuis_.size(); ++i)
                states[static_cast<unsigned>(addonGuis_[i]->getAddon().getId())] = stagedStatuses_[i];
            WINDOWMANAGER.Show(std::make_unique<iwSaveAddonPreset>(std::move(states)));
        }
        break;

        case ID_btLoadPreset:
            WINDOWMANAGER.Show(std::make_unique<iwLoadAddonPreset>(
              [this](const std::map<unsigned, unsigned>& states) { applyAddonStates(states); }));
            break;

        case ID_btS2Defaults: // Load S2 Defaults
            if(auto* combo = OpenDropdown())
                combo->CancelInput();
            for(unsigned i = 0; i < addonGuis_.size(); ++i)
            {
                const Addon& addon = addonGuis_[i]->getAddon();
                if(!isReadOnly(addon.getId()))
                {
                    stagedStatuses_[i] = addon.getDefaultStatus();
                    addonGuis_[i]->setStatus(stagedStatuses_[i]);
                }
            }
            RefreshFilter();
            break;
    }
}

void iwAddons::UpdateView()
{
    const auto selection = static_cast<AddonCategory>(GetCtrl<ctrlOptionGroup>(ID_grpAddonGroup)->GetSelection());
    const bool changedOnly = GetCtrl<ctrlCheck>(ID_changedOnly)->isChecked();
    const bool showDeveloper = GetCtrl<ctrlCheck>(ID_showDeveloper)->isChecked();
    std::vector<Window*> matches;
    for(unsigned i = 0; i < addonGuis_.size(); ++i)
    {
        auto& gui = *addonGuis_[i];
        const auto& addon = gui.getAddon();
        const auto category = GetAddonCategory(addon.getId()).value_or(AddonCategory::Developer);
        const bool matchesCategory = selection == AddonCategory::All || category == selection;
        const bool matchesChanged = !changedOnly || stagedStatuses_[i] != addon.getDefaultStatus();
        if(matchesCategory && matchesChanged && (showDeveloper || category != AddonCategory::Developer))
            matches.push_back(&gui.getWindow());
        gui.getWindow().SetVisible(false);
        auto* effect = gui.getWindow().GetCtrl<ctrlText>(4);
        switch(GetAddonDifficulty(addon, stagedStatuses_[i]))
        {
            case AddonDifficulty::Neutral: effect->SetText(""); break;
            case AddonDifficulty::Easier: effect->SetText(_("Easier")); break;
            case AddonDifficulty::Harder: effect->SetText(_("Harder")); break;
        }
    }
    // Range must be updated before reading the position: a filter can shorten the last page.
    auto* scrollbar = GetCtrl<ctrlScrollBar>(ID_scroll);
    scrollbar->SetRange(static_cast<unsigned short>(matches.size()));
    const unsigned start = scrollbar->GetScrollPos();
    const unsigned end = std::min<unsigned>(matches.size(), start + scrollbar->GetPageSize());
    for(unsigned i = start; i < end; ++i)
    {
        matches[i]->SetPos({0, 110 + static_cast<int>((i - start) * AddonGuiLineHeight)});
        matches[i]->SetVisible(true);
    }
    GetCtrl<ctrlText>(ID_empty)->SetVisible(matches.empty());
}

void iwAddons::RefreshFilter()
{
    if(auto* combo = OpenDropdown())
        combo->CancelInput();
    GetCtrl<ctrlScrollBar>(ID_scroll)->SetScrollPos(0);
    UpdateView();
}

Window* iwAddons::OpenDropdown() const
{
    for(const auto& gui : addonGuis_)
    {
        auto* combo = gui->getWindow().GetCtrl<ctrlComboBox>(2);
        if(combo && combo->IsListOpen())
            return combo;
    }
    return nullptr;
}

Window* iwAddons::SwitchPadTab(const int direction)
{
    // Browsing is still pending: neither shoulder may silently accept or discard it.
    if(auto* combo = OpenDropdown())
        return combo;
    auto* tabs = GetCtrl<ctrlOptionGroup>(ID_grpAddonGroup);
    const unsigned count = GetCtrl<ctrlCheck>(ID_showDeveloper)->isChecked() ? 7 : 6;
    const unsigned current = tabs->GetSelection();
    const unsigned next = direction < 0 ? (current + count - 1) % count : (current + 1) % count;
    tabs->SetSelection(next, true);
    return tabs->GetCtrl<ctrlButton>(next);
}

void iwAddons::Msg_CheckboxChange(const unsigned ctrl_id, const bool checked)
{
    if(ctrl_id == ID_showDeveloper)
    {
        auto* tabs = GetCtrl<ctrlOptionGroup>(ID_grpAddonGroup);
        tabs->GetCtrl<ctrlButton>(static_cast<unsigned>(AddonCategory::Developer))->SetVisible(checked);
        if(!checked && tabs->GetSelection() == static_cast<unsigned>(AddonCategory::Developer))
            tabs->SetSelection(static_cast<unsigned>(AddonCategory::All));
    }
    RefreshFilter();
}

void iwAddons::Msg_Group_CheckboxChange(const unsigned group_id, unsigned /*ctrl_id*/, const bool checked)
{
    stagedStatuses_.at(group_id - ID_grpAddonsStart) = checked ? 1 : 0;
    UpdateView();
}

void iwAddons::Msg_Group_ComboSelectItem(const unsigned group_id, unsigned /*ctrl_id*/, const unsigned selection)
{
    stagedStatuses_.at(group_id - ID_grpAddonsStart) = selection;
    UpdateView();
}

void iwAddons::applyAddonStates(const std::map<unsigned, unsigned>& states)
{
    if(auto* combo = OpenDropdown())
        combo->CancelInput();
    for(unsigned i = 0; i < addonGuis_.size(); ++i)
    {
        auto& gui = addonGuis_[i];
        const Addon& addon = gui->getAddon();
        if(!isReadOnly(addon.getId()))
        {
            const auto it = states.find(static_cast<unsigned>(addon.getId()));
            const unsigned rawStatus = (it != states.end()) ? it->second : addon.getDefaultStatus();
            const unsigned status = (rawStatus < addon.getNumOptions()) ? rawStatus : addon.getDefaultStatus();
            stagedStatuses_[i] = status;
            gui->setStatus(status);
        }
    }
    RefreshFilter();
}

bool iwAddons::isReadOnly(AddonId id) const
{
    return policy_ == AddonChangeAllowed::None
           || (policy_ == AddonChangeAllowed::WhitelistOnly && !helpers::contains(whitelistedAddons_, id));
}

void iwAddons::Msg_OptionGroupChange(const unsigned ctrl_id, const unsigned selection)
{
    if(ctrl_id == ID_grpAddonGroup)
    {
        (void)selection;
        RefreshFilter();
    }
}

/**
 *  get scrollbar notification
 */
void iwAddons::Msg_ScrollChange(const unsigned /*ctrl_id*/, const unsigned short /*position*/)
{
    UpdateView();
}

bool iwAddons::Msg_WheelUp(const MouseCoords& /*mc*/)
{
    if(!OpenDropdown())
        GetCtrl<ctrlScrollBar>(ID_scroll)->Scroll(-2);
    return true;
}

bool iwAddons::Msg_WheelDown(const MouseCoords& /*mc*/)
{
    if(!OpenDropdown())
        GetCtrl<ctrlScrollBar>(ID_scroll)->Scroll(+2);
    return true;
}
