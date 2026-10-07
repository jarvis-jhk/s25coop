// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "coop/Changelog.h"
#include "desktops/dskFrontEndPage.h"
#include <boost/filesystem/path.hpp>
#include <memory>
#include <vector>

/// The front end's home page (doc/coop/FrontEnd.md, F3): the first screen after the splash.
///
/// Every entry is a tile. Entries whose new page does not exist yet open upstream's desktop for it, and
/// those come back here (frontend::MainMenu). "Classic menus" leads to the old main menu until F12.
class dskHome : public dskFrontEndPage
{
public:
    dskHome();
    /// A home page that knows its own factory, so pages opened from it can come back.
    static std::unique_ptr<Desktop> Create();

    void SetActive(bool activate = true) override;
    void Msg_Timer(unsigned ctrl_id) override;
    void Msg_MsgBoxResult(unsigned msgbox_id, MsgboxResult mbr) override;

    enum ControlIds
    {
        ID_Continue = dskFrontEndPage::ID_FIRST_FREE,
        ID_Campaigns,
        ID_Maps,
        ID_Load,
        ID_Online,
        ID_Options,
        ID_WhatsNew,
        ID_Credits,
        ID_Classic,
        ID_Quit,
        ID_tmrDebugData
    };

    /// The tile chosen last, focused again when an upstream desktop opened from here returns.
    static unsigned GetLastChoice() { return lastChoice_; }
    static void ForgetLastChoice() { lastChoice_ = 0; }

    /// B on Home leads to the title page, where more controllers can join.
    bool HasBackAction() const override { return true; }

protected:
    void OnChoose(unsigned ctrl_id) override;
    bool OnBackAtRoot() override;

private:
    std::vector<coop::changelog::Section> pendingChangelog_;
    /// The newest save at construction: enables the tile and is what it resumes (one header scan per visit).
    boost::filesystem::path newestSave_;
    static unsigned lastChoice_;
};
