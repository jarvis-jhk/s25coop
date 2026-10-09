// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dskFrontEndOptions.h"
#include "Loader.h"
#include "MusicPlayer.h"
#include "Settings.h"
#include "SteamDeckUi.h"
#include "WindowManager.h"
#include "controls/ctrlComboBox.h"
#include "controls/ctrlProgress.h"
#include "controls/ctrlText.h"
#include "controls/ctrlTextButton.h"
#include "desktops/dskMainMenu.h"
#include "desktops/dskOptions.h"
#include "drivers/AudioDriverWrapper.h"
#include "drivers/VideoDriverWrapper.h"
#include "helpers/format.hpp"
#include "ingameWindows/iwMsgbox.h"
#include "ingameWindows/iwMusicPlayer.h"
#include "ingameWindows/iwTextfile.h"
#include "languages.h"
#include "mygettext/mygettext.h"
#include "ogl/glFont.h"
#include "s25util/colors.h"
#include <algorithm>

namespace {
std::string title(dskFrontEndOptions::Section section)
{
    using Section = dskFrontEndOptions::Section;
    switch(section)
    {
        case Section::Overview: return _("Options");
        case Section::Display: return _("Display");
        case Section::Audio: return _("Sound/Music");
        case Section::Controls: return _("Controls");
        case Section::Language: return _("Language");
    }
    return {};
}
std::string modeName(const VideoMode& mode)
{
    return std::to_string(mode.width) + " x " + std::to_string(mode.height);
}
} // namespace

dskFrontEndOptions::dskFrontEndOptions(const Section section) : dskFrontEndPage(title(section)), section_(section)
{
    switch(section_)
    {
        case Section::Overview:
            UseTiles(Extent(5, 2), Extent(340, 130));
            AddItem(ID_Display, _("Display / Steam Deck"));
            AddItem(ID_Audio, _("Sound/Music"));
            AddItem(ID_Controls, _("Controls"));
            AddItem(ID_Language, _("Language"));
            AddItem(ID_Advanced, _("Advanced settings"), TextureColor::Grey);
            AddItem(ID_Classic, _("Classic menus"), TextureColor::Grey);
            break;
        case Section::Display:
            AddChoice(ID_Mode, _("Mode:"), {_("Windowed"), _("Fullscreen"), _("Borderless window")},
                      rttr::enum_cast(SETTINGS.video.displayMode.type));
            AddChoice(ID_Size, _("Window size / resolution"), {}, 0);
            RefreshSize();
            AddChoice(ID_Scale, _("GUI Scale:"), {}, 0);
            RefreshScale();
            AddToggle(ID_Tv, _("TV mode:"), SETTINGS.video.tvMode);
            AddToggle(ID_Deck, _("Steam Deck UI"), SETTINGS.video.steamDeckUi);
            if(VIDEODRIVER.HasVSync())
                framerates_.push_back(0);
            framerates_.insert(framerates_.end(), Settings::SCREEN_REFRESH_RATES.begin(),
                               Settings::SCREEN_REFRESH_RATES.end());
            {
                std::vector<std::string> labels;
                unsigned selected = 0;
                for(const auto rate : framerates_)
                {
                    if(rate == SETTINGS.video.framerate)
                        selected = static_cast<unsigned>(labels.size());
                    labels.push_back(rate == 0 ? _("VSync") : rate < 0 ? _("Disabled") : std::to_string(rate) + " FPS");
                }
                AddChoice(ID_Framerate, _("Limit Framerate:"), labels, selected);
            }
            break;
        case Section::Audio:
            AddToggle(ID_Effects, _("Effects"), SETTINGS.sound.effectsEnabled);
            AddVolume(ID_EffectsVolume, _("Effects volume"), SETTINGS.sound.effectsVolume);
            AddToggle(ID_Birds, _("Bird sounds"), SETTINGS.sound.birdsEnabled);
            AddToggle(ID_Music, _("Music"), SETTINGS.sound.musicEnabled);
            AddVolume(ID_MusicVolume, _("Music volume"), SETTINGS.sound.musicVolume);
            rows_.push_back(ID_MusicPlayer);
            AddTextButton(ID_MusicPlayer, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, _("Music player"),
                          NormalFont);
            break;
        case Section::Controls:
            AddChoice(ID_Scroll, _("Map scroll mode:"), {_("Scroll same"), _("Scroll opposite"), _("Grab and drag")},
                      static_cast<unsigned>(SETTINGS.interface.mapScrollMode));
            AddToggle(ID_SmartCursor, _("Smart cursor"), SETTINGS.global.smartCursor);
            AddToggle(ID_Pinning, _("Window pinning"), SETTINGS.interface.enableWindowPinning);
            rows_.push_back(ID_KeyboardLayout);
            AddTextButton(ID_KeyboardLayout, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, _("Keyboard layout"),
                          NormalFont);
            break;
        case Section::Language:
        {
            std::vector<std::string> labels;
            unsigned selected = 0;
            for(unsigned i = 0; i < LANGUAGES.size(); ++i)
            {
                const auto& language = LANGUAGES.getLanguage(i);
                labels.push_back(_(language.name));
                if(language.code == SETTINGS.language.language)
                    selected = i;
            }
            AddChoice(ID_LanguageChoice, _("Language:"), labels, selected);
            break;
        }
    }
    Layout();
}

