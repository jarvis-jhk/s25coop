# Copyright (C) 2026 s25coop contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Records a headless game as a replay, replays it with ai-battle --check-replay and fails unless the replay stays in
# sync and ends in the same state. Then replays it with a different seed, which must be reported as async: a checker
# that never says "async" proves nothing.
# Usage: cmake -DAI_BATTLE=<exe> -DARGS=<ai-battle arguments separated by |> -DREPLAY=<file> -P checkReplay.cmake

string(REPLACE "|" ";" ARGS "${ARGS}")
file(REMOVE "${REPLAY}")

execute_process(COMMAND ${AI_BATTLE} ${ARGS} --replay ${REPLAY} RESULT_VARIABLE result OUTPUT_VARIABLE output
                ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Recording failed (${result}):\n${output}\n${err}")
endif()
string(REGEX MATCH "Final state: [^\n]*" recorded "${output}")
if(NOT recorded OR NOT EXISTS "${REPLAY}")
    message(FATAL_ERROR "Recording printed no final state or wrote no replay:\n${output}")
endif()

execute_process(COMMAND ${AI_BATTLE} --check-replay ${REPLAY} RESULT_VARIABLE result OUTPUT_VARIABLE output
                ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Replay check failed (${result}):\n${output}\n${err}")
endif()
string(REGEX MATCH "Final state: [^\n]*" replayed "${output}")
if(NOT recorded STREQUAL replayed)
    message(FATAL_ERROR "Replay ended in a different state:\n  recorded ${recorded}\n  replayed ${replayed}")
endif()
string(REGEX MATCH "Replay in sync: [^\n]*" inSync "${output}")

execute_process(COMMAND ${AI_BATTLE} --check-replay ${REPLAY} --random_init 424242 RESULT_VARIABLE result
                OUTPUT_VARIABLE output ERROR_VARIABLE err)
if(NOT result EQUAL 3 OR NOT output MATCHES "Replay ASYNC at GF")
    message(FATAL_ERROR "A replay with the wrong seed was not reported as async (${result}):\n${output}\n${err}")
endif()
message(STATUS "${inSync}, ${recorded}")
