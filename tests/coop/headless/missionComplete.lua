-- Copyright (C) 2026 s25coop contributors
--
-- SPDX-License-Identifier: GPL-2.0-or-later

-- Campaign progress test, loaded with ai-battle --test-script next to a mission's own script (--lua).
-- At the end it lets the mission's script finish the mission the way a won game would: a Roman mission's final
-- event (99, reaching the arc) or a world mission's onHumanWinner. The harness then prints the campaign progress
-- the script recorded, and tests/coop/CMakeLists.txt checks that it is this mission's own chapter.

function onTestEnd(gf)
    if MissionEvent then
        assert(eState, "mission script has no event state")
        eState[99] = 1
        MissionEvent(99, false)
    else
        assert(onHumanWinner, "mission script has neither MissionEvent nor onHumanWinner")
        onHumanWinner()
    end
end
