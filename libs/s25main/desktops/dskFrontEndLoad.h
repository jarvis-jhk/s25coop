// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "dskFrontEndPage.h"
#include "frontend/SaveCatalog.h"
#include <boost/filesystem/path.hpp>

/// F6a: metadata save browser; hosting/loading remains in GameClient and the existing lobby.
class dskFrontEndLoad final : public dskFrontEndPage
{
public:
    /// A Home Continue target is selected by identity, never by a row position.
    explicit dskFrontEndLoad(const boost::filesystem::path& preferred = {});

    enum ControlIds
    {
        ID_Saves = ID_FIRST_FREE,
        ID_Load,
        ID_Refresh,
        ID_Status,
        ID_Map,
        ID_Date,
        ID_GameTime,
        ID_Players,
        ID_Names,
        ID_Preview,
        ID_Coop,
    };

    void SetActive(bool activate = true) override;
    Window* GetPadEntryCtrl(unsigned slot) override;
    bool Msg_KeyDown(const KeyEvent& ke) override;
    void Msg_TableSelectItem(unsigned ctrl_id, const std::optional<unsigned>& selection) override;
    void Msg_TableChooseItem(unsigned ctrl_id, unsigned selection) override;
    const frontend::SaveCatalog& GetCatalog() const { return catalog_; }
    boost::filesystem::path GetSelectedPath() const;

protected:
    void OnLayout() override;
    void OnChoose(unsigned ctrl_id) override;

private:
    void Refresh(const boost::filesystem::path& preferred);
    void UpdateDetails();
    void LoadSelected();
    frontend::SaveCatalog catalog_;
    boost::filesystem::path resumeOnce_;
};