ctrlComboBox* dskFrontEndOptions::AddChoice(const unsigned id, const std::string& label,
                                            const std::vector<std::string>& choices, const unsigned selected)
{
    rows_.push_back(id);
    AddText(ID_LabelStart + id, DrawPoint(0, 0), label, COLOR_YELLOW, FontStyle{}, NormalFont);
    auto* combo = AddComboBox(id, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, NormalFont, 150);
    for(const auto& choice : choices)
        combo->AddItem(choice);
    combo->SetSelection(selected);
    return combo;
}

void dskFrontEndOptions::AddToggle(const unsigned id, const std::string& label, const bool value)
{
    AddChoice(id, label, {_("Off"), _("On")}, value ? 1 : 0);
}

void dskFrontEndOptions::AddVolume(const unsigned id, const std::string& label, const unsigned char value)
{
    rows_.push_back(id);
    AddText(ID_LabelStart + id, DrawPoint(0, 0), label, COLOR_YELLOW, FontStyle{}, NormalFont);
    // Use the complete byte range: merely opening Audio must not quantize a saved volume.
    AddProgress(id, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, 139, 138, 255)->SetPosition(value);
}

Window* dskFrontEndOptions::GetPadEntryCtrl(const unsigned slot)
{
    if(rows_.empty())
        return dskFrontEndPage::GetPadEntryCtrl(slot);
    return GetCtrl<Window>(rows_.front());
}

void dskFrontEndOptions::OnLayout()
{
    const auto rects = frontend::LayoutList(GetFrame().content, static_cast<unsigned>(rows_.size()), 54, 720);
    for(unsigned i = 0; i < rows_.size(); ++i)
    {
        const unsigned id = rows_[i];
        const auto& r = rects[i];
        auto* control = GetCtrl<Window>(id);
        if(auto* label = GetCtrl<ctrlText>(ID_LabelStart + id))
        {
            // Stacked labels keep translations and values apart at the minimum 800x600 size.
            label->SetPos(r.getOrigin());
            label->setMaxWidth(static_cast<unsigned short>(r.getSize().x));
            const unsigned labelHeight = NormalFont->getHeight() + 4;
            control->SetPos(r.getOrigin() + DrawPoint(0, static_cast<int>(labelHeight)));
            control->Resize(Extent(r.getSize().x, r.getSize().y > labelHeight ? r.getSize().y - labelHeight : 1));
        } else
        {
            control->SetPos(r.getOrigin());
            control->Resize(r.getSize());
        }
        if(auto* combo = dynamic_cast<ctrlComboBox*>(control))
        {
            // The classic combo opens downward. Keep its scrollable list above the shared footer.
            auto* list = combo->GetCtrl<ctrlList>(0);
            const auto room =
              static_cast<unsigned>(std::max(0, GetFrame().content.bottom - control->GetDrawRect().bottom));
            list->Resize(Extent(list->GetSize().x, std::min(list->GetSize().y, room)));
        }
    }
}

