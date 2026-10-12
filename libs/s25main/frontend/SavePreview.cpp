// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "SavePreview.h"
#include "Savegame.h"
#include "s25util/BinaryFile.h"
#include <array>
#include <exception>
#include <limits>

namespace frontend {
namespace {
    constexpr std::array<char, 8> signature = {{'S', '2', '5', 'P', 'R', 'V', '0', '1'}};
    constexpr unsigned pixelBytes = savePreviewWidth * savePreviewHeight * 4;
    constexpr unsigned trailerBytes = signature.size() + 4 + pixelBytes + 4;

    uint32_t checksum(const libsiedler2::PixelBufferBGRA& pixels)
    {
        uint32_t hash = 2166136261u;
        for(unsigned i = 0; i < pixels.getSizeInBytes(); ++i)
            hash = (hash ^ pixels.getPixelPtr()[i]) * 16777619u;
        return hash;
    }
} // namespace

bool AppendSavePreview(const boost::filesystem::path& path, const libsiedler2::PixelBufferBGRA& pixels)
{
    if(pixels.getWidth() != savePreviewWidth || pixels.getHeight() != savePreviewHeight)
        return false;
    try
    {
        BinaryFile file;
        if(!file.Open(path, OpenFileMode::WriteAppend))
            return false;
        file.WriteRawData(signature.data(), signature.size());
        file.WriteUnsignedShort(savePreviewWidth);
        file.WriteUnsignedShort(savePreviewHeight);
        file.WriteRawData(pixels.getPixelPtr(), pixelBytes);
        file.WriteUnsignedInt(checksum(pixels));
        file.Flush();
        return true;
    } catch(const std::exception&)
    {
        // A partial optional trailer must never turn a successfully written world into a failed save.
        return false;
    }
}

std::optional<libsiedler2::PixelBufferBGRA> ReadSavePreview(const boost::filesystem::path& path)
{
    try
    {
        BinaryFile file;
        Savegame save;
        if(!file.Open(path, OpenFileMode::Read) || !save.Load(file, SaveGameDataToLoad::HeaderAndSettings))
            return std::nullopt;
        const unsigned flagOrSize = file.ReadUnsignedInt();
        unsigned worldBytes = flagOrSize;
        if(flagOrSize == 1) // Current compressed saves; legacy uncompressed saves store their length here.
        {
            const unsigned uncompressedBytes = file.ReadUnsignedInt();
            worldBytes = file.ReadUnsignedInt();
            if(uncompressedBytes <= 1 || worldBytes == 0)
                return std::nullopt;
        } else if(flagOrSize <= 1)
        {
            return std::nullopt;
        }
        const auto worldStart = file.Tell();
        file.Seek(0, SEEK_END);
        const auto size = file.Tell();
        if(size < worldStart || size - worldStart < worldBytes || size - worldStart - worldBytes != trailerBytes
           || size > static_cast<unsigned long>(std::numeric_limits<long>::max()))
            return std::nullopt;
        file.Seek(static_cast<long>(worldStart + worldBytes), SEEK_SET);
        std::array<char, signature.size()> actual{};
        file.ReadRawData(actual.data(), actual.size());
        if(actual != signature)
            return std::nullopt;
        const auto width = file.ReadUnsignedShort();
        const auto height = file.ReadUnsignedShort();
        if(width != savePreviewWidth || height != savePreviewHeight)
            return std::nullopt;
        libsiedler2::PixelBufferBGRA pixels(savePreviewWidth, savePreviewHeight);
        file.ReadRawData(pixels.getPixelPtr(), pixelBytes);
        if(file.ReadUnsignedInt() != checksum(pixels))
            return std::nullopt;
        return pixels;
    } catch(const std::exception&)
    {
        return std::nullopt;
    }
}

} // namespace frontend
