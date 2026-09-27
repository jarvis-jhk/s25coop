-- Copyright (C) 2026 s25coop contributors
--
-- SPDX-License-Identifier: GPL-2.0-or-later

-- Scripted input: player 0 is a dummy AI that does nothing, so everything it builds comes from this script,
-- through the same command path as a network player. Place a woodcutter next to the HQ, connect it by road,
-- and check that it gets built.

function getRequiredLuaVersion()
    return 1
end

local site = nil
local connected = false

function onTestFrame(gf)
    local hqX, hqY = rttr:GetPlayer(0):GetHQPos()
    if site == nil then
        local x, y = test:FindBuildingSpot(0, BLD_WOODCUTTER, hqX + 4, hqY, 4)
        assert(x >= 0, "no spot for a woodcutter near the HQ")
        assert(test:SetBuildingSite(0, x, y, BLD_WOODCUTTER))
        site = { x = x, y = y }
        rttr:Log("scriptedInput: woodcutter site at " .. x .. "," .. y .. " (gf " .. gf .. ")")
    elseif not connected then
        -- One frame later: the site's flag exists only once the command above has run
        assert(rttr:GetPlayer(0):GetNumBuildingSites(BLD_WOODCUTTER) == 1, "the building site command did not run")
        local fx, fy = test:GetFlagPos(site.x, site.y)
        local hfx, hfy = test:GetFlagPos(hqX, hqY)
        connected = test:ConnectFlags(0, fx, fy, hfx, hfy)
        assert(connected, "no road from the site to the HQ")
    end
end

function onTestEnd(gf)
    assert(connected, "the site was never connected")
    local p = rttr:GetPlayer(0)
    assert(p:GetNumBuildings(BLD_WOODCUTTER) == 1,
        "woodcutter not built after " .. gf .. " frames (sites: " .. p:GetNumBuildingSites(BLD_WOODCUTTER) .. ")")
    assert(p:GetNumBuildings(BLD_WOODCUTTER) + p:GetNumBuildingSites(BLD_WOODCUTTER) == 1, "more than one woodcutter")
    rttr:Log("scriptedInput: woodcutter built by gf " .. gf)
end