void dskFrontEndOptions::OnChoose(const unsigned id)
{
    switch(id)
    {
        case ID_Display: Open([] { return std::make_unique<dskFrontEndOptions>(Section::Display); }); break;
        case ID_Audio: Open([] { return std::make_unique<dskFrontEndOptions>(Section::Audio); }); break;
        case ID_Controls: Open([] { return std::make_unique<dskFrontEndOptions>(Section::Controls); }); break;
        case ID_Language: Open([] { return std::make_unique<dskFrontEndOptions>(Section::Language); }); break;
        // The legacy page still owns ports/proxies, driver changes, portraits and addon defaults until parity.
        case ID_Advanced: WINDOWMANAGER.Switch(std::make_unique<dskOptions>()); break;
        case ID_Classic: WINDOWMANAGER.Switch(std::make_unique<dskMainMenu>()); break;
        case ID_MusicPlayer: WINDOWMANAGER.ToggleWindow(std::make_unique<iwMusicPlayer>()); break;
        case ID_KeyboardLayout:
            WINDOWMANAGER.ToggleWindow(std::make_unique<iwTextfile>("keyboardlayout.txt", _("Keyboard layout")));
            break;
        default: break;
    }
}

void dskFrontEndOptions::RefreshScale()
{
    const auto range = VIDEODRIVER.getGuiScaleRange();
    if(SETTINGS.video.guiScale > range.maxPercent)
    {
        // A fixed scale that fitted before a resize must not clip the minimum-size interface afterwards.
        SETTINGS.video.guiScale = std::max(range.minPercent, (range.maxPercent / 10) * 10);
        VIDEODRIVER.setGuiScalePercent(SETTINGS.video.guiScale);
        SETTINGS.Save();
    }
    auto* combo = GetCtrl<ctrlComboBox>(ID_Scale);
    combo->CancelInput();
    combo->DeleteAllItems();
    scales_ = {0};
    combo->AddItem(helpers::format(_("Auto (%u%%)"), range.recommendedPercent));
    combo->SetSelection(0);
    for(unsigned percent = ((range.minPercent + 9) / 10) * 10; percent <= range.maxPercent; percent += 10)
    {
        scales_.push_back(percent);
        combo->AddItem(std::to_string(percent) + "%");
        if(percent == SETTINGS.video.guiScale)
            combo->SetSelection(combo->GetNumItems() - 1);
    }
    // Preserve a custom percentage (e.g. 125%) instead of showing Auto for an active fixed scale.
    if(SETTINGS.video.guiScale && std::find(scales_.begin(), scales_.end(), SETTINGS.video.guiScale) == scales_.end())
    {
        scales_.push_back(SETTINGS.video.guiScale);
        combo->AddItem(std::to_string(SETTINGS.video.guiScale) + "%");
        combo->SetSelection(combo->GetNumItems() - 1);
    }
    Layout();
}

void dskFrontEndOptions::RefreshSize()
{
    auto* combo = GetCtrl<ctrlComboBox>(ID_Size);
    combo->CancelInput();
    combo->DeleteAllItems();
    const bool fullscreen = SETTINGS.video.displayMode == DisplayMode::Fullscreen;
    const auto current = fullscreen ? SETTINGS.video.fullscreenSize : SETTINGS.video.windowedSize;
    const auto modes = fullscreen ? VIDEODRIVER.ListVideoModes() : VIDEODRIVER.GetDefaultWindowSizes();
    sizes_.assign(modes.begin(), modes.end());
    if(std::find(sizes_.begin(), sizes_.end(), current) == sizes_.end())
        sizes_.insert(sizes_.begin(), current);
    for(unsigned i = 0; i < sizes_.size(); ++i)
    {
        combo->AddItem(modeName(sizes_[i]));
        if(sizes_[i] == current)
            combo->SetSelection(i);
    }
    const bool show = SETTINGS.video.displayMode != DisplayMode::BorderlessWindow;
    combo->SetVisible(show);
    GetCtrl<ctrlText>(ID_LabelStart + ID_Size)->SetVisible(show);
    Layout();
}

void dskFrontEndOptions::ApplySize()
{
    const auto oldMode = VIDEODRIVER.GetDisplayMode();
    const auto oldSize = VIDEODRIVER.GetWindowSize();
    const auto target = SETTINGS.video.displayMode == DisplayMode::Fullscreen ? SETTINGS.video.fullscreenSize :
                                                                                SETTINGS.video.windowedSize;
    if(!VIDEODRIVER.ResizeScreen(target, SETTINGS.video.displayMode))
    {
        // Persist the mode the driver actually retained, rather than retrying an unusable mode at next boot.
        SETTINGS.video.displayMode = oldMode;
        if(oldMode == DisplayMode::Fullscreen)
            SETTINGS.video.fullscreenSize = oldSize;
        else if(oldMode == DisplayMode::Windowed)
            SETTINGS.video.windowedSize = oldSize;
        GetCtrl<ctrlComboBox>(ID_Mode)->SetSelection(rttr::enum_cast(oldMode.type));
        RefreshSize();
        WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(_("Error"), _("Could not change the display mode."), nullptr,
                                                      MsgboxButton::Ok, MsgboxIcon::ExclamationRed));
    }
}

