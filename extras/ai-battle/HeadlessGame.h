// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "AsyncChecksum.h"
#include "Game.h"
#include "ILocalGameState.h"
#include "Replay.h"
#include "TestInput.h"
#include "ai/AIPlayer.h"
#include "gameTypes/AIInfo.h"
#include <boost/filesystem.hpp>
#include <chrono>
#include <limits>
#include <vector>

class GameWorld;
class GlobalGameSettings;
class EventManager;

/// Run an ai-only game without user-interface.
class HeadlessGame
{
public:
    HeadlessGame(const GlobalGameSettings& ggs, const boost::filesystem::path& map, const std::vector<AI::Info>& ais,
                 const boost::filesystem::path& luaPath = {});
    ~HeadlessGame();

    void Run(unsigned maxGF = std::numeric_limits<unsigned>::max());
    void Close();

    /// s25coop test mode: show the script's rttr:Log output
    void ShowLuaOutput();
    /// s25coop test mode: run a second script in the same Lua state as the map's script (--lua), e.g. to wrap
    /// a campaign mission's event handlers with checks and to add onTestEnd
    void LoadTestScript(const boost::filesystem::path& path);
    /// s25coop test mode: give the scripts the global `test` (see TestInput) and call their onTestFrame(gf) at
    /// every network frame, before the commands of that frame are collected
    void EnableTestInput();
    /// s25coop test mode: call the script's onTestEnd(), which asserts on the final game state.
    /// Throws LuaExecutionError on a failed assertion, std::runtime_error if there is no such function.
    void CheckTestEnd();

    /// s25coop test mode: the state checksum as used for async detection, to compare two runs
    AsyncChecksum GetChecksum() const { return AsyncChecksum::create(game_); }

    void RecordReplay(const boost::filesystem::path& path, unsigned random_init);
    void SaveGame(const boost::filesystem::path& path) const;

private:
    void PrintState();
    void CallTestFrame();

    struct LocalState : ILocalGameState
    {
        // No local player: every player is an AI, and a campaign script's mission statement for player 0 would
        // otherwise open a window, which headless has no graphics for (crashed on MISS200)
        unsigned GetPlayerId() const override { return 0xFFFFFFFF; }
        bool IsHost() const override { return true; }
        std::string FormatGFTime(unsigned) const override { return ""; }
        void SystemChat(const std::string&) override {}
    };

    LocalState localState_;
    boost::filesystem::path map_;
    Game game_;
    GameWorld& world_;
    EventManager& em_;
    std::vector<std::unique_ptr<AIPlayer>> players_;

    Replay replay_;
    boost::filesystem::path replayPath_;
    boost::filesystem::path luaPath_;
    std::unique_ptr<TestInput> testInput_;

    unsigned lastReportGf_ = 0;
    std::chrono::steady_clock::time_point gameStartTime_;
};
