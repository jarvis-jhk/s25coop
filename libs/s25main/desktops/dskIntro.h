// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Desktop.h"
#include "SmackerVideo.h"
#include "driver/EffectPlayId.h"
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

class glArchivItem_Bitmap_Raw;
class glArchivItem_Sound_Wave;

/// Plays one video of the original game (VIDEO/INTRO.SMK by default) full screen.
/// Any key, click or controller confirm/back/start skips it. Without the video it says what is missing.
class dskIntro : public Desktop
{
public:
    using NextDesktop = std::function<std::unique_ptr<Desktop>()>;

    /// next: where to go when the video ends or is skipped; the main menu if empty
    explicit dskIntro(const std::string& videoFile = "INTRO.SMK", NextDesktop next = {});
    ~dskIntro() override;

    /// Whether the original video exists and can be played, e.g. to enable the Intro button
    static bool isAvailable(const std::string& videoFile = "INTRO.SMK");

    bool WantsPadInput() const override { return true; }
    bool Msg_PadCommand(unsigned slot, PadButton button) override;

    bool Msg_LeftUp(const MouseCoords& mc) override;
    bool Msg_RightUp(const MouseCoords& mc) override;
    bool Msg_KeyDown(const KeyEvent& ke) override;

private:
    void Msg_ButtonClick(unsigned ctrl_id) override;
    void Draw_() override;
    void updateFrameTexture();
    void finish();

    SmackerVideo video_;
    NextDesktop next_;
    std::unique_ptr<glArchivItem_Bitmap_Raw> frameBmp_;
    std::unique_ptr<glArchivItem_Sound_Wave> sound_;
    EffectPlayId soundPlayId_;
    uint8_t volume_ = 0;
    bool started_ = false, finished_ = false;
    unsigned shownFrame_ = 0;
    std::chrono::steady_clock::time_point startTime_;
};
