// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "dskFrontEndPage.h"
#include "liblobby/LobbyInterface.h"
#include <optional>

/// Internet login, server selection and public chat share one page; credentials never enter its trail.
class dskFrontEndInternet final : public dskFrontEndPage, public LobbyInterface
{
public:
    dskFrontEndInternet();
    ~dskFrontEndInternet() override;
    Window* GetPadEntryCtrl(unsigned slot) override;
    enum ControlIds
    {
        ID_User = ID_FIRST_FREE,
        ID_Password,
        ID_SavePassword,
        ID_Ipv6,
        ID_Login,
        ID_Register,
        ID_Servers,
        ID_Join,
        ID_Refresh,
        ID_Host,
        ID_Logout,
        ID_Chat,
        ID_Message,
        ID_Send,
        ID_Status,
        ID_Timer,
        ID_UserLabel,
        ID_PasswordLabel
    };
    void LC_LoggedIn(const std::string& email) override;
    void LC_Status_Waiting() override;
    void LC_Status_Error(const std::string& error) override;
    void LC_Status_ConnectionLost() override;
    void LC_Status_IncompleteMessage() override;
    void LC_ServerList(const LobbyServerList& servers) override;
    void LC_Chat(const std::string& player, const std::string& text) override;

private:
    void OnChoose(unsigned id) override;
    void OnLayout() override;
    void Msg_EditChange(unsigned id) override;
    void Msg_EditEnter(unsigned id) override;
    void Msg_Timer(unsigned id) override;
    void Msg_TableSelectItem(unsigned id, const std::optional<unsigned>& selection) override;
    void Msg_TableChooseItem(unsigned id, unsigned selection) override;
    void UpdateMode();
    void UpdateJoin();
    void SetStatus(const std::string& text);
    void Login();
    void Join();
    bool loggedIn_ = false;
    bool waiting_ = false;
    bool layoutUnavailable_ = false;
    std::vector<std::string> endpoints_;
    std::string selected_;
    std::string status_;
    std::string savedPasswordHash_;
};
