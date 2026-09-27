# Copyright (C) 2026 s25coop contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# s25coop headless tests (ROADMAP M0.5): ai-battle plays a map at full speed without video or audio,
# and a Lua script asserts on the game state. Exit code 2 = Lua error or failed assertion.
set(coopTestMap ${PROJECT_SOURCE_DIR}/tests/testData/maps/LuaFunctions.SWD)

function(coop_headless_args out script maxGF)
    set(${out}
        --test --map ${coopTestMap} --lua ${CMAKE_CURRENT_SOURCE_DIR}/headless/${script} --ai aijh --ai aijh
        --random_init 1 --random_ai_init 1 --maxGF ${maxGF}
        PARENT_SCOPE)
endfunction()

function(add_coop_headless_test name script maxGF)
    coop_headless_args(args ${script} ${maxGF})
    add_test(NAME ${name} COMMAND ai-battle ${args} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endfunction()

add_coop_headless_test(CoopHeadless_Smoke smoke.lua 6000)
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
    endforeach()
endif()
