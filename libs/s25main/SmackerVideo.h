// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Point.h"
#include <boost/filesystem/path.hpp>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct smk_t;

/// A Smacker video (.SMK) of the original game, decoded with libsmacker.
/// Frames come out as 32-bit BGRA, the first audio track as a complete RIFF WAVE file.
class SmackerVideo
{
public:
    SmackerVideo();
    ~SmackerVideo();
    SmackerVideo(const SmackerVideo&) = delete;
    SmackerVideo& operator=(const SmackerVideo&) = delete;

    /// Path of a video of the original game (<RTTR_GAME>/VIDEO/<fileName>), matched case-insensitively
    /// because copies from GOG or disc images differ in case. Empty if there is none.
    static boost::filesystem::path findOriginal(const std::string& fileName);

    /// Loads the file and decodes the first frame. False (and a log line) if it is not a Smacker video.
    bool open(const boost::filesystem::path& file);
    bool isOpen() const { return smk_ != nullptr; }

    unsigned getNumFrames() const { return numFrames_; }
    std::chrono::microseconds getFrameDuration() const { return frameDuration_; }
    /// Size of one frame in pixels
    Extent getSize() const { return size_; }
    /// Size it is meant to be shown at: Smacker can ask for doubled or interlaced lines
    Extent getDisplaySize() const { return displaySize_; }

    /// Index of the frame that the last call decoded
    unsigned getCurrentFrame() const { return curFrame_; }
    /// BGRA pixels (getSize().x * getSize().y) of the current frame
    const std::vector<uint8_t>& getFrameBGRA() const { return frame_; }
    /// Decodes the next frame. False at the end of the video or on an error.
    bool nextFrame();

    /// All audio of the first track as a WAV file (header + PCM). Empty if the video has no sound.
    /// Decodes the whole file a second time, so call it once, before playback.
    std::vector<uint8_t> decodeAudioAsWav() const;

private:
    void convertFrame();

    std::vector<unsigned char> fileData_;
    smk_t* smk_ = nullptr;
    unsigned numFrames_ = 0, curFrame_ = 0;
    std::chrono::microseconds frameDuration_{};
    Extent size_, displaySize_;
    std::vector<uint8_t> frame_;
};
