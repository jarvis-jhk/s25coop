// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

class Desktop;

/// Where the classic desktops return to (doc/coop/FrontEnd.md, F3).
///
/// The new home page opens upstream's desktops for everything it does not rebuild yet (campaign list, map
/// selection, options, online play). Their own back buttons and the end of a game used to hard-code
/// dskMainMenu or dskSinglePlayer, which dropped a player who started from Home into the old menus. They
/// now ask here instead: whoever showed the last main menu decides. dskHome sets FrontEnd, dskMainMenu
/// (reachable as "Classic menus" until F12) sets Classic, so each menu style keeps its players.
namespace frontend {

enum class MenuStyle
{
    Classic,
    FrontEnd
};

MenuStyle GetMenuStyle();
void SetMenuStyle(MenuStyle style);

/// "Back to the main menu": the home page or dskMainMenu.
std::unique_ptr<Desktop> MainMenu();
/// "Back to single player": the home page (which has those entries itself) or dskSinglePlayer.
std::unique_ptr<Desktop> SinglePlayerMenu();

} // namespace frontend
