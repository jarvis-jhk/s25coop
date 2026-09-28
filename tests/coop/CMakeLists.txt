# Copyright (C) 2026 s25coop contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# s25coop headless tests (ROADMAP M0.5): ai-battle plays a map at full speed without video or audio,
# and a Lua script asserts on the game state. Exit code 2 = Lua error or failed assertion.
set(coopTestMap ${PROJECT_SOURCE_DIR}/tests/testData/maps/LuaFunctions.SWD)

# Optional further arguments: the AI of each player (default: two aijh)
function(coop_headless_args out script maxGF)
    set(ais ${ARGN})
    if(NOT ais)
        set(ais aijh aijh)
    endif()
    list(TRANSFORM ais PREPEND "--ai;")
    set(${out}
        --test --map ${coopTestMap} --lua ${CMAKE_CURRENT_SOURCE_DIR}/headless/${script} ${ais}
        --random_init 1 --random_ai_init 1 --maxGF ${maxGF}
        PARENT_SCOPE)
endfunction()

function(add_coop_headless_test name script maxGF)
    coop_headless_args(args ${script} ${maxGF} ${ARGN})
    add_test(NAME ${name} COMMAND ai-battle ${args} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endfunction()

add_coop_headless_test(CoopHeadless_Smoke smoke.lua 6000)
# Scripted input: player 0 does nothing on its own, the script builds for it
add_coop_headless_test(CoopHeadless_ScriptedInput scriptedInput.lua 6000 dummy aijh)
# The harness must notice a failing script, or a green run proves nothing. Matching the message rather than
# WILL_FAIL: a run that fails for another reason (map not found) must not count as a pass.
add_coop_headless_test(CoopHeadless_FailingScriptFails mustFail.lua 100)
set_tests_properties(CoopHeadless_FailingScriptFails PROPERTIES PASS_REGULAR_EXPRESSION "Lua error: .*expected failure")

coop_headless_args(determinismArgs smoke.lua 6000)
string(REPLACE ";" "|" determinismArgs "${determinismArgs}") # a ; would split the -D argument
add_test(NAME CoopHeadless_Deterministic
         COMMAND ${CMAKE_COMMAND} -DAI_BATTLE=$<TARGET_FILE:ai-battle> "-DARGS=${determinismArgs}" -P
                 ${CMAKE_CURRENT_SOURCE_DIR}/checkDeterminism.cmake
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# Replay regression: record the scripted-input game (AI and script commands) and replay it in sync
coop_headless_args(replayArgs scriptedInput.lua 6000 dummy aijh)
string(REPLACE ";" "|" replayArgs "${replayArgs}")
add_test(NAME CoopHeadless_ReplayInSync
         COMMAND ${CMAKE_COMMAND} -DAI_BATTLE=$<TARGET_FILE:ai-battle> "-DARGS=${replayArgs}"
                 -DREPLAY=${CMAKE_CURRENT_BINARY_DIR}/replayInSync.rpl -P ${CMAKE_CURRENT_SOURCE_DIR}/checkReplay.cmake
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# Campaign mission smoke tests (ROADMAP M0.5b): each official mission loads with its own script and runs 30000 frames
# (about 25 game minutes) without a Lua error. The maps are original Settlers II data, which can never be in this
# repository or in public CI, so these exist only when a local S2 folder (with DATA and GFX) is configured.
set(RTTR_COOP_S2_DIR "" CACHE PATH "Original Settlers II folder (DATA, GFX) for local campaign tests")
if(RTTR_COOP_S2_DIR)
    # campaign;map folder;map;players on the map
    set(coopMissions
        "roman|MAPS|MISS200|1" "roman|MAPS|MISS201|2" "roman|MAPS|MISS202|3" "roman|MAPS|MISS203|3"
        "roman|MAPS|MISS204|3" "roman|MAPS|MISS205|3" "roman|MAPS|MISS206|2" "roman|MAPS|MISS207|3"
        "roman|MAPS|MISS208|3" "roman|MAPS|MISS209|3" "world|MAPS2|EUROPE|6" "world|MAPS2|NAMERICA|3"
        "world|MAPS2|SAMERICA|3" "world|MAPS2|GREEN|2" "world|MAPS2|AFRICA|4" "world|MAPS2|NASIA|4"
        "world|MAPS2|SASIA|3" "world|MAPS2|JAPAN|2" "world|MAPS2|AUSTRA|2")
    # Chapter order as in each campaign.lua's maps list
    set(coopChapters_roman MISS200 MISS201 MISS202 MISS203 MISS204 MISS205 MISS206 MISS207 MISS208 MISS209)
    set(coopChapters_world EUROPE AFRICA NAMERICA SAMERICA GREEN AUSTRA NASIA SASIA JAPAN)
    foreach(mission IN LISTS coopMissions)
        string(REPLACE "|" ";" mission "${mission}")
        list(GET mission 0 campaign)
        list(GET mission 1 folder)
        list(GET mission 2 map)
        list(GET mission 3 numPlayers)
        set(ais "")
        foreach(i RANGE 1 ${numPlayers})
            list(APPEND ais --ai aijh)
        endforeach()
        add_test(NAME CoopMission_${campaign}_${map}
                 COMMAND ai-battle --test --objective none --map ${RTTR_COOP_S2_DIR}/DATA/${folder}/${map}.WLD
                         --lua ${PROJECT_SOURCE_DIR}/data/RTTR/campaigns/${campaign}/${map}.lua
                         --test-script ${CMAKE_CURRENT_SOURCE_DIR}/headless/missionSmoke.lua ${ais}
                         --random_init 1 --random_ai_init 1 --maxGF 30000
                 WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
        set_tests_properties(CoopMission_${campaign}_${map} PROPERTIES ENVIRONMENT "RTTR_GAME_DIR=${RTTR_COOP_S2_DIR}")

        # Campaign progress (ROADMAP M1): finishing the mission records exactly its own chapter (its index in the
        # campaign's map list) as completed
        list(FIND coopChapters_${campaign} ${map} chapter)
        string(REPEAT "." ${chapter} before)
        add_test(NAME CoopCampaign_${campaign}_${map}
                 COMMAND ai-battle --test --objective none --map ${RTTR_COOP_S2_DIR}/DATA/${folder}/${map}.WLD
                         --lua ${PROJECT_SOURCE_DIR}/data/RTTR/campaigns/${campaign}/${map}.lua
                         --test-script ${CMAKE_CURRENT_SOURCE_DIR}/headless/missionComplete.lua ${ais}
                         --random_init 1 --random_ai_init 1 --maxGF 100
                 WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
        set_tests_properties(CoopCampaign_${campaign}_${map} PROPERTIES
            ENVIRONMENT "RTTR_GAME_DIR=${RTTR_COOP_S2_DIR}"
            PASS_REGULAR_EXPRESSION "Campaign progress: ${campaign}=${before}2[0-9]*[\r\n]+TEST PASSED")
    endforeach()

    # Mission walkthroughs: a script plays the mission as player 0 (a dummy AI) and asserts that the mission's own
    # triggers fire in order (buildings, occupied spots, geologist finds)
    add_test(NAME CoopWalkthrough_roman_MISS200
             COMMAND ai-battle --test --objective none --map ${RTTR_COOP_S2_DIR}/DATA/MAPS/MISS200.WLD
                     --lua ${PROJECT_SOURCE_DIR}/data/RTTR/campaigns/roman/MISS200.lua
                     --test-script ${CMAKE_CURRENT_SOURCE_DIR}/headless/miss200Walkthrough.lua --ai dummy
                     --random_init 1 --random_ai_init 1 --maxGF 20000
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(CoopWalkthrough_roman_MISS200 PROPERTIES ENVIRONMENT "RTTR_GAME_DIR=${RTTR_COOP_S2_DIR}")
endif()

# Network test harness (ROADMAP M2 step 0): host + client processes over localhost, headless. Not on Windows: both
# processes would share one user folder there, and an async opens a message box (GameServer::SendAsyncLog).
if(NOT WIN32)
    set(coopNetMap ${PROJECT_SOURCE_DIR}/data/RTTR/MAPS/NEW/KARTE06-fix.swd)
    function(add_coop_net_test name port expect)
        string(REPLACE ";" "|" joinArgs "${ARGN}")
        add_test(NAME ${name}
                 COMMAND ${CMAKE_COMMAND} -DCOOP_NET=$<TARGET_FILE:coop-net> -DMAP=${coopNetMap} -DPORT=${port}
                         -DMAX_GF=1000 -DWORK=${CMAKE_CURRENT_BINARY_DIR}/${name} -DEXPECT=${expect}
                         "-DJOIN_ARGS=${joinArgs}" -P ${CMAKE_CURRENT_SOURCE_DIR}/checkNetGame.cmake)
        set_tests_properties(${name} PROPERTIES TIMEOUT 200 RUN_SERIAL TRUE)
    endfunction()
    add_coop_net_test(CoopNet_TwoClientsInSync 13710 0)
    # The harness must catch a client whose world diverges, or a green run proves nothing
    add_coop_net_test(CoopNet_DesyncDetected 13711 3 --desync-at 300)
    # ... also in the last frames before the end: the run must not end before their checksums were compared
    add_coop_net_test(CoopNet_DesyncAtEndDetected 13712 3 --desync-at 999)
endif()
