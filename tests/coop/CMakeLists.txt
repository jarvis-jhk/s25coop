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
