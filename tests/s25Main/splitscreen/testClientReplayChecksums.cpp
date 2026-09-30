// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AsyncChecksum.h"
#include "GamePlayer.h"
#include "LocalGameFixture.h"
#include "Replay.h"
#include "factories/GameCommandFactory.h"
#include "variant.h"
#include "gameTypes/MapInfo.h"
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <optional>

namespace {
// Re-encode through Replay so a checksum mismatch remains a valid replay, including compressed recordings.
std::optional<unsigned> copyRecording(const boost::filesystem::path& sourcePath,
                                      const boost::filesystem::path& targetPath, const bool corruptChecksum)
{
    Replay source;
    BOOST_TEST_REQUIRE(source.LoadHeader(sourcePath));
    MapInfo map;
    BOOST_TEST_REQUIRE(source.LoadGameData(map));
    map.title = source.GetMapName();
    Replay target;
    target.ggs = source.ggs;
    for(unsigned player = 0; player < source.GetNumPlayers(); ++player)
        target.AddPlayer(source.GetPlayer(player));
    BOOST_TEST_REQUIRE(target.StartRecording(targetPath, map, source.getSeed()));

    std::optional<unsigned> changedGF;
    unsigned numGameCommands = 0;
    for(auto gf = source.ReadGF(); gf; gf = source.ReadGF())
    {
        auto command = source.ReadCommand();
        visit(composeVisitor(
                [&](const Replay::ChatCommand& chat) { target.AddChatCommand(*gf, chat.player, chat.dest, chat.msg); },
                [&](Replay::GameCommand& game) {
                    numGameCommands += game.cmds.gcs.size();
                    // Zero is the legacy marker for "no checksum"; retain the valid marker and
                    // corrupt exactly one object-count field instead.
                    if(corruptChecksum && !changedGF && game.cmds.checksum.randChecksum != 0)
                    {
                        game.cmds.checksum.objCt ^= 1u;
                        changedGF = *gf;
                    }
                    target.AddGameCommand(*gf, game.player, game.cmds);
                }),
              command);
    }
    BOOST_TEST_REQUIRE(numGameCommands > 0u);
    BOOST_TEST_REQUIRE((!corruptChecksum || changedGF.has_value()));
    target.UpdateLastGF(source.GetLastGF());
    BOOST_TEST_REQUIRE(target.StopRecording());
    return changedGF;
}

// Initialize the mock GUI driver before LocalGameFixture constructs the settings-dependent GameManager.
struct ReplayChecksumFixture : uiHelper::Fixture, rttr::test::LocalGameFixture
{
    void beginReplay(const boost::filesystem::path& path)
    {
        GAMECLIENT.Stop();
        BOOST_TEST_REQUIRE(!GAMECLIENT.IsReplayModeOn());
        BOOST_TEST(!GAMECLIENT.IsReplayFOWDisabled());
        ci().numErrors = 0;
        ci().numReplayAsync = 0;
        ci().replayEnded = false;
        GAMECLIENT.SetInterface(&ci());
        BOOST_TEST_REQUIRE(GAMECLIENT.StartReplay(path));
        GAMECLIENT.GameLoaded();
        GAMECLIENT.SetPause(false);
        GAMECLIENT.skiptogf = GAMECLIENT.GetLastReplayGF();
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(ClientReplayChecksumTests)

BOOST_FIXTURE_TEST_CASE(ACorruptedChecksumPausesTheRealClientButAnUnchangedCopyReplaysInSync, ReplayChecksumFixture)
{
    hostAndEnterLobby();
    lobby().SetPlayerState(1, PlayerState::AI, AI::Info(AI::Type::Dummy));
    lobby().SetPlayerState(2, PlayerState::AI, AI::Info(AI::Type::Dummy));
    pumpUntil(
      [] {
          const auto lobby = GAMECLIENT.GetGameLobby();
          return lobby->getPlayer(1).ps == PlayerState::AI && lobby->getPlayer(2).ps == PlayerState::AI;
      },
      "dummy AI slots to be configured");
    startGame();
    const MilitarySettings military{{1, 2, 3, 4, 5, 6, 7, 8}};
    BOOST_TEST_REQUIRE(GAMECLIENT.GetGCFactory(0)->ChangeMilitary(military));
    pumpUntilGF(GAMECLIENT.GetGFNumber() + 40u);
    const auto expectedFinalGF = GAMECLIENT.GetGFNumber();
    const auto expectedChecksum = AsyncChecksum::create(*ci().game);
    const auto recording = RTTRCONFIG.ExpandPath(s25::folders::replays) / GAMECLIENT.GetReplayFilename();
    GAMECLIENT.Stop();
    BOOST_TEST_REQUIRE(boost::filesystem::is_regular_file(recording));

    const auto cleanCopy = recording.parent_path() / "checksum-control.rpl";
    const auto brokenCopy = recording.parent_path() / "checksum-corrupt.rpl";
    BOOST_TEST(!copyRecording(recording, cleanCopy, false));
    const auto changedGF = copyRecording(recording, brokenCopy, true);
    BOOST_TEST_REQUIRE(changedGF.has_value());

    beginReplay(cleanCopy);
    pumpUntil([this] { return ci().replayEnded || ci().numReplayAsync > 0 || ci().numErrors > 0; },
              "unchanged replay to finish");
    BOOST_TEST_REQUIRE(ci().replayEnded);
    BOOST_TEST(ci().numReplayAsync == 0u);
    BOOST_TEST(ci().numErrors == 0u);
    BOOST_TEST(GAMECLIENT.GetGFNumber() == expectedFinalGF);
    BOOST_TEST((AsyncChecksum::create(*ci().game) == expectedChecksum));
    for(unsigned i = 0; i < military.size(); ++i)
        BOOST_TEST(world().GetPlayer(0).GetMilitarySetting(i) == military[i]);

    beginReplay(brokenCopy);
    pumpUntil([this] { return ci().numReplayAsync > 0 || ci().replayEnded || ci().numErrors > 0; },
              "corrupted checksum to be reported");
    BOOST_TEST(ci().numReplayAsync == 1u);
    BOOST_TEST(ci().numErrors == 0u);
    BOOST_TEST(!ci().replayEnded);
    BOOST_TEST_REQUIRE(GAMECLIENT.IsPaused());
    BOOST_TEST(GAMECLIENT.skiptogf == 0u);
    BOOST_TEST(GAMECLIENT.GetGFNumber() == *changedGF + 1u);
    const auto pausedGF = GAMECLIENT.GetGFNumber();
    const auto frameLength = GAMECLIENT.GetGFLength();
    BOOST_TEST_REQUIRE(frameLength.count() > 0);
    const auto deadline = std::chrono::steady_clock::now() + frameLength * 3;
    do
    {
        pump();
    } while(std::chrono::steady_clock::now() < deadline);
    BOOST_TEST(GAMECLIENT.GetGFNumber() == pausedGF);
    BOOST_TEST(ci().numReplayAsync == 1u);
    BOOST_TEST(!ci().replayEnded);
    GAMECLIENT.Stop();
    BOOST_TEST_REQUIRE(!GAMECLIENT.IsReplayModeOn());
    BOOST_TEST(!GAMECLIENT.IsReplayFOWDisabled());
}

BOOST_AUTO_TEST_SUITE_END()
