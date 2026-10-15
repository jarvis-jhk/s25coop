// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "dskFrontEndPage.h"
#include "frontend/CampaignCatalog.h"
#include "gameTypes/ServerType.h"
#include <boost/signals2/connection.hpp>
#include <optional>

struct KeyEvent;

class dskFrontEndMissions : public dskFrontEndPage
{
public:
    explicit dskFrontEndMissions(boost::filesystem::path folder, ServerType type = ServerType::Local);
    boost::filesystem::path GetSelectedPath() const;
    bool Msg_KeyDown(const KeyEvent& ke) override;
    enum ControlIds
    {
        ID_Missions = ID_FIRST_FREE,
        ID_Start,
        ID_Refresh,
        ID_Description,
        ID_Preview,
        ID_Info,
        ID_State
    };

protected:
    void OnChoose(unsigned id) override;
    void OnLayout() override;
    void Msg_TableSelectItem(unsigned id, const std::optional<unsigned>& selection) override;
    void Msg_TableChooseItem(unsigned id, unsigned selection) override;

private:
    ServerType type_;
    void Refresh();
    void UpdateDetails();
    void StartSelected();
    std::optional<frontend::CampaignEntry> campaign_;
    boost::filesystem::path folder_;
    std::string failure_;
    bool previewReady_ = false;
    bool layoutUnavailable_ = false;
    boost::signals2::scoped_connection onErrorConnection_;
};
