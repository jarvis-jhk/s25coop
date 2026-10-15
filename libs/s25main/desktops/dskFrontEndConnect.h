// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "dskFrontEndPage.h"
#include "gameTypes/ServerType.h"

/// The join form and live handshake share the full-screen frame; retries retain private inputs.
class dskFrontEndConnect : public dskFrontEndPage
{
public:
    struct JoinState
    {
        JoinState();
        std::string host;
        std::string port;
        bool ipv6;
        bool coop = false;
        ServerType serverType = ServerType::Direct;
    };
    explicit dskFrontEndConnect(std::shared_ptr<JoinState> state = std::make_shared<JoinState>());
    Window* GetPadEntryCtrl(unsigned slot) override;
    bool GoBack() override;
    enum ControlIds
    {
        ID_Host = ID_FIRST_FREE,
        ID_Port,
        ID_Password,
        ID_Ipv6,
        ID_Coop,
        ID_Connect,
        ID_Status,
        ID_Connection,
        ID_LabelStart = 100
    };

private:
    void OnChoose(unsigned id) override;
    void OnLayout() override;
    void Msg_EditChange(unsigned id) override;
    void Msg_EditEnter(unsigned id) override;
    void Msg_CheckboxChange(unsigned id, bool checked) override;
    void SetStatus(const std::string& text, unsigned color);
    std::shared_ptr<JoinState> state_;
};