void dskFrontEndOptions::Msg_ComboSelectItem(const unsigned id, const unsigned selection)
{
    switch(id)
    {
        case ID_Mode:
            SETTINGS.video.displayMode.type = static_cast<DisplayMode::Type>(selection);
            ApplySize();
            RefreshSize();
            break;
        case ID_Size:
            if(SETTINGS.video.displayMode == DisplayMode::Fullscreen)
                SETTINGS.video.fullscreenSize = sizes_.at(selection);
            else
                SETTINGS.video.windowedSize = sizes_.at(selection);
            ApplySize();
            break;
        case ID_Scale:
            SETTINGS.video.guiScale = scales_.at(selection);
            VIDEODRIVER.setGuiScalePercent(SETTINGS.video.guiScale);
            break;
        case ID_Tv:
        case ID_Deck:
            if(id == ID_Tv)
                SETTINGS.video.tvMode = selection != 0;
            else
                SETTINGS.video.steamDeckUi = selection != 0;
            // Enabling an automatic profile must have a visible effect even after a fixed scale was chosen.
            if(selection)
                SETTINGS.video.guiScale = 0;
            VIDEODRIVER.setUiReferenceHeight(
              deck::UiReferenceHeight(SETTINGS.video.tvMode, SETTINGS.video.steamDeckUi));
            VIDEODRIVER.setGuiScalePercent(SETTINGS.video.guiScale);
            RefreshScale();
            break;
        case ID_Framerate:
            SETTINGS.video.framerate = framerates_.at(selection);
            VIDEODRIVER.setTargetFramerate(SETTINGS.video.framerate);
            break;
        case ID_Effects: SETTINGS.sound.effectsEnabled = selection != 0; break;
        case ID_Birds: SETTINGS.sound.birdsEnabled = selection != 0; break;
        case ID_Music:
            SETTINGS.sound.musicEnabled = selection != 0;
            if(SETTINGS.sound.musicEnabled)
                MUSICPLAYER.Play();
            else
                MUSICPLAYER.Stop();
            break;
        case ID_Scroll: SETTINGS.interface.mapScrollMode = static_cast<MapScrollMode>(selection); break;
        case ID_SmartCursor:
            SETTINGS.global.smartCursor = selection != 0;
            VIDEODRIVER.SetMouseWarping(SETTINGS.global.smartCursor);
            break;
        case ID_Pinning: SETTINGS.interface.enableWindowPinning = selection != 0; break;
        case ID_LanguageChoice:
            SETTINGS.language.language = LANGUAGES.setLanguage(selection);
            // Keep this page's trail and input focus. Rebuilt ancestors pick up the new locale on Back.
            SetTitle(title(section_));
            GetCtrl<ctrlText>(ID_LabelStart + id)->SetText(_("Language:"));
            GetCtrl<ctrlTextButton>(ID_btBack)->SetText(_("Back"));
            GetCtrl<ctrlComboBox>(id)->SetText(0, _("System language"));
            break;
        default: return;
    }
    SETTINGS.Save();
}

void dskFrontEndOptions::Msg_ProgressChange(const unsigned id, const unsigned short position)
{
    if(id == ID_EffectsVolume)
    {
        SETTINGS.sound.effectsVolume = static_cast<uint8_t>(position);
        AUDIODRIVER.SetMasterEffectVolume(SETTINGS.sound.effectsVolume);
    } else if(id == ID_MusicVolume)
    {
        SETTINGS.sound.musicVolume = static_cast<uint8_t>(position);
        AUDIODRIVER.SetMusicVolume(SETTINGS.sound.musicVolume);
    } else
    {
        return;
    }
    SETTINGS.Save();
}

void dskFrontEndOptions::Msg_ScreenResize(const ScreenResizeEvent& sr)
{
    dskFrontEndPage::Msg_ScreenResize(sr);
    if(section_ == Section::Display)
    {
        RefreshScale();
        RefreshSize();
    }
}
