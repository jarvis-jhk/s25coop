# Copyright (C) 2026 s25coop contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Refusal must follow a sent, authenticated host-management probe, not an unrelated setup failure.
if(NOT MODE MATCHES "^(normal|state|swap|settings)$")
    message(FATAL_ERROR "Unknown member authorization mode: ${MODE}")
endif()
file(LOCK "$ENV{HOME}/.cache/s25coop/coop-net-${PORT}.lock" GUARD PROCESS TIMEOUT 100)
file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK}/host ${WORK}/member)
set(common --port ${PORT} --maxGF ${MAX_GF} --timeout 30)
set(probe)
if(NOT MODE STREQUAL "normal")
    set(probe --member-host-probe ${MODE})
endif()
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env HOME=${WORK}/host USER=coop ${COOP_NET} host ${common} --map ${MAP}
            --ai dummy --players 1 --members 1 --out ${WORK}/host.txt --log ${WORK}/host.log ${probe}
    COMMAND ${CMAKE_COMMAND} -E env HOME=${WORK}/member USER=coop ${COOP_NET} join ${common} --member-of 0
            --after ${WORK}/host.txt.connected --out ${WORK}/member.txt --wait-for ${WORK}/host.txt
            --log ${WORK}/member.log --build-at 200 ${probe}
    RESULTS_VARIABLE results ERROR_VARIABLE errors)
message(STATUS "Exit codes (host;member): ${results}\n${errors}")
file(READ ${WORK}/host.log hostLog)
file(READ ${WORK}/member.log memberLog)
list(GET results 0 hostResult)
list(GET results 1 memberResult)
if(NOT MODE STREQUAL "normal")
    if(NOT memberLog MATCHES "Sent authenticated member host probe: ${MODE}"
       OR NOT hostLog MATCHES "Client joined player 0 as a member")
        message(FATAL_ERROR "The probe was not sent through an authenticated member connection")
    endif()
    file(READ ${WORK}/host.txt.probe-sent receipt)
    if(NOT receipt STREQUAL "${MODE}\n")
        message(FATAL_ERROR "Wrong sent probe receipt: ${receipt}")
    endif()
    if(NOT hostLog MATCHES "Member Client of player 0 removed: unexpected message")
        message(FATAL_ERROR "The server did not refuse the sent ${MODE} host-management probe")
    endif()
    if(NOT memberResult STREQUAL "4" OR EXISTS ${WORK}/member.txt)
        message(FATAL_ERROR "The forbidden member was not disconnected before game start: ${results}")
    endif()
    if(NOT hostLog MATCHES "Host configuration unchanged after member host probe: ${MODE}")
        message(FATAL_ERROR "Host-owned configuration was not preserved after refusal")
    endif()
endif()
if(NOT hostResult STREQUAL "0")
    message(FATAL_ERROR "The host did not continue to its final GF: ${results}\n${errors}")
endif()
file(READ ${WORK}/host.txt host)
if(NOT host MATCHES "State at GF ${MAX_GF}: ([^\n]*)")
    message(FATAL_ERROR "The host did not reach exact GF ${MAX_GF}: ${host}")
endif()
set(hostState "${CMAKE_MATCH_1}")
if(NOT host MATCHES "Members at start: Client@0"
   OR NOT host MATCHES "Start goods: p0 boards 44 stones 68 helpers [1-9][0-9]*; p1 boards 44 stones 68 helpers [1-9][0-9]*;")
    message(FATAL_ERROR "Wrong member/host topology or host start-goods settings: ${host}")
endif()
if(MODE STREQUAL "normal")
    if(NOT memberResult STREQUAL "0" OR hostLog MATCHES "removed: unexpected message")
        message(FATAL_ERROR "The ordinary member control failed: ${results}\n${errors}")
    endif()
    file(READ ${WORK}/member.txt member)
    if(NOT member MATCHES "State at GF ${MAX_GF}: ([^\n]*)" OR NOT CMAKE_MATCH_1 STREQUAL hostState
       OR NOT hostState MATCHES "woodcutters [1-9][0-9]* 0")
        message(FATAL_ERROR "The normal member's real order did not converge with the host: ${host}\n${member}")
    endif()
else()
    if(NOT host MATCHES "Members at the end: none[\r\n]*$" OR NOT hostState MATCHES "woodcutters 0 0")
        message(FATAL_ERROR "The rejected member altered the world: ${hostState}")
    endif()
endif()
