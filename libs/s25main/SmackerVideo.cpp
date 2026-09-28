// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "SmackerVideo.h"
#include "RttrConfig.h"
#include "libsiedler2/WAV_Header.h"
#include "s25util/Log.h"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include <algorithm>
#include <cstring>
#include <iterator>
#include <smacker.h>

namespace bfs = boost::filesystem;

namespace {
/// Finds the entry of dir whose name equals name ignoring case
bfs::path findCaseInsensitive(const bfs::path& dir, const std::string& name)
{
    boost::system::error_code ec;
    if(!bfs::is_directory(dir, ec))
        return {};
    // Always list the folder: on a case-insensitive file system dir / name exists but is not the real name
    for(bfs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    {
        if(boost::iequals(it->path().filename().string(), name))
            return it->path();
    }
    return {};
}

template<typename T>
void appendLE(std::vector<uint8_t>& out, T value)
{
    for(unsigned i = 0; i < sizeof(T); i++)
        out.push_back(static_cast<uint8_t>(value >> (8 * i)));
}
} // namespace

SmackerVideo::SmackerVideo() = default;

SmackerVideo::~SmackerVideo()
{
    if(smk_)
        smk_close(smk_);
}

bfs::path SmackerVideo::findOriginal(const std::string& fileName)
{
    const bfs::path videoDir = findCaseInsensitive(RTTRCONFIG.ExpandPath("<RTTR_GAME>"), "VIDEO");
    if(videoDir.empty())
        return {};
    return findCaseInsensitive(videoDir, fileName);
}

bool SmackerVideo::open(const bfs::path& file)
{
    if(smk_)
    {
        smk_close(smk_);
        smk_ = nullptr;
    }
    boost::nowide::ifstream in(file, std::ios::binary);
    if(!in)
    {
        LOG.write("Video %1% could not be opened\n") % file;
        return false;
    }
    fileData_.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    // Check the signature before libsmacker parses anything: it is not made for arbitrary input
    const bool isSmacker = fileData_.size() > 104 && std::memcmp(fileData_.data(), "SMK", 3) == 0
                           && (fileData_[3] == '2' || fileData_[3] == '4');
    if(!isSmacker)
    {
        LOG.write("Video %1% is not a Smacker video\n") % file;
        fileData_.clear();
        return false;
    }
    smk_ = smk_open_memory(fileData_.data(), static_cast<unsigned long>(fileData_.size()));
    if(!smk_)
    {
        LOG.write("Video %1% is not a Smacker video\n") % file;
        return false;
    }
    unsigned long frame, frameCount, width, height;
    double usPerFrame;
    unsigned char yScale;
    smk_info_all(smk_, &frame, &frameCount, &usPerFrame);
    smk_info_video(smk_, &width, &height, &yScale);
    numFrames_ = static_cast<unsigned>(frameCount);
    // 0 would mean "as fast as possible"; nobody wants that for an intro, assume the usual 15 fps
    frameDuration_ = std::chrono::microseconds(usPerFrame > 0 ? static_cast<long long>(usPerFrame) : 66667);
    size_ = Extent(static_cast<unsigned>(width), static_cast<unsigned>(height));
    displaySize_ = size_;
    if(yScale != SMK_FLAG_Y_NONE)
        displaySize_.y *= 2;
    if(numFrames_ == 0 || width == 0 || height == 0)
    {
        LOG.write("Video %1% has no frames\n") % file;
        smk_close(smk_);
        smk_ = nullptr;
        return false;
    }

    smk_enable_all(smk_, SMK_VIDEO_TRACK);
    if(smk_first(smk_) < 0)
    {
        smk_close(smk_);
        smk_ = nullptr;
        return false;
    }
    curFrame_ = 0;
    convertFrame();
    return true;
}

bool SmackerVideo::nextFrame()
{
    if(!smk_ || curFrame_ + 1 >= numFrames_)
        return false;
    if(smk_next(smk_) < 0)
        return false;
    curFrame_++;
    convertFrame();
    return true;
}

void SmackerVideo::convertFrame()
{
    const unsigned char* palette = smk_get_palette(smk_); // 256 * RGB
    const unsigned char* pixels = smk_get_video(smk_);    // palette indices
    const size_t numPixels = static_cast<size_t>(size_.x) * size_.y;
    frame_.resize(numPixels * 4);
    if(!palette || !pixels)
    {
        std::fill(frame_.begin(), frame_.end(), 0);
        return;
    }
    for(size_t i = 0; i < numPixels; i++)
    {
        const unsigned char* rgb = &palette[pixels[i] * 3];
        frame_[i * 4 + 0] = rgb[2];
        frame_[i * 4 + 1] = rgb[1];
        frame_[i * 4 + 2] = rgb[0];
        frame_[i * 4 + 3] = 0xFF;
    }
}

std::vector<uint8_t> SmackerVideo::decodeAudioAsWav() const
{
    if(fileData_.empty())
        return {};
    // Own handle: the playback handle has to stay on its current frame
    smk audio = smk_open_memory(fileData_.data(), static_cast<unsigned long>(fileData_.size()));
    if(!audio)
        return {};
    unsigned char trackMask, channels[7], bitDepth[7];
    unsigned long rate[7];
    smk_info_audio(audio, &trackMask, channels, bitDepth, rate);
    std::vector<uint8_t> pcm;
    if((trackMask & SMK_AUDIO_TRACK_0) && channels[0] > 0 && bitDepth[0] > 0 && rate[0] > 0)
    {
        smk_enable_all(audio, SMK_AUDIO_TRACK_0);
        if(smk_first(audio) >= 0)
        {
            do
            {
                const unsigned char* data = smk_get_audio(audio, 0);
                const unsigned long len = smk_get_audio_size(audio, 0);
                if(data && len)
                    pcm.insert(pcm.end(), data, data + len);
            } while(smk_next(audio) == SMK_MORE);
        }
    }
    const unsigned numChannels = channels[0], bits = bitDepth[0];
    const auto sampleRate = static_cast<uint32_t>(rate[0]);
    smk_close(audio);
    if(pcm.empty())
        return {};

    // Smacker delivers 8 bit unsigned or 16 bit signed little endian, exactly what WAV wants
    std::vector<uint8_t> wav;
    wav.reserve(sizeof(libsiedler2::WAV_Header) + pcm.size());
    const auto frameSize = static_cast<uint16_t>(numChannels * bits / 8);
    wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
    appendLE<uint32_t>(wav, static_cast<uint32_t>(36 + pcm.size()));
    wav.insert(wav.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    appendLE<uint32_t>(wav, 16);
    appendLE<uint16_t>(wav, 1); // PCM
    appendLE<uint16_t>(wav, static_cast<uint16_t>(numChannels));
    appendLE<uint32_t>(wav, sampleRate);
    appendLE<uint32_t>(wav, sampleRate * frameSize);
    appendLE<uint16_t>(wav, frameSize);
    appendLE<uint16_t>(wav, static_cast<uint16_t>(bits));
    wav.insert(wav.end(), {'d', 'a', 't', 'a'});
    appendLE<uint32_t>(wav, static_cast<uint32_t>(pcm.size()));
    wav.insert(wav.end(), pcm.begin(), pcm.end());
    return wav;
}
