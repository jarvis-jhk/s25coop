// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Rect.h"
#include "world/ViewportLayout.h"
#include <array>
#include <optional>
#include <vector>

struct ControllerPanelLayout
{
    bool compact;
    Rect panel;
    Rect tabs;
    Rect content;
    Rect footer;
    /// Compact mode overlays the full world; side mode reserves the panel and its gap.
    Rect world;
};

/// The viewport is already in GUI view units, exactly like CalcViewports' result.
/// Never apply GUI scaling again here. Even transient zero/tiny resize cells stay bounded.
ControllerPanelLayout CalcControllerPanelLayout(const Viewport& viewport);

/// One instance per view, even when multiple views control the same player.
/// This is navigation bookkeeping only: no windows, game pointers, commands or input routing.
/// The shell must resolve modal/dirty-state policy before accepting Open/Back/tab transitions.
class ControllerPanelNavigation
{
public:
    enum class Tab
    {
        Buildings,
        Stock,
        Statistics,
        Post,
        Settings
    };
    enum class LayerKind
    {
        Detail,
        Dropdown,
        Edit,
        Confirmation
    };
    struct Layer
    {
        LayerKind kind;
        unsigned id;
    };
    enum class TabResult
    {
        Inactive,
        BlockedByChild,
        Changed
    };
    enum class BackResult
    {
        Inactive,
        CancelInput,
        ReturnFromDetail,
        Close
    };
    struct Page
    {
        std::vector<unsigned> rows;
        std::optional<unsigned> selectedId;
        unsigned firstVisibleRow = 0;
        unsigned visibleRows = 1;
    };

    void Open() { open_ = true; }
    bool IsOpen() const { return open_; }
    Tab GetTab() const { return tab_; }
    TabResult CycleTab(bool forward);
    /// Only accepted while open; ids describe page-local destinations, never object pointers.
    bool PushLayer(LayerKind kind, unsigned id);
    const std::vector<Layer>& GetLayers() const { return layers_; }
    BackResult PeekBack() const;
    /// Call only after the caller accepted cancellation/close (including any discard decision).
    BackResult Back();
    /// Release local capture on disconnect; remember tab and row state for this game session.
    void Disconnect();

    const Page& GetPage(Tab tab) const;
    /// Refresh/reflow with unique stable row ids. Preserve identity through reorder/replacement,
    /// clamp scrolling, and reveal the selected row. Empty pages have no selection.
    /// Layer ids are page-local routes, not necessarily row ids. The shell must reconcile
    /// vanished child targets through its cancellation policy; refresh never commits an edit.
    void UpdateRows(Tab tab, std::vector<unsigned> rows, unsigned visibleRows);
    bool SelectRow(Tab tab, unsigned id);

private:
    static constexpr unsigned NumTabs = 5;
    static void RevealSelection(Page& page);
    bool open_ = false;
    Tab tab_ = Tab::Buildings;
    std::array<Page, NumTabs> pages_;
    std::vector<Layer> layers_;
};
