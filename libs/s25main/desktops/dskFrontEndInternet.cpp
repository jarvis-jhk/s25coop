// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dskFrontEndInternet.h"
#include "Loader.h"
#include "RTTR_Version.h"
#include "Settings.h"
#include "WindowManager.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlChat.h"
#include "controls/ctrlCheck.h"
#include "controls/ctrlEdit.h"
#include "controls/ctrlTable.h"
#include "controls/ctrlText.h"
#include "desktops/dskFrontEndConnect.h"
#include "desktops/dskFrontEndHost.h"
#include "desktops/dskLobby.h"
#include "helpers/containerUtils.h"
#include "ingameWindows/iwMsgbox.h"
#include "input/MenuPadInput.h"
#include "mygettext/mygettext.h"
#include "ogl/glFont.h"
#include "liblobby/LobbyClient.h"
#include "s25util/StringConversion.h"
#include "s25util/colors.h"
#include "s25util/md5.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <set>

namespace {
bool storedHash(const std::string& password)
{
    return password.size() == 36 && password.compare(0, 4, "md5:") == 0;
}

std::string endpoint(const LobbyServerInfo& server)
{
    // Include the endpoint: a reused lobby id must not retarget a selected row.
    return std::to_string(server.getId()) + ":" + server.getHost() + ":" + std::to_string(server.getPort());
}
} // namespace

dskFrontEndInternet::dskFrontEndInternet() : dskFrontEndPage(_("Internet Lobby")), loggedIn_(LOBBYCLIENT.IsLoggedIn())
{
    AddText(ID_UserLabel, DrawPoint(0, 0), _("Username:"), COLOR_YELLOW, FontStyle{}, NormalFont);
    AddText(ID_PasswordLabel, DrawPoint(0, 0), _("Password:"), COLOR_YELLOW, FontStyle{}, NormalFont);
    auto* user = AddEdit(ID_User, DrawPoint(0, 0), Extent(1, 1), TextureColor::Green2, NormalFont, 30);
    auto* password = AddEdit(ID_Password, DrawPoint(0, 0), Extent(1, 1), TextureColor::Green2, NormalFont, 0, true);
    user->SetControllerKeyboardEnabled();
    password->SetControllerKeyboardEnabled();
    user->SetText(SETTINGS.lobby.name);
    savedPasswordHash_ = storedHash(SETTINGS.lobby.password) ? SETTINGS.lobby.password.substr(4) : "";
    password->SetText(savedPasswordHash_.empty() ? SETTINGS.lobby.password : savedPasswordHash_);
    AddCheckBox(ID_SavePassword, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, _("Save Password?"), NormalFont)
      ->setChecked(SETTINGS.lobby.savePassword);
    AddCheckBox(ID_Ipv6, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, _("Use IPv6:"), NormalFont)
      ->setChecked(SETTINGS.server.ipv6);
    for(const auto& item : std::array<std::pair<unsigned, std::string>, 7>{{{ID_Login, _("Connect")},
                                                                            {ID_Register, _("Register")},
                                                                            {ID_Join, _("Join Game")},
                                                                            {ID_Refresh, _("Refresh")},
                                                                            {ID_Host, _("Create Game")},
                                                                            {ID_Logout, _("Disconnect")},
                                                                            {ID_Send, _("Send")}}})
    {
        AddTextButton(item.first, DrawPoint(0, 0), Extent(1, 1), TextureColor::Green2, item.second, NormalFont);
    }
    using Sort = ctrlTable::SortType;
    auto* servers = AddTable(ID_Servers, DrawPoint(0, 0), Extent(500, 200), TextureColor::Grey, NormalFont,
                             {{_("Server"), 35, Sort::String},
                              {_("Map"), 35, Sort::String},
                              {_("Player"), 12, Sort::String},
                              {_("Version"), 18, Sort::String}});
    for(unsigned id = 1; id <= servers->GetNumColumns(); ++id)
        servers->GetCtrl<ctrlButton>(id)->SetEnabled(false);
    AddChatCtrl(ID_Chat, DrawPoint(0, 0), Extent(500, 100), TextureColor::Grey, NormalFont);
    AddEdit(ID_Message, DrawPoint(0, 0), Extent(1, 1), TextureColor::Green2, NormalFont, 400)
      ->SetControllerKeyboardEnabled();
    AddText(ID_Status, DrawPoint(0, 0), "", COLOR_YELLOW, FontStyle{}, NormalFont);
    AddTimer(ID_Timer, std::chrono::seconds(5));
    user->SetNotify();
    password->SetNotify();
    LOBBYCLIENT.SetProgramVersion(rttr::version::GetReadableVersion());
    LOBBYCLIENT.AddListener(this);
    if(loggedIn_)
    {
        if(LOBBYCLIENT.IsIngame())
            LOBBYCLIENT.SendLeaveServer();
        LC_ServerList(LOBBYCLIENT.GetServerList());
        LOBBYCLIENT.SendServerListRequest();
    }
    UpdateMode();
}

