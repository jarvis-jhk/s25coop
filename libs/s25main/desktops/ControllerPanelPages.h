// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "input/ControllerPanelModel.h"
#include <algorithm>
#include <string>
#include <vector>

class GamePlayer;
class ITexture;

/// What the controller side panel shows. Pure reads of the player's actual state: no windows,
/// no commands. The shell (dskGameInterface) owns input, the navigation model owns selection.
namespace panel {

using Tab = ControllerPanelNavigation::Tab;

struct Row
{
    /// Stable within its tab: a ware/job/building enum value, never a pointer.
    unsigned id;
    ITexture* icon;
    std::string name;
    std::string value;
    unsigned valueColor;
};

/// Ids of the people rows on the Stock page start here so wares and jobs never collide.
constexpr unsigned JobRowBase = 1000;

/// Wares then people in the classic stock window's order, without the addon-disabled ones.
std::vector<Row> StockRows(const GamePlayer& player);
/// Every building type in use: "built (+sites)" and the average productivity where there is one.
std::vector<Row> BuildingRows(const GamePlayer& player);
/// Tabs without a page of their own yet open their classic window; this is the line saying so.
std::string LegacyText(Tab tab);
std::string TabName(Tab tab);
bool HasOwnPage(Tab tab);

constexpr unsigned RowHeight = 28;
constexpr unsigned RowGap = 4;
inline unsigned VisibleRows(const ControllerPanelLayout& layout)
{
    return std::max(1u, (layout.content.getSize().y + RowGap) / (RowHeight + RowGap));
}

/// seatColor marks the selection, so four panels on one TV can be told apart.
void Draw(const ControllerPanelLayout& layout, const ControllerPanelNavigation& nav, const std::vector<Row>& rows,
          unsigned seatColor);

} // namespace panel
