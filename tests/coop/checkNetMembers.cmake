# Copyright (C) 2026 s25coop contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Coop members (ROADMAP M2 steps 1+2): a host, optionally a second player, and a member that controls the host's
# player 0 together with it and orders a woodcutter. Every process prints its state at MAX_GF; all must be equal and
# player 0 must own the member's woodcutter. The server does not compare a member's checksum yet (step 5), so this
# comparison is what proves the member stayed in sync.
# Inputs: COOP_NET, MAP, PORT, MAX_GF, WORK, SECOND_PLAYER (ON: a normal client joins as player 1, after the member,
# so the member first gets that free slot and gives it back), MEMBER_ARGS (| separated),
# EXPECT (sync, or differ = the member's state must differ, to prove the comparison works)
string(REPLACE "|" ";" MEMBER_ARGS "${MEMBER_ARGS}")
file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK}/host ${WORK}/member ${WORK}/join)
set(common --port ${PORT} --maxGF ${MAX_GF} --timeout 120 --trace 20)
set(roles host member)
if(SECOND_PLAYER)
    set(players 2)
    list(APPEND roles join)
    set(second COMMAND ${CMAKE_COMMAND} -E env HOME=${WORK}/join USER=coop ${COOP_NET} join ${common} --connect-delay 3
                --after ${WORK}/host.txt.connected
                --out ${WORK}/join.txt --wait-for ${WORK}/host.txt --log ${WORK}/join.log)
else()
    set(players 1)
    set(second)
endif()
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env HOME=${WORK}/host USER=coop ${COOP_NET} host ${common} --map ${MAP} --ai aijh
            --players ${players} --members 1 --out ${WORK}/host.txt --log ${WORK}/host.log
    COMMAND ${CMAKE_COMMAND} -E env HOME=${WORK}/member USER=coop ${COOP_NET} join ${common} --member-of 0 --build-at 200
            --after ${WORK}/host.txt.connected
            --out ${WORK}/member.txt --wait-for ${WORK}/host.txt --log ${WORK}/member.log ${MEMBER_ARGS}
    ${second}
    RESULTS_VARIABLE results
    ERROR_VARIABLE errors)
message(STATUS "Exit codes (${roles}): ${results}\n${errors}")
foreach(role IN LISTS roles)
    if(EXISTS ${WORK}/${role}.log)
        file(READ ${WORK}/${role}.log log)
        string(REGEX MATCHALL "[^\n]*(Async|error|Error|member|Member|Ordered)[^\n]*" important "${log}")
        message(STATUS "${role}: ${important}")
    endif()
endforeach()
foreach(result IN LISTS results)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Expected every process to exit with 0")
    endif()
endforeach()
foreach(role IN LISTS roles)
    file(READ ${WORK}/${role}.txt result)
    if(NOT result MATCHES "State at GF ${MAX_GF}: ([^\n]*)")
        message(FATAL_ERROR "${role} printed no state at GF ${MAX_GF}: ${result}")
    endif()
    set(state_${role} "${CMAKE_MATCH_1}")
    message(STATUS "${role}: ${state_${role}}")
endforeach()
if(NOT state_host MATCHES "woodcutters [1-9]")
    message(FATAL_ERROR "The member's woodcutter is not in the host's world")
endif()
if(EXPECT STREQUAL "differ")
    if(state_member STREQUAL state_host)
        message(FATAL_ERROR "The member diverged on purpose but its state equals the host's")
    endif()
else()
    foreach(role IN LISTS roles)
        if(NOT state_${role} STREQUAL state_host)
            message(FATAL_ERROR "${role} is out of sync with the host")
        endif()
    endforeach()
endif()
