# Copyright (C) 2026 s25coop contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# An otherwise valid AI slot remains owned by the host when a real network member requests it as an extra
# local player. The control uses exactly the same host/member topology without the extra-slot request.
if(NOT MODE STREQUAL "normal" AND NOT MODE STREQUAL "extra")
    message(FATAL_ERROR "Unknown local-slot test mode: ${MODE}")
endif()
# RUN_SERIAL covers one CTest invocation; serialize this port across other builds of the same test as well.
file(LOCK "$ENV{HOME}/.cache/s25coop/coop-net-${PORT}.lock" GUARD PROCESS TIMEOUT 200)
file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK}/host ${WORK}/member)
set(common --port ${PORT} --maxGF ${MAX_GF} --timeout 120 --trace 20)
set(memberArgs)
if(MODE STREQUAL "extra")
    set(memberArgs --extra-local-slot 1)
endif()
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env HOME=${WORK}/host USER=coop ${COOP_NET} host ${common} --map ${MAP}
            --ai dummy --players 1 --members 1 --out ${WORK}/host.txt --log ${WORK}/host.log
    COMMAND ${CMAKE_COMMAND} -E env HOME=${WORK}/member USER=coop ${COOP_NET} join ${common} --member-of 0
            --after ${WORK}/host.txt.connected --out ${WORK}/member.txt --wait-for ${WORK}/host.txt
            --log ${WORK}/member.log ${memberArgs}
    RESULTS_VARIABLE results ERROR_VARIABLE errors)
message(STATUS "Exit codes (host;member): ${results}\n${errors}")
list(GET results 0 hostResult)
list(GET results 1 memberResult)
if(NOT hostResult STREQUAL "0")
    message(FATAL_ERROR "The host did not continue after the member connection: ${results}\n${errors}")
endif()
file(READ ${WORK}/host.txt hostState)
if(NOT hostState MATCHES "State at GF ${MAX_GF}: ([^\n]*)")
    message(FATAL_ERROR "The host did not finish GF ${MAX_GF}: ${hostState}")
endif()
set(expectedState "${CMAKE_MATCH_1}")
if(NOT hostState MATCHES "Members at start: Client@0" OR NOT hostState MATCHES "Start goods: [^\n]*p1 boards")
    message(FATAL_ERROR "The test did not start with a real member and spare AI slot: ${hostState}")
endif()
if(MODE STREQUAL "extra")
    if(NOT memberResult STREQUAL "4" OR NOT errors MATCHES "LocalPlayerSetup: loading=0 started=0")
        message(FATAL_ERROR "Expected the local-slot guard before loading or starting the member: ${results}\n${errors}")
    endif()
    if(EXISTS ${WORK}/member.txt)
        message(FATAL_ERROR "The rejected member produced a game result")
    endif()
    if(NOT hostState MATCHES "Members at the end: none[\r\n]*$")
        message(FATAL_ERROR "The rejected member stayed attached to the host: ${hostState}")
    endif()
else()
    if(NOT memberResult STREQUAL "0" OR errors MATCHES "LocalPlayerSetup:")
        message(FATAL_ERROR "The ordinary member connection failed: ${results}\n${errors}")
    endif()
    file(READ ${WORK}/member.txt memberState)
    if(NOT memberState MATCHES "State at GF ${MAX_GF}: ([^\n]*)" OR NOT CMAKE_MATCH_1 STREQUAL expectedState)
        message(FATAL_ERROR "The ordinary member did not finish in sync:\n${hostState}\n${memberState}")
    endif()
endif()
