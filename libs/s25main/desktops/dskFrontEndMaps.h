// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "dskFrontEndPage.h"
#include "frontend/MapCatalog.h"
#include "frontend/ScenarioCatalog.h"

/// Maps and explicitly registered single-mission campaigns; the existing mission page owns scenario starts.
class dskFrontEndMaps final : public dskFrontEndPage
{
public:
    struct BrowseState
    {
        unsigned category = 0;
        unsigned players = 0;
        boost::filesystem::path map;
        boost::filesystem::path scenario;
    };
    explicit dskFrontEndMaps(std::shared_ptr<BrowseState> state = std::make_shared<BrowseState>());
    static constexpr unsigned scenarioCategory = 10;
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
    boost::filesystem::path GetSelectedScenarioFolder() const;

protected:
    void OnLayout() override;
    void OnChoose(unsigned ctrl_id) override;

private:
    void Refresh(const boost::filesystem::path& preferred);
    void UpdateStatus();
    void UpdateDetails();
    void StartSelected();
    frontend::MapCatalog catalog_;
    frontend::CampaignCatalog scenarios_;
    std::shared_ptr<BrowseState> state_;
    bool previewReady_ = false;
    bool layoutUnavailable_ = false;
};
