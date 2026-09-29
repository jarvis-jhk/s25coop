# Copyright (C) 2026 s25coop contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Save and resume a coop campaign (ROADMAP M2 step 6), a host and one co-player, original S2 data (local only):
#   1. the host hosts the mission (MAP + LUA), the co-player joins the host's player and orders a woodcutter; the host
#      saves the game at MAX_GF.
#   2. the host continues that savegame, the same co-player joins the host's player again and orders a second
#      woodcutter; at 2 * MAX_GF both run TEST_SCRIPT (finishing the mission).
# Both woodcutters must stand in both worlds, both processes must stay in sync, and both must record the progress.
# Inputs: COOP_NET, MAP, LUA, GAME_DIR, PORT, MAX_GF, WORK, TEST_SCRIPT
file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK}/host ${WORK}/join)
math(EXPR buildAt2 "${MAX_GF} + 200")
math(EXPR maxGF2 "${MAX_GF} * 2")
math(EXPR port2 "${PORT} + 1")
set(env ${CMAKE_COMMAND} -E env USER=coop RTTR_GAME_DIR=${GAME_DIR})

function(run phase port maxGF hostArgs joinArgs)
    execute_process(
        COMMAND ${env} HOME=${WORK}/host ${COOP_NET} host --port ${port} --maxGF ${maxGF} --timeout 120 --players 1
                --members 1 --members-via-lobby ${hostArgs} --out ${WORK}/host${phase}.txt --log ${WORK}/host${phase}.log
        COMMAND ${env} HOME=${WORK}/join ${COOP_NET} join --port ${port} --maxGF ${maxGF} --timeout 120 --member-of-host
                ${joinArgs} --after ${WORK}/host${phase}.txt.connected --wait-for ${WORK}/host${phase}.txt
                --out ${WORK}/join${phase}.txt --log ${WORK}/join${phase}.log
        RESULTS_VARIABLE results
        ERROR_VARIABLE errors)
    message(STATUS "Phase ${phase} exit codes (host join): ${results}\n${errors}")
    foreach(result IN LISTS results)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "Phase ${phase}: expected every process to exit with 0")
        endif()
    endforeach()
endfunction()

run(1 ${PORT} ${MAX_GF} "--map;${MAP};--lua;${LUA};--save;${WORK}/coop.sav" "--build-at;200")
if(NOT EXISTS ${WORK}/coop.sav)
    message(FATAL_ERROR "The host saved nothing")
endif()
run(2 ${port2} ${maxGF2} "--savegame;${WORK}/coop.sav;--test-script;${TEST_SCRIPT}"
    "--build-at;${buildAt2};--test-script;${TEST_SCRIPT}")

file(READ ${WORK}/host2.txt host)
file(READ ${WORK}/join2.txt join)
message(STATUS "host: ${host}\njoin: ${join}")
foreach(role host join)
    # The woodcutter from before the save and the one ordered after resuming
    if(NOT ${role} MATCHES "State at GF ${maxGF2}: [^\n]* woodcutters 2 0 0 0 0 0 0")
        message(FATAL_ERROR "${role}: expected both woodcutters of the co-player at GF ${maxGF2}")
    endif()
    if(NOT ${role} MATCHES "Campaign progress: roman=2")
        message(FATAL_ERROR "${role}: the resumed mission did not record its chapter")
    endif()
endforeach()
if(NOT host MATCHES "Members at start: Client@0")
    message(FATAL_ERROR "The co-player did not join the resumed game as the host's co-player")
endif()
string(REGEX MATCH "State at GF ${maxGF2}: [^\n]*" hostState "${host}")
string(REGEX MATCH "State at GF ${maxGF2}: [^\n]*" joinState "${join}")
if(NOT hostState STREQUAL joinState)
    message(FATAL_ERROR "The co-player is out of sync with the host:\n${hostState}\n${joinState}")
endif()
