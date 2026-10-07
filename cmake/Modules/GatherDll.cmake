# Copyright (C) 2026 s25coop contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later

include_guard(GLOBAL)

# Keep upstream's DLL discovery, but serialize its per-target post-build writes. Several targets
# share a runtime directory; simultaneous copy_if_different calls can fail on Windows file handles.
include("${CMAKE_CURRENT_LIST_DIR}/../../external/libutil/cmake/GatherDll.cmake")
set_property(GLOBAL PROPERTY RTTR_DLL_COPY_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/../scripts/CopyRuntimeDlls.cmake.in")

function(gather_dll_copy target)
    if(NOT WIN32 OR NOT GATHER_DLLS OR CMAKE_CROSSCOMPILING)
        return()
    endif()
    set(RTTR_DLL_SOURCES "")
    set(missing "")
    foreach(source IN LISTS GATHER_DLLS)
        if(EXISTS "${source}")
            list(APPEND RTTR_DLL_SOURCES "${source}")
        else()
            list(APPEND missing "${source}")
        endif()
    endforeach()
    if(missing)
        message(WARNING "Could not find the following files: ${missing}")
    endif()
    if(NOT RTTR_DLL_SOURCES)
        return()
    endif()
    list(REMOVE_DUPLICATES RTTR_DLL_SOURCES)
    set(RTTR_DLL_DESTINATION "$<TARGET_FILE_DIR:${target}>")
    get_property(template GLOBAL PROPERTY RTTR_DLL_COPY_TEMPLATE)
    file(READ "${template}" script)
    string(CONFIGURE "${script}" script @ONLY)
    set(script_path "${CMAKE_CURRENT_BINARY_DIR}/copy-runtime-dlls-${target}-$<CONFIG>.cmake")
    file(GENERATE OUTPUT "${script_path}" CONTENT "${script}")
    add_custom_command(TARGET ${target} POST_BUILD COMMAND "${CMAKE_COMMAND}" -P "${script_path}" VERBATIM)
endfunction()
