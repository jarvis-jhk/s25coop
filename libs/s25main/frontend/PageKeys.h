// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "input/PlayerBrief.h"
#include <vector>

class FocusPath;

namespace frontend {

/// The footer help line of a front-end page, for the controller that owns the page.
///
/// Asks the same questions the button press decides on (Window::CanActivate, FocusPath::PeekStep),
/// exactly like the in-game brief does for windows, so the line cannot promise what a press does not do.
/// `canGoBack`: B leads somewhere (dskFrontEndPage::CanGoBack).
std::vector<brief::KeyHint> PageKeys(const FocusPath& focus, bool canGoBack);

} // namespace frontend
