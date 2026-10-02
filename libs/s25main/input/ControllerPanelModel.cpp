// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "input/ControllerPanelModel.h"
#include <algorithm>
#include <cstdint>
#include <utility>

ControllerPanelLayout CalcControllerPanelLayout(const Viewport& viewport)
{
    const unsigned w = viewport.size.x;
    const unsigned h = viewport.size.y;
    const unsigned insetX = std::min(8u, w / 2);
    const unsigned insetY = std::min(8u, h / 2);
    const bool compact = w < 800 || h < 480;
    unsigned width;
    if(compact)
        width = std::min(320u, w - 2 * insetX);
    else if(w < 960)
        width = 320;
    else
        width = std::clamp(static_cast<unsigned>(static_cast<uint64_t>(w) * 35 / 100), 336u, 480u);

    const Rect panel(viewport.origin + Position(static_cast<int>(w - insetX - width), static_cast<int>(insetY)),
                     Extent(width, h - 2 * insetY));
    const unsigned marginX = std::min(8u, width / 2);
    const unsigned marginY = std::min(8u, panel.getSize().y / 2);
    const unsigned innerWidth = width - 2 * marginX;
    const unsigned innerHeight = panel.getSize().y - 2 * marginY;
    const unsigned tabHeight = std::min(32u, innerHeight);
    const unsigned footerHeight = std::min(32u, innerHeight - tabHeight);
    const Position start = panel.getOrigin() + Position(static_cast<int>(marginX), static_cast<int>(marginY));
    const Rect tabs(start, Extent(innerWidth, tabHeight));
    const Rect content(Position(start.x, tabs.bottom), Extent(innerWidth, innerHeight - tabHeight - footerHeight));
    const Rect footer(Position(start.x, content.bottom), Extent(innerWidth, footerHeight));
    const Rect world(viewport.origin, compact ? viewport.size : Extent(w - insetX - width - 8, h));
    return {compact, panel, tabs, content, footer, world};
}

ControllerPanelNavigation::TabResult ControllerPanelNavigation::CycleTab(const bool forward)
{
    if(!open_)
        return TabResult::Inactive;
    // Child input must consume shoulders, including detail pages with navigation history.
    if(!layers_.empty())
        return TabResult::BlockedByChild;
    const unsigned index = static_cast<unsigned>(tab_);
    tab_ = static_cast<Tab>((index + (forward ? 1 : NumTabs - 1)) % NumTabs);
    return TabResult::Changed;
}

bool ControllerPanelNavigation::PushLayer(const LayerKind kind, const unsigned id)
{
    if(!open_)
        return false;
    layers_.push_back({kind, id});
    return true;
}

ControllerPanelNavigation::BackResult ControllerPanelNavigation::PeekBack() const
{
    if(!open_)
        return BackResult::Inactive;
    if(layers_.empty())
        return BackResult::Close;
    return layers_.back().kind == LayerKind::Detail ? BackResult::ReturnFromDetail : BackResult::CancelInput;
}

ControllerPanelNavigation::BackResult ControllerPanelNavigation::Back()
{
    const BackResult result = PeekBack();
    if(result == BackResult::Close)
        open_ = false;
    else if(result != BackResult::Inactive)
        layers_.pop_back();
    return result;
}

void ControllerPanelNavigation::Disconnect()
{
    layers_.clear();
    open_ = false;
}

const ControllerPanelNavigation::Page& ControllerPanelNavigation::GetPage(const Tab tab) const
{
    return pages_[static_cast<unsigned>(tab)];
}

void ControllerPanelNavigation::UpdateRows(const Tab tab, std::vector<unsigned> rows, const unsigned visibleRows)
{
    Page& page = pages_[static_cast<unsigned>(tab)];
    page.rows = std::move(rows);
    page.visibleRows = std::max(1u, visibleRows);
    if(page.rows.empty())
        page.selectedId.reset();
    else if(!page.selectedId || std::find(page.rows.begin(), page.rows.end(), *page.selectedId) == page.rows.end())
        page.selectedId = page.rows.front();
    RevealSelection(page);
}

bool ControllerPanelNavigation::SelectRow(const Tab tab, const unsigned id)
{
    Page& page = pages_[static_cast<unsigned>(tab)];
    if(std::find(page.rows.begin(), page.rows.end(), id) == page.rows.end())
        return false;
    page.selectedId = id;
    RevealSelection(page);
    return true;
}

void ControllerPanelNavigation::RevealSelection(Page& page)
{
    const unsigned count = static_cast<unsigned>(page.rows.size());
    page.firstVisibleRow = std::min(page.firstVisibleRow, count > page.visibleRows ? count - page.visibleRows : 0);
    if(!page.selectedId)
        return;
    const unsigned index =
      static_cast<unsigned>(std::find(page.rows.begin(), page.rows.end(), *page.selectedId) - page.rows.begin());
    if(index < page.firstVisibleRow)
        page.firstVisibleRow = index;
    else if(index - page.firstVisibleRow >= page.visibleRows)
        page.firstVisibleRow = index - page.visibleRows + 1;
}
