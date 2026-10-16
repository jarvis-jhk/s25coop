# Copyright (C) 2026 s25coop contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# A save written by unmodified upstream RttR, from before s25coop existed, resumes as a network game: the host takes the
# saved human tribe, a co-player joins it and orders a woodcutter for it, the seven saved AIs keep playing, both stay
# in sync. Inputs: COOP_NET, SAVE, FROM_GF (the save's GF), RUN_GF, PORT, WORK
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/host" "${WORK}/member")
set(env ${CMAKE_COMMAND} -E env USER=coop)
math(EXPR toGF "${FROM_GF} + ${RUN_GF}")
math(EXPR buildAt "${FROM_GF} + 50")
execute_process(
    COMMAND ${env} HOME=${WORK}/host ${COOP_NET} host --players 1 --members 1 --members-via-lobby
            --savegame ${SAVE} --port ${PORT} --maxGF ${toGF} --timeout 120
            --out ${WORK}/host.txt --log ${WORK}/host.log
    COMMAND ${env} HOME=${WORK}/member ${COOP_NET} join --member-of-host --port ${PORT} --maxGF ${toGF}
            --timeout 120 --build-at ${buildAt} --after ${WORK}/host.txt.connected
            --wait-for ${WORK}/host.txt --out ${WORK}/member.txt --log ${WORK}/member.log
    RESULTS_VARIABLE results ERROR_VARIABLE errors)
message(STATUS "Host/member exits ${results}")
list(GET results 0 hostResult)
list(GET results 1 memberResult)
if(NOT hostResult EQUAL 0 OR NOT memberResult EQUAL 0)
    message(FATAL_ERROR "Host/member failed: ${results}\n${errors}")
endif()
file(READ "${WORK}/host.txt" host)
file(READ "${WORK}/member.txt" member)
file(READ "${WORK}/member.log" memberLog)
if(NOT host MATCHES "Members at start: Client@0" OR NOT memberLog MATCHES "Ordered a woodcutter for player 0")
    message(FATAL_ERROR "The co-player did not join the saved human tribe and order for it: ${host}")
endif()
string(REGEX MATCH "Player roles: [^\n]*" roles "${host}")
string(REGEX MATCHALL " AI [0-9]+ level [0-9]+;" ais "${roles}")
list(LENGTH ais numAis)
if(NOT roles MATCHES "^Player roles: p0 state 1;" OR NOT numAis EQUAL 7)
    message(FATAL_ERROR "Expected the saved human tribe and seven saved AIs: ${roles}")
endif()
foreach(role host member)
    string(REGEX MATCH "Started at GF ${FROM_GF}: checksum [0-9]+, woodcutters ([0-9]+)" start "${${role}}")
    set(startWood_${role} "${CMAKE_MATCH_1}")
    string(REGEX MATCH "Started at GF ${FROM_GF}: [^\n]*" start_${role} "${${role}}")
    string(REGEX MATCH "State at GF ${toGF}: checksum [0-9]+, woodcutters ([0-9]+)" state "${${role}}")
    set(endWood_${role} "${CMAKE_MATCH_1}")
    string(REGEX MATCH "State at GF ${toGF}: [^\n]*" state_${role} "${${role}}")
    string(REGEX MATCH "Loaded economy:[^\n]*" economy_${role} "${${role}}")
    string(REGEX MATCH "Player roles: [^\n]*" roles_${role} "${${role}}")
    if(start_${role} STREQUAL "" OR state_${role} STREQUAL "" OR NOT ${role} MATCHES "Reached GF [0-9]+ in sync")
        message(FATAL_ERROR "${role} did not resume the upstream save at GF ${FROM_GF} and finish in sync: ${${role}}")
    endif()
endforeach()
if(NOT start_host STREQUAL start_member OR NOT state_host STREQUAL state_member
   OR NOT economy_host STREQUAL economy_member OR NOT roles_host STREQUAL roles_member)
    message(FATAL_ERROR "Host and member loaded or reached different worlds:\n${host}\n${member}")
endif()
math(EXPR expectedWood "${startWood_host} + 1")
if(NOT endWood_host EQUAL expectedWood)
    message(FATAL_ERROR "The co-player's woodcutter is missing: ${startWood_host} -> ${endWood_host}")
endif()
message(STATUS "${state_host}; ${roles}")
