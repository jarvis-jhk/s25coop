// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "frontend/MenuRoutes.h"
#include "desktops/dskHome.h"
#include "desktops/dskMainMenu.h"
#include "desktops/dskSinglePlayer.h"

namespace frontend {

namespace {
    // Classic until a home page was shown: a start that skips the splash (tests, direct map start) keeps
    // upstream's flow.
    MenuStyle style = MenuStyle::Classic;
} // namespace

MenuStyle GetMenuStyle()
{
    return style;
}

void SetMenuStyle(const MenuStyle newStyle)
{
    style = newStyle;
}

std::unique_ptr<Desktop> MainMenu()
{
    if(style == MenuStyle::FrontEnd)
        return dskHome::Create();
    return std::make_unique<dskMainMenu>();
}

std::unique_ptr<Desktop> SinglePlayerMenu()
{
    if(style == MenuStyle::FrontEnd)
        return dskHome::Create();
    return std::make_unique<dskSinglePlayer>();
}

} // namespace frontend
