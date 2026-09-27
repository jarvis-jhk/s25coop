-- Copyright (C) 2026 s25coop contributors
--
-- SPDX-License-Identifier: GPL-2.0-or-later

-- Campaign mission smoke test, loaded with ai-battle --test-script next to a mission's own script (--lua).
-- Wraps the mission's event handlers so the mission keeps working, and checks that its script started and ran
-- for the whole game without a Lua error (any error already ends the run with exit code 2).

local missionOnStart = onStart
local missionOnGameFrame = onGameFrame
local started = false
local frames = 0

function onStart(...)
    started = true
    if missionOnStart then
        return missionOnStart(...)
    end
end

function onGameFrame(gf, ...)
    frames = frames + 1
    if missionOnGameFrame then
        return missionOnGameFrame(gf, ...)
    end
end

function onTestEnd(gf)
    assert(started, "the start event never fired")
    assert(frames > 0 and frames >= gf - 1, "onGameFrame ran " .. frames .. " times in " .. gf .. " frames")
    assert(rttr:GetPlayer(0):GetNumBuildings(BLD_HEADQUARTERS) == 1, "player 0 lost its HQ")
    rttr:Log("mission smoke: " .. frames .. " frames, mission script " .. (missionOnGameFrame and "has" or "has no")
             .. " onGameFrame")
end
