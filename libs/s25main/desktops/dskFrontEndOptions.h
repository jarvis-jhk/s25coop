// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "driver/VideoMode.h"
#include "dskFrontEndPage.h"
#include <vector>

class ctrlComboBox;

/// F9: small grouped pages, sharing the front end's trail and physical control behavior.
class dskFrontEndOptions : public dskFrontEndPage
{
public:
    enum class Section
    {
        Overview,
        Display,
        Audio,
        Controls,
        Language
    };
    explicit dskFrontEndOptions(Section section = Section::Overview);
    Section GetSection() const { return section_; }
    Window* GetPadEntryCtrl(unsigned slot) override;
    void Msg_ScreenResize(const ScreenResizeEvent& sr) override;

    enum ControlIds
    {
        ID_Display = ID_FIRST_FREE,
        ID_Audio,
        ID_Controls,
        ID_Language,
        ID_Advanced,
        ID_Classic,
        ID_Mode,
        ID_Size,
        ID_Scale,
        ID_Tv,
        ID_Deck,
        ID_Framerate,
        ID_Effects,
        ID_EffectsVolume,
        ID_Birds,
        ID_Music,
        ID_MusicVolume,
        ID_MusicPlayer,
        ID_Scroll,
        ID_SmartCursor,
        ID_Pinning,
        ID_KeyboardLayout,
        ID_LanguageChoice,
        ID_LabelStart = 100
    };

private:
    void OnChoose(unsigned id) override;
    void OnLayout() override;
    void Msg_ComboSelectItem(unsigned id, unsigned selection) override;
    void Msg_ProgressChange(unsigned id, unsigned short position) override;
    ctrlComboBox* AddChoice(unsigned id, const std::string& label, const std::vector<std::string>& choices,
                            unsigned selected);
    void AddToggle(unsigned id, const std::string& label, bool value);
    void AddVolume(unsigned id, const std::string& label, unsigned char value);
    void RefreshScale();
    void RefreshSize();
    void ApplySize();

    Section section_;
    std::vector<unsigned> rows_;
    std::vector<unsigned> scales_;
    std::vector<VideoMode> sizes_;
    std::vector<short> framerates_;
};
