// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dskIntro.h"

#include "Loader.h"
#include "MusicPlayer.h"
#include "Settings.h"
#include "WindowManager.h"
#include "drivers/AudioDriverWrapper.h"
#include "ogl/FontStyle.h"
#include "ogl/glArchivItem_Bitmap_Raw.h"
#include "ogl/glArchivItem_Sound_Wave.h"

#include "dskMainMenu.h"
#include "libsiedler2/PixelBufferBGRA.h"
#include <boost/format.hpp>
#include <algorithm>
#include <sstream>

namespace {
enum
{
    ID_btBack,
    ID_logo,
    ID_txtMissing
};
} // namespace

dskIntro::dskIntro(const std::string& videoFile, NextDesktop next) : Desktop(nullptr), next_(std::move(next))
{
    SetFpsDisplay(false);
    const auto path = SmackerVideo::findOriginal(videoFile);
    if(path.empty() || !video_.open(path))
    {
        // Without the video this is the old, empty intro page, plus what is missing
        background = LOADER.GetImageN("menu", 0);
        AddTextButton(ID_btBack, DrawPoint(300, 550), Extent(200, 22), TextureColor::Red1, _("Back"), NormalFont);
        AddImage(ID_logo, DrawPoint(20, 20), LOADER.GetImageN("logo", 0));
        AddText(
          ID_txtMissing, DrawPoint(400, 300),
          (boost::format(_("The video %1% of the original game was not found (folder VIDEO).")) % videoFile).str(),
          COLOR_YELLOW, FontStyle::CENTER, NormalFont);
        return;
    }

    const auto wav = video_.decodeAudioAsWav();
    // The soundtrack is music and speech; play it if either is switched on, at the louder setting
    volume_ = std::max(SETTINGS.sound.musicEnabled ? SETTINGS.sound.musicVolume : uint8_t(0),
                       SETTINGS.sound.effectsEnabled ? SETTINGS.sound.effectsVolume : uint8_t(0));
    if(!wav.empty() && volume_ > 0)
    {
        sound_ = std::make_unique<glArchivItem_Sound_Wave>();
        std::istringstream in(std::string(wav.begin(), wav.end()));
        if(sound_->load(in, static_cast<uint32_t>(wav.size())) != 0)
            sound_.reset();
    }
    frameBmp_ = std::make_unique<glArchivItem_Bitmap_Raw>();
    updateFrameTexture();
}

dskIntro::~dskIntro()
{
    if(sound_)
        AUDIODRIVER.StopEffect(soundPlayId_);
}

bool dskIntro::isAvailable(const std::string& videoFile)
{
    return !SmackerVideo::findOriginal(videoFile).empty();
}

void dskIntro::updateFrameTexture()
{
    const Extent size = video_.getSize();
    libsiedler2::PixelBufferBGRA buffer(size.x, size.y);
    std::copy(video_.getFrameBGRA().begin(), video_.getFrameBGRA().end(),
              reinterpret_cast<uint8_t*>(buffer.getPixelPtr()));
    frameBmp_->DeleteTexture();
    frameBmp_->create(buffer);
    shownFrame_ = video_.getCurrentFrame();
}

void dskIntro::Draw_()
{
    if(!video_.isOpen())
    {
        Desktop::Draw_();
        return;
    }
    if(finished_)
        return;
    if(!started_)
    {
        // Start the clock on the first frame drawn, not at construction, so loading does not eat frames
        started_ = true;
        startTime_ = std::chrono::steady_clock::now();
        MUSICPLAYER.Stop();
        if(sound_)
            soundPlayId_ = sound_->Play(volume_, false);
    }
    const auto elapsed = std::chrono::steady_clock::now() - startTime_;
    const auto target = static_cast<unsigned>(elapsed / video_.getFrameDuration());
    bool broken = false;
    while(video_.getCurrentFrame() < target)
    {
        if(!video_.nextFrame())
        {
            // Before the last frame a failed decode means a damaged file: stop instead of freezing on a picture
            broken = video_.getCurrentFrame() + 1 < video_.getNumFrames();
            break;
        }
    }
    if(broken || target >= video_.getNumFrames())
    {
        finish();
        return;
    }
    if(video_.getCurrentFrame() != shownFrame_)
        updateFrameTexture();

    const Extent screen = GetSize();
    DrawRectangle(Rect(DrawPoint(0, 0), screen), 0xFF000000);
    // The DOS videos are 320x200 (VGA mode 13h), which was shown at 4:3
    Extent disp = video_.getDisplaySize();
    const bool vga = disp.x * 10 == disp.y * 16;
    const double aspect = vga ? 4.0 / 3.0 : static_cast<double>(disp.x) / disp.y;
    Extent dst(screen.x, static_cast<unsigned>(screen.x / aspect));
    if(dst.y > screen.y)
        dst = Extent(static_cast<unsigned>(screen.y * aspect), screen.y);
    const DrawPoint pos((screen.x - dst.x) / 2, (screen.y - dst.y) / 2);
    frameBmp_->DrawFull(Rect(pos, dst));
}

void dskIntro::finish()
{
    if(finished_)
        return;
    finished_ = true;
    if(sound_)
        AUDIODRIVER.StopEffect(soundPlayId_);
    if(started_ && SETTINGS.sound.musicEnabled)
        MUSICPLAYER.Play();
    if(next_)
        WINDOWMANAGER.Switch(next_());
    else
        WINDOWMANAGER.Switch(std::make_unique<dskMainMenu>());
}

bool dskIntro::Msg_PadCommand(unsigned, const PadButton button)
{
    if(button != PadButton::A && button != PadButton::B && button != PadButton::Start)
        return false;
    // Start reaches the desktop even when a window owns focus; never bypass its confirmation.
    if(WINDOWMANAGER.GetTopMostWindow())
        return false;
    finish();
    return true;
}

bool dskIntro::Msg_LeftUp(const MouseCoords&)
{
    if(!video_.isOpen())
        return false;
    finish();
    return true;
}

bool dskIntro::Msg_RightUp(const MouseCoords&)
{
    finish();
    return true;
}

bool dskIntro::Msg_KeyDown(const KeyEvent&)
{
    finish();
    return true;
}

void dskIntro::Msg_ButtonClick(const unsigned ctrl_id)
{
    if(ctrl_id == ID_btBack)
        finish();
}
