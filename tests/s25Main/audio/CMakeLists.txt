# Copyright (C) 2005 - 2021 Settlers Freaks <sf-team at siedler25.org>
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Tests using the audio system

add_testcase(NAME sounds
    LIBS s25Main audioMockup testHelpers testConfig rttr::vld
)
# The Smacker tests decode the original intro when the original game is there (see tests/coop)
if(RTTR_COOP_S2_DIR)
    set_property(TEST Test_sounds APPEND PROPERTY ENVIRONMENT "RTTR_COOP_S2_DIR=${RTTR_COOP_S2_DIR}")
endif()
