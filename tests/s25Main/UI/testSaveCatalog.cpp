// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "BasePlayerInfo.h"
#include "Savegame.h"
#include "frontend/SaveCatalog.h"
#include "libendian/ConvertEndianess.h"
#include "rttr/test/TmpFolder.hpp"
#include "s25util/BinaryFile.h"
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>

namespace {
void metadata(const boost::filesystem::path& path, s25util::time64_t savedAt, const std::string& map)
{
    Savegame save;
    BasePlayerInfo human;
    human.ps = PlayerState::Occupied;
    human.name = "Human";
    save.AddPlayer(human);
    BasePlayerInfo ai;
    ai.ps = PlayerState::AI;
    ai.name = "Computer";
    save.AddPlayer(ai);
    save.AddPlayer(BasePlayerInfo()); // An unused tribe must not be counted as a player.
    save.start_gf = 1234;
    {
        BinaryFile file;
        BOOST_TEST_REQUIRE(file.Open(path, OpenFileMode::Write));
        save.WriteAllHeaderData(file, map);
        save.WritePlayerData(file);
        save.WriteGGS(file);
    }
    // Signature + format + revision, followed by the save's own little-endian timestamp.
    boost::nowide::fstream file(path.string(), std::ios::binary | std::ios::in | std::ios::out);
    BOOST_TEST_REQUIRE(file.good());
    file.seekp(16);
    const auto time = libendian::ConvertEndianess<false>::fromNative(savedAt);
    file.write(reinterpret_cast<const char*>(&time), sizeof(time));
    BOOST_TEST_REQUIRE(file.good());
}
} // namespace

BOOST_AUTO_TEST_SUITE(SaveCatalogTests)

BOOST_AUTO_TEST_CASE(MetadataCountsOnlyUsedTribesWithoutLoadingAWorld)
{
    rttr::test::TmpFolder folder;
    metadata(folder / "save.sav", 100, "My map");
    const auto entry = frontend::ReadSaveEntry(folder / "save.sav");
    BOOST_TEST_REQUIRE(entry.has_value());
    BOOST_TEST(entry->map == "My map");
    BOOST_TEST(entry->savedAt == 100);
    BOOST_TEST(entry->gameFrame == 1234u);
    BOOST_TEST(entry->humans == 1u);
    BOOST_TEST(entry->ais == 1u);
    BOOST_TEST_REQUIRE(entry->players.size() == 2u);
    BOOST_TEST(entry->players[0] == "Human");
    BOOST_TEST(entry->players[1] == "Computer");
    // Deliberately no serialized world in this fixture: HeaderAndSettings is all browsing needs.
    Savegame full;
    BOOST_TEST(!full.Load(folder / "save.sav", SaveGameDataToLoad::All));
}

BOOST_AUTO_TEST_CASE(NewestTimestampWinsEvenWithinAMinuteAndTiesUsePaths)
{
    rttr::test::TmpFolder folder;
    metadata(folder / "a.sav", 101, "Older");
    metadata(folder / "z.SAV", 102, "Newest z");
    metadata(folder / "b.sav", 102, "Newest b");
    const auto catalog = frontend::ScanSaves(folder);
    BOOST_TEST_REQUIRE(catalog.entries.size() == 3u);
    BOOST_TEST(catalog.entries[0].path.filename().string() == "b.sav");
    BOOST_TEST(catalog.entries[1].path.filename().string() == "z.SAV");
    BOOST_TEST(catalog.entries[2].path.filename().string() == "a.sav");
    BOOST_TEST(catalog.rejected == 0u);
}

BOOST_AUTO_TEST_CASE(BrokenIncompatibleAndTruncatedFilesDoNotHideGoodSaves)
{
    rttr::test::TmpFolder folder;
    metadata(folder / "good.sav", 100, "Good");
    {
        boost::nowide::ofstream file((folder / "broken.sav").string(), std::ios::binary);
        file << "not a save";
    }
    {
        BinaryFile file;
        BOOST_TEST_REQUIRE(file.Open(folder / "incompatible.sav", OpenFileMode::Write));
        file.WriteRawData("RTTRSV", 6);
        file.WriteUnsignedChar(255);
        file.WriteUnsignedChar(0);
    }
    {
        Savegame save;
        BinaryFile file;
        BOOST_TEST_REQUIRE(file.Open(folder / "truncated.sav", OpenFileMode::Write));
        save.WriteAllHeaderData(file, "Header only");
    }
    {
        Savegame save;
        BinaryFile file;
        BOOST_TEST_REQUIRE(file.Open(folder / "too-many-players.sav", OpenFileMode::Write));
        save.WriteAllHeaderData(file, "Bad player count");
        Serializer players;
        players.PushUnsignedChar(255);
        players.WriteToFile(file);
    }
    metadata(folder / "ignored.rpl", 1000, "Not a save");
    boost::filesystem::create_directory(folder / "subdirectory.sav");
    const auto catalog = frontend::ScanSaves(folder);
    BOOST_TEST_REQUIRE(catalog.entries.size() == 1u);
    BOOST_TEST(catalog.entries[0].map == "Good");
    BOOST_TEST(catalog.rejected == 4u);
    BOOST_TEST(!frontend::ReadSaveEntry(folder / "missing.sav"));
}

BOOST_AUTO_TEST_CASE(MissingFolderIsAnEmptyCatalog)
{
    rttr::test::TmpFolder folder;
    const auto catalog = frontend::ScanSaves(folder / "not-created");
    BOOST_TEST(catalog.entries.empty());
    BOOST_TEST(catalog.rejected == 0u);
}

BOOST_AUTO_TEST_SUITE_END()
