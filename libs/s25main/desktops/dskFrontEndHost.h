// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "dskFrontEndPage.h"
#include "gameTypes/MapDescription.h"
#include "gameTypes/ServerType.h"

/// Choose content first, then enter hosting credentials. Passwords never enter the page trail.
class dskFrontEndHost final : public dskFrontEndPage
{
public:
    struct CampaignStart
    {
        boost::filesystem::path folder;
        std::string uid;
        unsigned mission;
    };
    explicit dskFrontEndHost(ServerType type);
    dskFrontEndHost(ServerType type, MapDescription map, const std::string& name,
                    std::optional<CampaignStart> campaign = std::nullopt);
    Window* GetPadEntryCtrl(unsigned slot) override;
    bool GoBack() override;
    enum ControlIds
    {
        ID_Campaigns = ID_FIRST_FREE,
        ID_Maps,
        ID_Saves,
        ID_Name,
        ID_Port,
        ID_Password,
        ID_Ipv6,
        ID_Start,
        ID_Status,
        ID_Connection,
        ID_LabelStart = 100
    };

private:
    void OnChoose(unsigned id) override;
    void OnLayout() override;
    void Msg_EditEnter(unsigned id) override;
    void Msg_EditChange(unsigned id) override;
    void SetStatus(const std::string& text);
    void Start();
    ServerType type_;
    std::optional<MapDescription> map_;
    std::optional<CampaignStart> campaign_;

    bool layoutUnavailable_ = false;
};
