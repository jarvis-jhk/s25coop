// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AsyncChecksum.h"
#include "BasePlayerInfo.h"
#include "IngameMinimap.h"
#include "Savegame.h"
#include "frontend/SavePreview.h"
#include "worldFixtures/CreateEmptyWorld.h"
#include "worldFixtures/WorldFixture.h"
#include "world/GameWorldViewer.h"
#include "rttr/test/TmpFolder.hpp"
#include "s25util/BinaryFile.h"
#include <boost/filesystem/operations.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <algorithm>

namespace {
libsiedler2::PixelBufferBGRA picture()
{
    libsiedler2::PixelBufferBGRA pixels(frontend::savePreviewWidth, frontend::savePreviewHeight);
    for(unsigned i = 0; i < pixels.getNumPixels(); ++i)
        pixels.set(i, libsiedler2::ColorBGRA(0xFF000000u | i));
    return pixels;
}

void saveFile(const boost::filesystem::path& path, const bool compressed = true)
{
    Savegame save;
    BasePlayerInfo human;
    human.ps = PlayerState::Occupied;
    save.AddPlayer(human);
    save.start_gf = 123;
    save.sgd.PushUnsignedInt(0x12345678);
    if(compressed)
    {
        BOOST_TEST_REQUIRE(save.Save(path, "Preview map"));
    } else
    {
        BinaryFile file;
        BOOST_TEST_REQUIRE(file.Open(path, OpenFileMode::Write));
        save.WriteAllHeaderData(file, "Preview map");
        save.WritePlayerData(file);
        save.WriteGGS(file);
        file.WriteUnsignedInt(save.sgd.GetLength());
        file.WriteRawData(save.sgd.GetData(), save.sgd.GetLength());
    }
}

void expectPixels(const libsiedler2::PixelBufferBGRA& actual, const libsiedler2::PixelBufferBGRA& expected)
{
    BOOST_TEST(actual.getWidth() == expected.getWidth());
    BOOST_TEST(actual.getHeight() == expected.getHeight());
    BOOST_TEST_REQUIRE(actual.getNumPixels() == expected.getNumPixels());
    for(unsigned i = 0; i < actual.getNumPixels(); ++i)
        BOOST_TEST(actual.get(i).asValue() == expected.get(i).asValue());
}
} // namespace

BOOST_AUTO_TEST_SUITE(SavePreviewTests)

BOOST_AUTO_TEST_CASE(OptionalTrailerPreservesCompressedAndLegacyWorldBytes)
{
    rttr::test::TmpFolder folder;
    const auto pixels = picture();
    for(const bool compressed : {false, true})
    {
        const auto path = folder / "world.sav";
        saveFile(path, compressed);
        BOOST_TEST(!frontend::ReadSavePreview(path));
        const auto originalSize = boost::filesystem::file_size(path);
        BOOST_TEST_REQUIRE(frontend::AppendSavePreview(path, pixels));
        BOOST_TEST(boost::filesystem::file_size(path) == originalSize + 64016u);
        const auto preview = frontend::ReadSavePreview(path);
        BOOST_TEST_REQUIRE(preview.has_value());
        expectPixels(*preview, pixels);
        Savegame loaded;
        BOOST_TEST_REQUIRE(loaded.Load(path, SaveGameDataToLoad::All));
        BOOST_TEST(loaded.start_gf == 123u);
        BOOST_TEST(loaded.sgd.PopUnsignedInt() == 0x12345678u);
        // A normal overwrite truncates the trailer, so another world's preview cannot survive it.
        saveFile(path, compressed);
        BOOST_TEST(!frontend::ReadSavePreview(path));
    }
}

BOOST_AUTO_TEST_CASE(DamagedUnknownAndTruncatedPreviewsAreOptional)
{
    rttr::test::TmpFolder folder;
    const auto path = folder / "damaged.sav";
    const auto pixels = picture();
    BOOST_TEST(!frontend::ReadSavePreview(folder / "missing.sav"));
    for(const unsigned offset : {0u, 8u, 10u, 12u, 64012u})
    {
        saveFile(path);
        const auto start = boost::filesystem::file_size(path);
        BOOST_TEST_REQUIRE(frontend::AppendSavePreview(path, pixels));
        {
            boost::nowide::fstream file(path.string(), std::ios::binary | std::ios::in | std::ios::out);
            BOOST_TEST_REQUIRE(file.good());
            const auto pos = static_cast<std::streamoff>(start + offset);
            file.seekg(pos);
            const auto byte = file.get();
            BOOST_TEST_REQUIRE(byte != std::char_traits<char>::eof());
            file.seekp(pos);
            file.put(static_cast<char>(static_cast<unsigned>(byte) ^ 0x7Fu));
        }
        BOOST_TEST(!frontend::ReadSavePreview(path));
        Savegame stillLoadable;
        BOOST_TEST(stillLoadable.Load(path, SaveGameDataToLoad::All));
    }
    for(const unsigned kept : {0u, 1u, 12u, 64015u})
    {
        saveFile(path);
        const auto start = boost::filesystem::file_size(path);
        BOOST_TEST_REQUIRE(frontend::AppendSavePreview(path, pixels));
        boost::filesystem::resize_file(path, start + kept);
        BOOST_TEST(!frontend::ReadSavePreview(path));
    }
    saveFile(path);
    BOOST_TEST_REQUIRE(frontend::AppendSavePreview(path, pixels));
    {
        BinaryFile file;
        BOOST_TEST_REQUIRE(file.Open(path, OpenFileMode::WriteAppend));
        file.WriteUnsignedChar(42); // Unknown extra data cannot silently become a valid v1 trailer.
    }
    BOOST_TEST(!frontend::ReadSavePreview(path));
    {
        boost::nowide::ofstream file(path.string(), std::ios::binary);
        file << "truncated header";
    }
    BOOST_TEST(!frontend::ReadSavePreview(path));
}

