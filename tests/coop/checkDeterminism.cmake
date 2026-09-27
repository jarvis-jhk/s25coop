# Copyright (C) 2026 s25coop contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Runs the same headless game twice (same map, seeds and frame count) and fails if the final state differs.
# Coop sends every player's commands over the network and relies on all machines computing the same game;
# a change that makes the simulation depend on anything else breaks that, and this catches it.
# Usage: cmake -DAI_BATTLE=<exe> -DARGS=<ai-battle arguments separated by |> -P checkDeterminism.cmake

string(REPLACE "|" ";" ARGS "${ARGS}")

foreach(run 1 2)
    execute_process(COMMAND ${AI_BATTLE} ${ARGS} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Run ${run} failed (${result}):\n${output}\n${err}")
    endif()
    string(REGEX MATCH "Final state: [^\n]*" state${run} "${output}")
    if(NOT state${run})
        message(FATAL_ERROR "Run ${run} printed no final state:\n${output}")
    endif()
endforeach()
if(NOT state1 STREQUAL state2)
    message(FATAL_ERROR "Same seed, different game:\n  ${state1}\n  ${state2}")
endif()
message(STATUS "Deterministic: ${state1}")
