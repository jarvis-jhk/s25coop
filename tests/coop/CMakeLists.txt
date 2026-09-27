# Copyright (C) 2026 s25coop contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# s25coop headless tests (ROADMAP M0.5): ai-battle plays a map at full speed without video or audio,
# and a Lua script asserts on the game state. Exit code 2 = Lua error or failed assertion.
set(coopTestMap ${PROJECT_SOURCE_DIR}/tests/testData/maps/LuaFunctions.SWD)

function(add_coop_headless_test name script maxGF)
    add_test(NAME ${name}
             COMMAND ai-battle --test --map ${coopTestMap} --lua ${CMAKE_CURRENT_SOURCE_DIR}/headless/${script}
                     --ai aijh --ai aijh --random_init 1 --random_ai_init 1 --maxGF ${maxGF}
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endfunction()

add_coop_headless_test(CoopHeadless_Smoke smoke.lua 6000)
# The harness must notice a failing script, or a green run proves nothing. Matching the message rather than
# WILL_FAIL: a run that fails for another reason (map not found) must not count as a pass.
add_coop_headless_test(CoopHeadless_FailingScriptFails mustFail.lua 100)
set_tests_properties(CoopHeadless_FailingScriptFails PROPERTIES PASS_REGULAR_EXPRESSION "Lua error: .*expected failure")
