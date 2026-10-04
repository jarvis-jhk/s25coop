// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "s25util/colors.h"

/// Shared views need distinct cursors even though their tribe has only one colour.
inline unsigned LocalViewColor(const unsigned view, const unsigned tribeColor, const bool shared)
{
    return shared ? PLAYER_COLORS[view % PLAYER_COLORS.size()] : tribeColor;
}
