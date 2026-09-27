-- Copyright (C) 2026 s25coop contributors
--
-- SPDX-License-Identifier: GPL-2.0-or-later

-- Proves the harness itself: a failed assertion in onTestEnd must fail the run (ctest WILL_FAIL).

function getRequiredLuaVersion()
    return 1
end

function onTestEnd(gf)
    assert(rttr:GetPlayer(0):GetNumBuildings(BLD_HEADQUARTERS) == 42, "expected failure")
end
