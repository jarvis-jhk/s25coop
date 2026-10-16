// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

// Writes a savegame with upstream Return to the Roots' own code, as a fixture for s25coop's compatibility tests.
// The workflow copies this file into an unmodified upstream checkout, so every byte of the save comes from
// upstream's Savegame/SerializedGameData. The world is upstream's own recorded game (tests/testData/200kGFs.rpl:
// one human tribe and seven hard AIs on "Big Slaughter v2"), replayed with checksum verification up to SAVE_GF.

#define BOOST_TEST_MODULE RTTR_UpstreamSaveFixture
#include "EventManager.h"
#include "Game.h"
#include "GamePlayer.h"
#include "PlayerInfo.h"
#include "Replay.h"
#include "Savegame.h"
#include "network/PlayerGameCommands.h"
#include "ogl/glAllocator.h"
#include "random/Random.h"
#include "variant.h"
#include "world/GameWorld.h"
#include "gameTypes/MapInfo.h"
#include "test/testConfig.h"
#include "libsiedler2/libsiedler2.h"
#include <rttr/test/Fixture.hpp>
#include <boost/test/unit_test.hpp>
#include <cstdlib>
#include <iostream>

namespace {
struct Fixture : rttr::test::Fixture
{
    Fixture() { libsiedler2::setAllocator(new GlAllocator); }
};
struct MockGameState : ILocalGameState
{
    unsigned GetPlayerId() const override { return 0; }
    bool IsHost() const override { return true; }
    std::string FormatGFTime(unsigned) const override { return ""; }
    void SystemChat(const std::string&) override {}
};
} // namespace
BOOST_GLOBAL_FIXTURE(Fixture);

BOOST_AUTO_TEST_CASE(WriteUpstreamSave)
{
    const char* out = std::getenv("UPSTREAM_SAVE_OUT");
    BOOST_TEST_REQUIRE(out);
    const char* gfText = std::getenv("UPSTREAM_SAVE_GF");
    const unsigned saveGF = gfText ? static_cast<unsigned>(std::strtoul(gfText, nullptr, 10)) : 3000u;

    Replay replay;
    BOOST_TEST_REQUIRE(replay.LoadHeader(rttr::test::rttrBaseDir / "tests" / "testData" / "200kGFs.rpl"));
    MapInfo mapInfo;
    BOOST_TEST_REQUIRE(replay.LoadGameData(mapInfo));
    BOOST_TEST_REQUIRE(static_cast<bool>(mapInfo.savegame));
    std::vector<PlayerInfo> players;
    for(unsigned i = 0; i < replay.GetNumPlayers(); i++)
        players.emplace_back(replay.GetPlayer(i));
    Game game(replay.ggs, /*startGF*/ 0, players);
    RANDOM.Init(replay.getSeed());
    MockGameState gs;
    mapInfo.savegame->sgd.ReadSnapshot(game, gs);
    game.world_.SetReplayCompatVersion(replay.GetMinorVersion());
    game.world_.InitAfterLoad();

    auto nextGF = replay.ReadGF();
    BOOST_TEST_REQUIRE(nextGF.has_value());
    while(game.em_->GetCurrentGF() < saveGF)
    {
        const unsigned curGF = game.em_->GetCurrentGF();
        AsyncChecksum checksum;
        if(nextGF && *nextGF == curGF)
            checksum = AsyncChecksum::create(game);
        while(nextGF && *nextGF == curGF)
        {
            const auto cmd = replay.ReadCommand();
            visit(composeVisitor([](const Replay::ChatCommand&) {},
                                 [&](const Replay::GameCommand& gcmd) {
                                     for(const gc::GameCommandPtr& gc : gcmd.cmds.gcs)
                                         gc->Execute(game.world_, gcmd.player);
                                     BOOST_TEST_REQUIRE(
                                       (gcmd.cmds.checksum.randChecksum == 0 || checksum == gcmd.cmds.checksum));
                                 }),
                  cmd);
            nextGF = replay.ReadGF();
        }
        game.RunGF();
    }

    // As GameClient::SaveToFile/WritePlayerInfo do it
    Savegame save;
    for(unsigned i = 0; i < replay.GetNumPlayers(); i++)
        save.AddPlayer(replay.GetPlayer(i));
    save.ggs = game.ggs_;
    save.start_gf = game.em_->GetCurrentGF();
    save.sgd.MakeSnapshot(game);
    BOOST_TEST_REQUIRE(save.Save(out, replay.GetMapName()));

    Savegame reread;
    BOOST_TEST_REQUIRE(reread.Load(out, SaveGameDataToLoad::All));
    std::cout << "Saved " << out << " version " << int(reread.GetMajorVersion()) << '.' << int(reread.GetMinorVersion())
              << " map '" << reread.GetMapName() << "' GF " << reread.start_gf << " game data version "
              << reread.sgd.GetGameDataVersion() << '\n';
    for(unsigned i = 0; i < reread.GetNumPlayers(); i++)
    {
        const auto& p = reread.GetPlayer(i);
        std::cout << "Player " << i << ": state " << int(p.ps) << " AI " << int(p.aiInfo.type) << " level "
                  << int(p.aiInfo.level) << " nation " << int(p.nation) << " team " << int(p.team) << " name '"
                  << p.name << "'\n";
    }
}
