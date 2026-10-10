# Copyright (C) 2026 s25coop contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Make a real solo save, resume it with an online co-player, save again and reconnect the same group.
# Public map only: this regression runs in CI without original S2 data. It does not claim historical-save parity.
# Inputs: COOP_NET, MAP, PORT, MAX_GF, WORK, MODE (sync, desync or desync-resave)
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/solo" "${WORK}/host" "${WORK}/member")
set(env ${CMAKE_COMMAND} -E env USER=coop)
execute_process(
    COMMAND ${env} HOME=${WORK}/solo ${COOP_NET} host --single-player --players 1 --map ${MAP} --ai aijh dummy
            --port ${PORT} --maxGF ${MAX_GF} --timeout 90 --build-at 200 --save ${WORK}/solo.sav
            --out ${WORK}/solo.txt --log ${WORK}/solo.log
    RESULT_VARIABLE result ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Solo save failed (${result}): ${errors}")
endif()
file(READ "${WORK}/solo.log" soloLog)
file(READ "${WORK}/solo.txt" solo)
if(NOT soloLog MATCHES "Host session: single-player" OR NOT solo MATCHES "Members at start: none"
   OR NOT solo MATCHES "State at GF ${MAX_GF}: [^\n]* woodcutters 1 [0-9]+ 0 0"
   OR NOT EXISTS "${WORK}/solo.sav")
    message(FATAL_ERROR "Expected a played solo world, one human's order and no co-players: ${solo}")
endif()
string(REGEX MATCH "Player roles: [^\n]*" soloRoles "${solo}")
if(NOT soloRoles MATCHES "Player roles: p0 state 1; p1 state 3 AI 1 level 2; p2 state 3 AI 0 level 0; p3 state 2;$")
    message(FATAL_ERROR "The solo fixture lacks its human and distinct saved AI types: ${soloRoles}")
endif()

function(resume phase save previousResult fromGF toGF expectedWoodcutters)
    math(EXPR port "${PORT} + ${phase}")
    math(EXPR buildAt "${fromGF} + 200")
    set(memberArgs --build-at ${buildAt})
    set(hostArgs --save ${WORK}/online.sav)
    set(desync OFF)
    if(MODE STREQUAL "desync" OR (MODE STREQUAL "desync-resave" AND phase EQUAL 2))
        set(desync ON)
        math(EXPR desyncAt "${fromGF} + 300")
        list(APPEND memberArgs --desync-at ${desyncAt})
        set(hostArgs)
    endif()
    execute_process(
        COMMAND ${env} HOME=${WORK}/host ${COOP_NET} host --players 1 --members 1 --members-via-lobby
                --savegame ${save} --port ${port} --maxGF ${toGF} --timeout 90 ${hostArgs}
                --out ${WORK}/host${phase}.txt --log ${WORK}/host${phase}.log
        COMMAND ${env} HOME=${WORK}/member ${COOP_NET} join --member-of-host --port ${port} --maxGF ${toGF}
                --timeout 90 ${memberArgs} --after ${WORK}/host${phase}.txt.connected
                --wait-for ${WORK}/host${phase}.txt --out ${WORK}/member${phase}.txt --log ${WORK}/member${phase}.log
        RESULTS_VARIABLE results ERROR_VARIABLE errors)
    message(STATUS "Resume ${phase}: host/member exits ${results}")
    list(GET results 0 hostResult)
    list(GET results 1 memberResult)
    file(READ "${WORK}/host${phase}.txt" host)
    file(READ "${WORK}/host${phase}.log" hostLog)
    if(NOT hostResult EQUAL 0 OR NOT hostLog MATCHES "Host session: network"
       OR NOT host MATCHES "Members at start: Client@0"
       OR NOT host MATCHES "Started at GF ${fromGF}: "
       OR NOT host MATCHES "State at GF ${toGF}: [^\n]* woodcutters ${expectedWoodcutters} [0-9]+ 0 0")
        message(FATAL_ERROR "Resume ${phase}: host did not continue the solo tribe with the member's order: ${host}\n${errors}")
    endif()
    string(REGEX MATCH "Player roles: [^\n]*" roles "${host}")
    if(NOT roles STREQUAL soloRoles)
        message(FATAL_ERROR "Resume ${phase}: the saved AI tribes changed: ${roles} vs ${soloRoles}")
    endif()
    file(READ "${previousResult}" previous)
    string(REGEX MATCH "Economy at GF ${fromGF}:([^\n]*)" match "${previous}")
    set(savedEconomy "${CMAKE_MATCH_1}")
    string(REGEX MATCH "Loaded economy:([^\n]*)" match "${host}")
    if(savedEconomy STREQUAL "" OR NOT CMAKE_MATCH_1 STREQUAL savedEconomy)
        message(FATAL_ERROR "Resume ${phase}: saved inventories, HQs or buildings/sites changed on host load")
    endif()
    if(desync)
        if(NOT memberResult EQUAL 4 OR NOT hostLog MATCHES "Member Client of player 0 removed: out of sync"
           OR NOT errors MATCHES "Client error [0-9]+: Your game went out of sync")
            message(FATAL_ERROR "Expected a resumed member desync, its removal and continued host play: ${errors}")
        endif()
        file(READ "${WORK}/member${phase}.log" memberLog)
        if(NOT memberLog MATCHES "Started at GF ${fromGF}: "
           OR NOT memberLog MATCHES "Injected desync at GF ${desyncAt}[\r\n]")
            message(FATAL_ERROR "The deliberate desync probe never executed at its scheduled GF in the loaded world")
        endif()
        string(REGEX MATCH "Loaded economy:([^\n]*)" match "${memberLog}")
        if(NOT CMAKE_MATCH_1 STREQUAL savedEconomy)
            message(FATAL_ERROR "Resume ${phase}: desync member did not first load the saved economy")
        endif()
        string(REGEX MATCH "Player roles: [^\n]*" memberRoles "${memberLog}")
        string(REGEX MATCH "Started at GF ${fromGF}: [^\n]*" memberStart "${memberLog}")
        string(REGEX MATCH "Started at GF ${fromGF}: [^\n]*" hostStart "${host}")
        if(NOT memberRoles STREQUAL soloRoles OR NOT memberStart STREQUAL hostStart)
            message(FATAL_ERROR "Resume ${phase}: desync member initial world or tribes differ from host")
        endif()
        return()
    endif()
    if(NOT memberResult EQUAL 0)
        message(FATAL_ERROR "Resume ${phase}: member failed (${memberResult}): ${errors}")
    endif()
    file(READ "${WORK}/member${phase}.txt" member)
    foreach(role host member)
        string(REGEX MATCH "State at GF ${toGF}: [^\n]*" state_${role} "${${role}}")
        string(REGEX MATCH "Started at GF ${fromGF}: [^\n]*" start_${role} "${${role}}")
        string(REGEX MATCH "Player roles: [^\n]*" roles "${${role}}")
        string(REGEX MATCH "Loaded economy:([^\n]*)" match "${${role}}")
        if(NOT CMAKE_MATCH_1 STREQUAL savedEconomy)
            message(FATAL_ERROR "Resume ${phase}: saved economy changed on ${role} load")
        endif()
        if(NOT roles STREQUAL soloRoles OR NOT ${role} MATCHES "Reached GF [0-9]+ in sync"
           OR start_${role} STREQUAL "")
            message(FATAL_ERROR "Resume ${phase}: ${role} did not load the original tribes and finish in sync")
        endif()
    endforeach()
    if(NOT state_host STREQUAL state_member OR NOT start_host STREQUAL start_member)
        message(FATAL_ERROR "Resume ${phase}: host/member initial or final world states differ")
    endif()
    message(STATUS "Resume ${phase}: ${state_host}; ${soloRoles}")
endfunction()

math(EXPR secondGF "${MAX_GF} * 2")
resume(1 "${WORK}/solo.sav" "${WORK}/solo.txt" ${MAX_GF} ${secondGF} 2)
if(NOT MODE STREQUAL "desync")
    math(EXPR thirdGF "${MAX_GF} * 3")
    resume(2 "${WORK}/online.sav" "${WORK}/host1.txt" ${secondGF} ${thirdGF} 3)
endif()
