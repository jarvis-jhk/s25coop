-- Copyright (C) 2026 s25coop contributors
--
-- SPDX-License-Identifier: GPL-2.0-or-later

-- Roman campaign, mission 1 (MISS200), played by this script: loaded with ai-battle --test-script next to the
-- mission's own script, with player 0 a dummy AI, so everything that happens comes from here, through the same
-- command path as a network player. It works through the mission's goals the way a player would, and asserts
-- that the mission's own triggers fire in order: building counts (onGameFrame), a conquered spot (onOccupied)
-- and a geologist's find (onResourceFound).

local P = 0
local missionOnResourceFound = onResourceFound
local ironAt = nil -- first iron a geologist found

function onResourceFound(p, x, y, rIdx, q, ...)
    if p == P and rIdx == RES_IRON and ironAt == nil then
        ironAt = { x = x, y = y }
    end
    if missionOnResourceFound then
        return missionOnResourceFound(p, x, y, rIdx, q, ...)
    end
end

-- Mission events that fired, from the mission's own history (eHist is the mission script's global)
local function fired(e)
    for i = 1, eHist["n"] do
        if eHist[i] == e then
            return true
        end
    end
    return false
end

local function firedList()
    local list = {}
    for i = 1, eHist["n"] do
        list[#list + 1] = tostring(eHist[i])
    end
    return table.concat(list, " ")
end

local function hqFlag()
    local x, y = rttr:GetPlayer(P):GetHQPos()
    return test:GetFlagPos(x, y)
end

-- A construction: place the site near a point; its flag is connected to the road network in a later frame
local pending = {}

local function build(bld, x, y, radius)
    local sx, sy = test:FindBuildingSpot(P, bld, x, y, radius or 6)
    assert(sx >= 0, "no spot for building " .. tostring(bld) .. " near " .. x .. "," .. y)
    assert(test:SetBuildingSite(P, sx, sy, bld))
    pending[#pending + 1] = { x = sx, y = sy }
    rttr:Log("walkthrough: site " .. tostring(bld) .. " at " .. sx .. "," .. sy)
end

-- One road per frame: two roads queued together are planned on the same map and can cross, and then the second
-- one is refused when it runs
local function connectPending()
    local site = table.remove(pending, 1)
    local fx, fy = site.flagX, site.flagY
    if fx == nil then
        fx, fy = test:GetFlagPos(site.x, site.y)
    end
    assert(test:ConnectToNetwork(P, fx, fy, 12) >= 0, "no road from " .. fx .. "," .. fy .. " to the road network")
end

local function count(bld)
    return rttr:GetPlayer(P):GetNumBuildings(bld)
end

-- The plan: each step runs once its condition holds, then the next one waits for its own
local hqX, hqY
local geologistFlags = {}
local steps = {
    { "basic production", function() return true end, function()
        build(BLD_WOODCUTTER, hqX - 4, hqY)
        build(BLD_QUARRY, hqX + 4, hqY)
        build(BLD_SAWMILL, hqX, hqY + 4)
    end },
    { "forester", function() return fired(2) end, function() build(BLD_FORESTER, hqX - 4, hqY + 3) end },
    -- (34,28) is the spot the mission wants occupied; a barracks to the north claims it
    { "barracks to the north", function() return fired(3) end, function() build(BLD_BARRACKS, 34, 31, 3) end },
    { "geologists to the mountains", function() return fired(4) end, function()
        -- A mine-capable spot is mountain terrain: plant flags there and send the four geologists the mission gave
        for _, near in ipairs({ { hqX, hqY - 8 }, { hqX + 6, hqY - 6 }, { hqX - 6, hqY - 6 }, { hqX + 8, hqY } }) do
            local mx, my = test:FindBuildingSpot(P, BLD_IRONMINE, near[1], near[2], 5)
            if mx >= 0 then
                local fx, fy = test:GetFlagPos(mx, my)
                test:SetFlag(P, fx, fy)
                geologistFlags[#geologistFlags + 1] = { x = fx, y = fy }
            end
        end
        assert(#geologistFlags > 0, "no mountain within reach for the geologists")
    end },
    { "roads to the geologist flags", function() return true end, function()
        for _, f in ipairs(geologistFlags) do
            pending[#pending + 1] = { flagX = f.x, flagY = f.y }
        end
    end },
    { "send geologists", function() return true end, function()
        for _, f in ipairs(geologistFlags) do
            test:CallSpecialist(P, f.x, f.y, JOB_GEOLOGIST)
        end
    end },
    { "iron industry", function() return fired(5) and ironAt ~= nil end, function()
        build(BLD_IRONMINE, ironAt.x, ironAt.y, 4)
        build(BLD_IRONSMELTER, hqX + 3, hqY + 4)
        build(BLD_ARMORY, hqX - 2, hqY + 6)
    end },
    -- The iron industry unlocks the barracks again; (39,19) is the next spot the mission wants
    { "barracks to the north-east", function() return fired(6) end, function() build(BLD_BARRACKS, 39, 22, 5) end },
}

local step = 1
local goals = { [2] = "forester unlocked", [3] = "barracks unlocked", [4] = "(34,28) occupied", [5] = "iron found",
                [6] = "iron industry built", [7] = "(39,19) occupied" }

function onTestFrame(gf)
    if hqX == nil then
        hqX, hqY = rttr:GetPlayer(P):GetHQPos()
    end
    if #pending > 0 then
        connectPending()
        return
    end
    local s = steps[step]
    if s and s[2]() then
        rttr:Log("walkthrough: gf " .. gf .. " -> " .. s[1])
        s[3]()
        step = step + 1
    end
end

function onTestEnd(gf)
    rttr:Log("walkthrough: events fired: " .. firedList() .. " (step " .. step .. " of " .. #steps .. ")")
    local p = rttr:GetPlayer(P)
    for _, b in ipairs({ BLD_IRONMINE, BLD_IRONSMELTER, BLD_ARMORY, BLD_BARRACKS }) do
        rttr:Log("walkthrough: building " .. tostring(b) .. ": " .. p:GetNumBuildings(b) .. " built, "
                 .. p:GetNumBuildingSites(b) .. " sites")
    end
    for e = 1, 7 do
        assert(fired(e), "mission event " .. e .. " (" .. (goals[e] or "start") .. ") never fired in " .. gf
                   .. " frames; fired: " .. firedList())
    end
    assert(ironAt ~= nil, "no iron reported")
    assert(count(BLD_BARRACKS) == 2, "expected two barracks, got " .. count(BLD_BARRACKS))
end
