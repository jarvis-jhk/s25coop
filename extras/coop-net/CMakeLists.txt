# Copyright (C) 2026 s25coop contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# s25coop network test harness: host and join a game over localhost without video (ROADMAP M2 step 0)
add_executable(coop-net main.cpp)
target_link_libraries(coop-net PRIVATE s25Main Boost::program_options Boost::nowide)

if(WIN32)
    include(GatherDll)
    gather_dll_copy(coop-net)
endif()