dskFrontEndInternet::~dskFrontEndInternet()
{
    LOBBYCLIENT.RemoveListener(this);
    if(!LOBBYCLIENT.IsLoggedIn())
        LOBBYCLIENT.Stop();
}

Window* dskFrontEndInternet::GetPadEntryCtrl(unsigned /*slot*/)
{
    return GetCtrl<Window>(loggedIn_ ? (endpoints_.empty() ? ID_Refresh : ID_Servers) : ID_User);
}

void dskFrontEndInternet::SetStatus(const std::string& text)
{
    status_ = text;
    GetCtrl<ctrlText>(ID_Status)->SetText(layoutUnavailable_ ? _("Reduce the UI scale or enlarge the window.") :
                                                               status_);
}

void dskFrontEndInternet::UpdateMode()
{
    Layout();
    WINDOWMANAGER.GetPadInput().ClearFocus();
}

void dskFrontEndInternet::OnLayout()
{
    const auto& area = GetFrame().content;
    layoutUnavailable_ = area.getSize().x < 480 || area.getSize().y < 250;
    for(unsigned id = ID_User; id <= ID_Send; ++id)
        GetCtrl<Window>(id)->SetVisible(!layoutUnavailable_ && ((id <= ID_Register) != loggedIn_));
    for(const unsigned id : {ID_UserLabel, ID_PasswordLabel})
        GetCtrl<Window>(id)->SetVisible(!layoutUnavailable_ && !loggedIn_);
    auto* status = GetCtrl<ctrlText>(ID_Status);
    status->SetPos(area.getOrigin());
    status->setMaxWidth(area.getSize().x);
    SetStatus(status_);
    if(layoutUnavailable_)
        return;
    auto place = [this](unsigned id, const Rect& rect) {
        auto* control = this->GetCtrl<Window>(id);
        control->SetPos(rect.getOrigin());
        control->Resize(rect.getSize());
    };
    if(!loggedIn_)
    {
        constexpr std::array<unsigned, 7> rows{ID_User,  ID_Password, ID_SavePassword, ID_Ipv6,
                                               ID_Login, ID_Register, ID_Status};
        const auto rects = frontend::LayoutList(area, rows.size(), 54, 680);
        for(unsigned i = 0; i < rows.size(); ++i)
            place(rows[i], rects[i]);
        for(const unsigned id : {ID_User, ID_Password})
        {
            auto* label = GetCtrl<ctrlText>(id == ID_User ? ID_UserLabel : ID_PasswordLabel);
            const auto rect = GetCtrl<Window>(id)->GetBoundaryRect();
            label->SetPos(rect.getOrigin());
            label->setMaxWidth(rect.getSize().x);
            const unsigned height = NormalFont->getHeight() + 4u;
            place(id, Rect(rect.getOrigin() + DrawPoint(0, height),
                           Extent(rect.getSize().x, rect.getSize().y > height ? rect.getSize().y - height : 1)));
        }
    } else
    {
        const unsigned gap = frontend::itemGap;
        const unsigned tableHeight = (area.getSize().y - 156) * 2 / 3;
        place(ID_Servers, Rect(area.getOrigin(), Extent(area.getSize().x, tableHeight)));
        const unsigned width = (area.getSize().x - 3 * gap) / 4;
        for(unsigned i = 0; i < 4; ++i)
            place(ID_Join + i,
                  Rect(area.getOrigin() + DrawPoint(i * (width + gap), tableHeight + gap), Extent(width, 32)));
        const unsigned chatY = tableHeight + gap + 40;
        const unsigned chatHeight = area.getSize().y - chatY - 68;
        place(ID_Chat, Rect(area.getOrigin() + DrawPoint(0, chatY), Extent(area.getSize().x, chatHeight)));
        place(ID_Message,
              Rect(area.getOrigin() + DrawPoint(0, chatY + chatHeight + 4), Extent(area.getSize().x - 108, 28)));
        place(ID_Send,
              Rect(area.getOrigin() + DrawPoint(area.getSize().x - 100, chatY + chatHeight + 4), Extent(100, 28)));
        status->SetPos(area.getOrigin() + DrawPoint(0, area.getSize().y - 24));
    }
    GetCtrl<ctrlButton>(ID_Login)->SetEnabled(!waiting_);
    UpdateJoin();
}

