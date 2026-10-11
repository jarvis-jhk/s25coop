// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "dskFrontEndPage.h"
#include "frontend/MapCatalog.h"

/// F5b: map browsing stays independent of lobby seating and scenario/campaign registration.
class dskFrontEndMaps final : public dskFrontEndPage
{
public:
    dskFrontEndMaps();
    enum ControlIds
    {
        ID_Category = ID_FIRST_FREE,
        ID_PlayerFilter,
        ID_Maps,
        ID_Continue,
        ID_Refresh,
        ID_CategoryLabel,
        ID_PlayerLabel,
        ID_Status,
        ID_Preview,
        ID_MapName,
        ID_Author,
        ID_Info,
        ID_Script,
        ID_MapState,
    };

    Window* GetPadEntryCtrl(unsigned slot) override;
    bool Msg_PadCommand(unsigned slot, PadButton button) override;
    bool Msg_KeyDown(const KeyEvent& ke) override;
    void Msg_ComboSelectItem(unsigned ctrl_id, unsigned selection) override;
    void Msg_TableSelectItem(unsigned ctrl_id, const std::optional<unsigned>& selection) override;
    void Msg_TableChooseItem(unsigned ctrl_id, unsigned selection) override;
    const frontend::MapCatalog& GetCatalog() const { return catalog_; }
    boost::filesystem::path GetSelectedPath() const;

protected:
    void OnLayout() override;
    void OnChoose(unsigned ctrl_id) override;

private:
    void Refresh(const boost::filesystem::path& preferred);
    void UpdateStatus();
    void UpdateDetails();
    void StartSelected();
    frontend::MapCatalog catalog_;
    bool previewReady_ = false;
    bool layoutUnavailable_ = false;
};
