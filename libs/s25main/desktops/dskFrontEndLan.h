// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "dskFrontEndPage.h"
#include "frontend/LanServers.h"

class dskFrontEndLan : public dskFrontEndPage
{
public:
    struct BrowseState
    {
        std::string selected;
    };
    explicit dskFrontEndLan(std::shared_ptr<BrowseState> state = std::make_shared<BrowseState>());
    Window* GetPadEntryCtrl(unsigned slot) override;
    enum ControlIds
    {
        ID_Servers = ID_FIRST_FREE,
        ID_Join,
        ID_Refresh,
        ID_Host,
        ID_Status,
        ID_Timer
    };

private:
    void OnChoose(unsigned id) override;
    void OnLayout() override;
    void Msg_PaintBefore() override;
    void Msg_Timer(unsigned id) override;
    void Msg_TableSelectItem(unsigned id, const std::optional<unsigned>& selection) override;
    void Msg_TableChooseItem(unsigned id, unsigned selection) override;
    void UpdateServers(bool force = false);
    void UpdateStatus();
    frontend::LanDiscovery discovery_;
    LANDiscoveryClient::ServiceMap lastServices_;
    std::vector<frontend::LanServer> servers_;
    std::shared_ptr<BrowseState> state_;
    bool listening_ = false;
    bool layoutUnavailable_ = false;
};