BOOST_AUTO_TEST_CASE(InvalidDimensionsAndUnwritableTargetsCannotAppend)
{
    rttr::test::TmpFolder folder;
    const auto path = folder / "world.sav";
    saveFile(path);
    const auto size = boost::filesystem::file_size(path);
    BOOST_TEST(!frontend::AppendSavePreview(path, libsiedler2::PixelBufferBGRA(1, 1)));
    BOOST_TEST(boost::filesystem::file_size(path) == size);
    BOOST_TEST(!frontend::AppendSavePreview(folder / "missing" / "world.sav", picture()));
#ifndef _WIN32
    BOOST_TEST(!frontend::AppendSavePreview("/dev/full", picture()));
#endif
}

BOOST_AUTO_TEST_CASE(InvalidWorldLengthsCannotMasqueradeAsAPreview)
{
    rttr::test::TmpFolder folder;
    const auto path = folder / "invalid.sav";
    const auto writeInvalid = [&](unsigned flag, unsigned rawSize, unsigned storedSize) {
        Savegame save;
        BinaryFile file;
        BOOST_TEST_REQUIRE(file.Open(path, OpenFileMode::Write));
        save.WriteAllHeaderData(file, "Invalid length");
        save.WritePlayerData(file);
        save.WriteGGS(file);
        file.WriteUnsignedInt(flag);
        if(flag == 1)
        {
            file.WriteUnsignedInt(rawSize);
            file.WriteUnsignedInt(storedSize);
        }
        file.Close();
        BOOST_TEST_REQUIRE(frontend::AppendSavePreview(path, picture()));
        BOOST_TEST(!frontend::ReadSavePreview(path));
    };
    writeInvalid(0, 0, 0);
    writeInvalid(1, 0, 4);
    writeInvalid(1, 4, 0);
    writeInvalid(1, 4, 0xFFFFFFFFu);
    writeInvalid(0xFFFFFFFFu, 0, 0);
    // Valid metadata followed by no world-length field reaches the reader's exception fallback.
    {
        Savegame save;
        BinaryFile file;
        BOOST_TEST_REQUIRE(file.Open(path, OpenFileMode::Write));
        save.WriteAllHeaderData(file, "Metadata only");
        save.WritePlayerData(file);
        save.WriteGGS(file);
    }
    BOOST_TEST(!frontend::ReadSavePreview(path));
}

using PreviewWorld = WorldFixture<CreateEmptyWorld, 1>;
BOOST_FIXTURE_TEST_CASE(CpuCaptureUsesVisibilityAndLeavesSimulationStateUntouched, PreviewWorld)
{
    const GameWorldViewer viewer(0, world);
    for(unsigned y = 0; y < world.GetHeight(); ++y)
        for(unsigned x = 0; x < world.GetWidth(); ++x)
            world.SetVisibility(MapPoint(x, y), 0, Visibility::Invisible);
    const auto beforeHidden = AsyncChecksum::create(*game);
    const auto hidden = IngameMinimap::CapturePreview(viewer, Extent(160, 100));
    BOOST_TEST((AsyncChecksum::create(*game) == beforeHidden));
    for(const auto pixel : hidden)
        BOOST_TEST(pixel.asValue() == 0xFF000000u);
    for(unsigned y = 0; y < world.GetHeight(); ++y)
        for(unsigned x = 0; x < world.GetWidth(); ++x)
            world.SetVisibility(MapPoint(x, y), 0, Visibility::Visible);
    const auto beforeVisible = AsyncChecksum::create(*game);
    const auto visible = IngameMinimap::CapturePreview(viewer, Extent(160, 100));
    BOOST_TEST(std::any_of(visible.getPixels().data(), visible.getPixels().data() + visible.getNumPixels(),
                           [](const auto color) { return color.asValue() != 0xFF000000u; }));
    BOOST_TEST((AsyncChecksum::create(*game) == beforeVisible));
    for(unsigned y = 0; y < world.GetHeight(); ++y)
        for(unsigned x = 0; x < world.GetWidth(); ++x)
            world.SetVisibility(MapPoint(x, y), 0, Visibility::FogOfWar);
    const auto remembered = IngameMinimap::CapturePreview(viewer, Extent(160, 100));
    auto& node = world.GetNodeWriteable(MapPoint(0, 0));
    node.owner = node.owner ? 0 : 1;
    const auto changedHiddenOwnership = IngameMinimap::CapturePreview(viewer, Extent(160, 100));
    expectPixels(changedHiddenOwnership, remembered);
    for(unsigned i = 0; i < visible.getNumPixels(); ++i)
    {
        const auto bright = visible.get(i);
        const auto dim = remembered.get(i);
        BOOST_TEST(dim.getRed() == bright.getRed() / 2);
        BOOST_TEST(dim.getGreen() == bright.getGreen() / 2);
        BOOST_TEST(dim.getBlue() == bright.getBlue() / 2);
    }
    BOOST_TEST(IngameMinimap::CapturePreview(viewer, Extent(0, 100)).size() == 0u);
    BOOST_TEST(IngameMinimap::CapturePreview(viewer, Extent(160, 0)).size() == 0u);
    BOOST_CHECK_THROW(IngameMinimap::CapturePreview(viewer, Extent(65536, 100)), std::invalid_argument);
}

BOOST_AUTO_TEST_SUITE_END()