void dskFrontEndInternet::Msg_EditChange(unsigned /*id*/)
{
    SetStatus("");
}

void dskFrontEndInternet::Msg_EditEnter(const unsigned id)
{
    if(id == ID_User)
    {
        GetCtrl<ctrlEdit>(ID_User)->SetFocus(false);
        GetCtrl<ctrlEdit>(ID_Password)->SetFocus();
    } else if(id == ID_Password)
        OnChoose(ID_Login);
    else if(id == ID_Message)
        OnChoose(ID_Send);
}

void dskFrontEndInternet::Msg_Timer(unsigned /*id*/)
{
    if(loggedIn_ && LOBBYCLIENT.IsLoggedIn() && !LOBBYCLIENT.IsIngame())
        LOBBYCLIENT.SendServerListRequest();
}

void dskFrontEndInternet::Login()
{
    if(waiting_)
        return;
    const auto user = GetCtrl<ctrlEdit>(ID_User)->GetText();
    auto password = GetCtrl<ctrlEdit>(ID_Password)->GetText();
    if(user.empty() || password.empty())
    {
        SetStatus(_("Username and password are required."));
        return;
    }
    if(savedPasswordHash_.empty() || password != savedPasswordHash_)
        password = s25util::md5(password).toString();
    SETTINGS.lobby.name = user;
    SETTINGS.lobby.savePassword = GetCtrl<ctrlCheck>(ID_SavePassword)->isChecked();
    SETTINGS.lobby.password = SETTINGS.lobby.savePassword ? "md5:" + password : "";
    SETTINGS.server.ipv6 = GetCtrl<ctrlCheck>(ID_Ipv6)->isChecked();
    waiting_ = true;
    GetCtrl<ctrlButton>(ID_Login)->SetEnabled(false);
    SetStatus(_("Connecting with Host..."));
    const auto port = validate::checkPort(LOADER.GetTextN("client", 1));
    if(!port || !LOBBYCLIENT.Login(LOADER.GetTextN("client", 0), *port, user, password, SETTINGS.server.ipv6))
        LC_Status_Error(_("Connection failed!"));
}

void dskFrontEndInternet::LC_LoggedIn(const std::string& /*email*/)
{
    loggedIn_ = true;
    waiting_ = false;
    GetCtrl<ctrlEdit>(ID_Password)->SetText("");
    endpoints_.clear();
    selected_.clear();
    GetCtrl<ctrlTable>(ID_Servers)->DeleteAllItems();
    SetStatus("");
    UpdateMode();
    LOBBYCLIENT.SendServerListRequest();
}

void dskFrontEndInternet::LC_Status_Waiting()
{
    SetStatus(_("Waiting for Reply..."));
}

void dskFrontEndInternet::LC_Status_Error(const std::string& error)
{
    waiting_ = false;
    GetCtrl<ctrlButton>(ID_Login)->SetEnabled();
    SetStatus(error);
}

void dskFrontEndInternet::LC_Status_ConnectionLost()
{
    loggedIn_ = false;
    waiting_ = false;
    endpoints_.clear();
    selected_.clear();
    GetCtrl<ctrlTable>(ID_Servers)->DeleteAllItems();
    UpdateMode();
    SetStatus(_("Lost connection to lobby!"));
}

void dskFrontEndInternet::LC_Status_IncompleteMessage()
{
    LC_Status_ConnectionLost();
}

void dskFrontEndInternet::LC_ServerList(const LobbyServerList& servers)
{
    if(!loggedIn_)
        return;
    const auto preferred = selected_;
    endpoints_.clear();
    auto* table = GetCtrl<ctrlTable>(ID_Servers);
    table->DeleteAllItems();
    std::optional<unsigned> chosen;
    std::set<unsigned> ids;
    for(const auto& server : servers)
    {
        if(server.getName().empty() || server.getHost().empty() || !server.getPort()
           || !ids.insert(server.getId()).second)
            continue;
        if(endpoint(server) == preferred)
            chosen = table->GetNumRows();
        endpoints_.push_back(endpoint(server));
        table->AddRow({(server.hasPassword() ? "(pwd) " : "") + server.getName(), server.getMap(),
                       std::to_string(server.getCurPlayers()) + "/" + std::to_string(server.getMaxPlayers()),
                       server.getVersion()});
    }
    if(preferred.empty() && !endpoints_.empty())
        chosen = 0;
    table->SetSelection(chosen);
    selected_ = preferred.empty() && chosen ? endpoints_[*chosen] : preferred;
    UpdateJoin();
}

