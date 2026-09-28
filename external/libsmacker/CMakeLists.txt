# Copyright (C) 2026 s25coop contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# libsmacker (github.com/JonnyH/libsmacker @ ae8d4c9), decodes the Smacker videos of the original game
# (VIDEO/INTRO.SMK, CREDITS.SMK). Vendored unchanged apart from this file.
add_library(smacker STATIC smacker.c smacker.h smk_malloc.h)
target_include_directories(smacker SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(smacker PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_definitions(smacker PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(smacker PRIVATE /w)
else()
    target_compile_options(smacker PRIVATE -w)
endif()
