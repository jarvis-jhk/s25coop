-- Copyright (C) 2026 s25coop contributors
--
-- SPDX-License-Identifier: GPL-2.0-or-later

-- Headless smoke test: the game starts, Lua events fire and the AI builds up its economy.
-- Run by ai-battle --test; onTestEnd(gf) asserts on the final state.

started = false
frames = 0

function getRequiredLuaVersion()
    return 1
end

function onStart(isFirstStart)
    assert(isFirstStart, "a new game must start as first start")
    started = true
    for i = 0, rttr:GetPlayerCount() - 1 do
        assert(rttr:GetPlayer(i):GetNumBuildings(BLD_HEADQUARTERS) == 1, "player " .. i .. " has no HQ at start")
    end
end

function onGameFrame(gf)
    frames = frames + 1
end

function onTestEnd(gf)
    assert(started, "onStart never ran")
    assert(frames > 0 and frames >= gf - 1, "onGameFrame ran " .. frames .. " times in " .. gf .. " frames")
    assert(rttr:GetPlayerCount() >= 2, "expected at least 2 players")
    for i = 0, rttr:GetPlayerCount() - 1 do
        local p = rttr:GetPlayer(i)
        assert(p:GetNumBuildings(BLD_HEADQUARTERS) == 1, "player " .. i .. " lost its HQ")
        assert(not p:IsDefeated(), "player " .. i .. " is defeated")
        local built = p:GetNumBuildings(BLD_WOODCUTTER) + p:GetNumBuildingSites(BLD_WOODCUTTER)
                    + p:GetNumBuildings(BLD_SAWMILL) + p:GetNumBuildingSites(BLD_SAWMILL)
        assert(built > 0, "AI player " .. i .. " built no wood economy")
    end
    rttr:Log("smoke: " .. frames .. " frames, " .. rttr:GetPlayerCount() .. " players")
end
