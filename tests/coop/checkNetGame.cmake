# Copyright (C) 2026 s25coop contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Network test harness (ROADMAP M2 step 0): a host and a joining client, each coop-net process with its own home folder,
# play one game over localhost. The server compares both checksums at every network frame.
# Inputs: COOP_NET (binary), MAP, PORT, MAX_GF, WORK (scratch folder), EXPECT (0 = in sync, 3 = must detect an async),
# JOIN_ARGS (extra arguments for the client, | separated)
string(REPLACE "|" ";" JOIN_ARGS "${JOIN_ARGS}")
file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK}/host ${WORK}/join)
set(common --port ${PORT} --maxGF ${MAX_GF} --timeout 120)
# Both commands run at the same time (execute_process pipes one into the other; with --log neither writes to the pipe)
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env HOME=${WORK}/host USER=coop ${COOP_NET} host ${common} --map ${MAP} --ai aijh
            --out ${WORK}/host.txt --log ${WORK}/host.log
    COMMAND ${CMAKE_COMMAND} -E env HOME=${WORK}/join USER=coop ${COOP_NET} join ${common} --out ${WORK}/join.txt
            --wait-for ${WORK}/host.txt --log ${WORK}/join.log ${JOIN_ARGS}
    RESULTS_VARIABLE results
    ERROR_VARIABLE errors)
message(STATUS "Exit codes (host;join): ${results}\n${errors}")
foreach(role host join)
    if(EXISTS ${WORK}/${role}.log)
        file(READ ${WORK}/${role}.log log)
        string(REGEX MATCHALL "[^\n]*(Async|error|Error|Reached)[^\n]*" important "${log}")
        message(STATUS "${role}: ${important}")
    endif()
endforeach()
if(NOT results STREQUAL "${EXPECT};${EXPECT}")
    message(FATAL_ERROR "Expected both to exit with ${EXPECT}")
endif()
if(EXPECT EQUAL 0)
    foreach(role host join)
        file(READ ${WORK}/${role}.txt result)
        if(NOT result MATCHES "Reached GF [0-9]+ in sync")
            message(FATAL_ERROR "${role} did not reach GF ${MAX_GF}: ${result}")
        endif()
    endforeach()
endif()