void dskFrontEndInternet::UpdateJoin()
{
    const auto selected = GetCtrl<ctrlTable>(ID_Servers)->GetSelection();
    bool compatible = false;
    if(selected && *selected < endpoints_.size())
    {
        const auto& servers = LOBBYCLIENT.GetServerList();
        const auto found = helpers::find_if(
          servers, [this, selected](const auto& server) { return endpoint(server) == endpoints_[*selected]; });
        compatible = found != servers.end() && isServerVersionValid(*found);
    }
    GetCtrl<ctrlButton>(ID_Join)->SetEnabled(loggedIn_ && compatible && !layoutUnavailable_);
}

void dskFrontEndInternet::Msg_TableSelectItem(const unsigned id, const std::optional<unsigned>& selection)
{
    if(id == ID_Servers && selection && *selection < endpoints_.size())
        selected_ = endpoints_[*selection];
    UpdateJoin();
}

void dskFrontEndInternet::Msg_TableChooseItem(const unsigned id, unsigned /*selection*/)
{
    if(id == ID_Servers)
        OnChoose(ID_Join);
}

void dskFrontEndInternet::Join()
{
    if(!LOBBYCLIENT.IsLoggedIn())
    {
        LC_Status_ConnectionLost();
        return;
    }
    const auto& servers = LOBBYCLIENT.GetServerList();
    const auto found = helpers::find_if(servers, [this](const auto& server) { return endpoint(server) == selected_; });
    if(found == servers.end())
    {
        SetStatus(_("The selected game was not found anymore."));
        return;
    }
    if(!isServerVersionValid(*found))
    {
        SetStatus(_("You can't join that game with your version!"));
        return;
    }
    auto state = std::make_shared<dskFrontEndConnect::JoinState>();
    state->host = found->getHost();
    state->port = std::to_string(found->getPort());
    state->ipv6 = SETTINGS.server.ipv6;
    state->serverType = ServerType::Lobby;
    Open([state] { return std::make_unique<dskFrontEndConnect>(state); });
}

void dskFrontEndInternet::LC_Chat(const std::string& player, const std::string& text)
{
    if(!loggedIn_)
        return;
    if(player == "LobbyBot" && !text.empty() && text.front() == ' ')
    {
        const auto prefix = " " + LOBBYCLIENT.GetUser();
        if(text.compare(0, prefix.size(), prefix) != 0)
            return;
        if(text.compare(prefix.size(), 2, ": ") == 0)
            SetStatus(text.substr(prefix.size() + 2));
        else if(text.compare(prefix.size(), 2, ", ") == 0)
            GetCtrl<ctrlChat>(ID_Chat)->AddMessage("", player, ctrlChat::CalcUniqueColor(player),
                                                   text.substr(prefix.size() + 2), COLOR_YELLOW);
        return;
    }
    GetCtrl<ctrlChat>(ID_Chat)->AddMessage("", player, ctrlChat::CalcUniqueColor(player), text, COLOR_YELLOW);
}

void dskFrontEndInternet::OnChoose(const unsigned id)
{
    if(layoutUnavailable_)
        return;
    if(id == ID_Login && !loggedIn_)
        Login();
    else if(id == ID_Register && !loggedIn_)
        SetStatus(_("Create an account at https://forum.siedler25.org."));
    else if(loggedIn_)
    {
        if(id == ID_Join)
            Join();
        else if(id == ID_Refresh)
            Msg_Timer(ID_Timer);
        else if(id == ID_Logout)
        {
            LOBBYCLIENT.Stop();
            LC_Status_ConnectionLost();
            SetStatus("");
        } else if(id == ID_Send && LOBBYCLIENT.IsLoggedIn())
        {
            auto* edit = GetCtrl<ctrlEdit>(ID_Message);
            if(!edit->GetText().empty())
                LOBBYCLIENT.SendChat(edit->GetText());
            edit->SetText("");
        } else if(id == ID_Host)
        {
            if(SETTINGS.proxy.type != ProxyType::None)
                SetStatus(_(
                  "You can't create a game while a proxy server is active\nDisable the use of a proxy server first!"));
            else
                Open([] { return std::make_unique<dskFrontEndHost>(ServerType::Lobby); });
        }
    }
}
