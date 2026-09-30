# Copyright (C) 2026 s25coop contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Play the tutorial to its real victory trigger, then disable just that trigger and require the same walkthrough
# to fail. A test that only constructs buildings or calls MissionEvent(99) itself must not count as a mission win.
set(args --test --objective none --map "${MAP}" --test-script "${WALKTHROUGH}" --ai dummy
         --random_init 1 --random_ai_init 1 --maxGF 60000)
execute_process(COMMAND "${AI_BATTLE}" ${args} --lua "${MISSION}"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE err TIMEOUT 120)
if(NOT result STREQUAL "0" OR NOT output MATCHES "Campaign progress: roman=2[\r\n]+TEST PASSED")
    message(FATAL_ERROR "Mission walkthrough did not win and record its chapter (${result}):\n${output}\n${err}")
endif()

file(READ "${MISSION}" missionScript)
if(NOT missionScript MATCHES "then MissionEvent\\(99\\)")
    message(FATAL_ERROR "The mission's arc victory trigger changed; update the negative walkthrough test")
endif()
string(REPLACE "then MissionEvent(99)" "then return -- victory deliberately disabled" withoutVictory "${missionScript}")
set(negativeMission "${WORK}/MISS200-no-victory.lua")
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${negativeMission}" "${withoutVictory}")
execute_process(COMMAND "${AI_BATTLE}" ${args} --lua "${negativeMission}"
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE err TIMEOUT 120)
if(NOT result STREQUAL "2" OR NOT err MATCHES "Lua error: .*mission event 99 .*never fired")
    message(FATAL_ERROR "Disabling arc victory did not fail the walkthrough's victory assertion (${result}):\n${output}\n${err}")
endif()
message(STATUS "The walkthrough occupied the arc, won and recorded roman=2; disabling the victory trigger was caught")
