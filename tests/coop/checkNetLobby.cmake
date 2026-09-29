# Copyright (C) 2026 s25coop contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Coop members in the lobby (ROADMAP M2 step 3), a host and one joining client:
#   switch  - the host allows members through the lobby message; the client takes the open slot 1, then becomes a
#             member of player 0 from the lobby and orders a woodcutter for it. Both must end in the same state.
#   refused - members are not allowed; the client asks to switch anyway, is refused and plays on as player 1.
#   kick    - the client joins as a member; the host kicks it in the lobby and plays alone.
#   swap    - the client joins as a member; the host then swaps its slot 0 with the dummy AI in slot 1. The member must
#             follow it to slot 1 (server and client), so its woodcutter is built for player 1, not for the AI.
# Inputs: COOP_NET, MAP, PORT, MAX_GF, WORK, MODE
file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK}/host ${WORK}/join)
set(common --port ${PORT} --maxGF ${MAX_GF} --timeout 120)
set(ai aijh)
if(MODE STREQUAL "switch")
    set(hostArgs --players 1 --open-slots 1 --members 1 --members-via-lobby)
    set(joinArgs --switch-to-member 0 --build-at 200 --wait-for ${WORK}/host.txt)
elseif(MODE STREQUAL "refused")
    set(hostArgs --players 2)
    set(joinArgs --switch-to-member 0 --switch-now --wait-for ${WORK}/host.txt)
elseif(MODE STREQUAL "kick")
    set(hostArgs --players 1 --members 1 --kick-members)
    set(joinArgs --member-of 0 --expect-kick)
elseif(MODE STREQUAL "swap")
    set(hostArgs --players 1 --members 1 --members-via-lobby --swap-with 1)
    set(ai dummy)
    set(joinArgs --member-of 0 --build-at 200 --wait-for ${WORK}/host.txt)
else()
    message(FATAL_ERROR "Unknown MODE ${MODE}")
endif()
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env HOME=${WORK}/host USER=coop ${COOP_NET} host ${common} --map ${MAP} --ai ${ai}
            ${hostArgs} --out ${WORK}/host.txt --log ${WORK}/host.log
    COMMAND ${CMAKE_COMMAND} -E env HOME=${WORK}/join USER=coop ${COOP_NET} join ${common} ${joinArgs}
            --out ${WORK}/join.txt --log ${WORK}/join.log
    RESULTS_VARIABLE results
    ERROR_VARIABLE errors)
message(STATUS "Exit codes (host join): ${results}\n${errors}")
foreach(role host join)
    if(EXISTS ${WORK}/${role}.log)
        file(READ ${WORK}/${role}.log log)
        string(REGEX MATCHALL "[^\n]*(Async|error|Error|member|Member|Switch|Ordered)[^\n]*" important "${log}")
        message(STATUS "${role}: ${important}")
    endif()
endforeach()
foreach(result IN LISTS results)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Expected every process to exit with 0")
    endif()
endforeach()
file(READ ${WORK}/host.txt host)
file(READ ${WORK}/join.txt join)
message(STATUS "host: ${host}\njoin: ${join}")

function(expect text pattern)
    if(NOT text MATCHES "${pattern}")
        message(FATAL_ERROR "Expected '${pattern}' in:\n${text}")
    endif()
endfunction()

expect("${host}" "State at GF ${MAX_GF}: ")
if(MODE STREQUAL "kick")
    expect("${join}" "Kicked by the host in the lobby")
    # ... and not by a connection that simply broke
    file(READ ${WORK}/host.log hostLog)
    expect("${hostLog}" "Member Client of player 0 removed: kicked by the host")
    expect("${host}" "Members at start: Client@0")
    expect("${host}" "Members at the end: none")
    return()
endif()
if(MODE STREQUAL "switch")
    expect("${join}" "Switched to a member of player 0")
    expect("${host}" "Members at start: Client@0")
    expect("${host}" "woodcutters [1-9]")
elseif(MODE STREQUAL "swap")
    expect("${host}" "Members at start: Client@1")
    # The dummy AI now in slot 0 builds nothing; the woodcutter is the member's, for the host's new slot 1
    expect("${host}" "woodcutters 0 [1-9]")
else()
    expect("${join}" "Switch refused, still player 1")
    expect("${host}" "Members at start: none")
endif()
string(REGEX MATCH "State at GF ${MAX_GF}: [^\n]*" hostState "${host}")
string(REGEX MATCH "State at GF ${MAX_GF}: [^\n]*" joinState "${join}")
if(NOT hostState STREQUAL joinState)
    message(FATAL_ERROR "The client is out of sync with the host:\n${hostState}\n${joinState}")
endif()
