// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dskFrontEndConnect.h"
#include "Loader.h"
#include "Settings.h"
#include "WindowManager.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlCheck.h"
#include "controls/ctrlConnectionProgress.h"
#include "controls/ctrlEdit.h"
#include "controls/ctrlText.h"
#include "input/MenuPadInput.h"
#include "network/CoopMemberInfo.h"
#include "network/GameClient.h"
#include "ogl/glFont.h"
#include "s25util/StringConversion.h"
#include "s25util/colors.h"
#include <array>

dskFrontEndConnect::JoinState::JoinState()
    : host(SETTINGS.server.lastIP), port(std::to_string(SETTINGS.server.localPort)), ipv6(SETTINGS.server.ipv6)
{}

dskFrontEndConnect::dskFrontEndConnect(std::shared_ptr<JoinState> state)
    : dskFrontEndPage(_("Join Game")), state_(std::move(state))
{
    const std::array<std::string, 3> labels{_("IP Address of Host:"), _("Server-Port:"), _("Password (if needed):")};
    for(unsigned i = 0; i < labels.size(); ++i)
    {
        const unsigned id = ID_Host + i;
        AddText(ID_LabelStart + id, DrawPoint(0, 0), labels[i], COLOR_YELLOW, FontStyle{}, NormalFont);
        auto* edit = AddEdit(id, DrawPoint(0, 0), Extent(1, 1), TextureColor::Green2, NormalFont, 0, id == ID_Password,
                             false, false);
        edit->SetControllerKeyboardEnabled();
    }
    GetCtrl<ctrlEdit>(ID_Host)->SetText(state_->host);
    GetCtrl<ctrlEdit>(ID_Port)->SetText(state_->port);
    GetCtrl<ctrlEdit>(ID_Port)->SetType(EditType::Number);
    AddCheckBox(ID_Ipv6, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, _("Use IPv6:"), NormalFont)
      ->setChecked(state_->ipv6);
    AddCheckBox(ID_Coop, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, _("Play the host's tribe together"),
                NormalFont)
      ->setChecked(state_->coop);
    GetCtrl<ctrlCheck>(ID_Coop)->SetTooltip(_("Join as a co-player of the host instead of taking a slot of your own. "
                                              "The host must allow co-players."));
    AddTextButton(ID_Connect, DrawPoint(0, 0), Extent(1, 1), TextureColor::Green2, _("Connect"), NormalFont);
    AddText(ID_Status, DrawPoint(0, 0), "", COLOR_RED, FontStyle{}, NormalFont);
    AddCtrl(std::make_unique<ctrlConnectionProgress>(this, ID_Connection, [this](ClientError error) {
        this->GetCtrl<ctrlButton>(ID_Connect)->SetEnabled();
        SetStatus(ClientErrorToStr(error), COLOR_RED);
        Layout();
        WINDOWMANAGER.GetPadInput().ClearFocus();
    }));
    // Initial SetText calls must not notify before the status control exists.
    for(const unsigned id : {ID_Host, ID_Port, ID_Password})
        GetCtrl<ctrlEdit>(id)->SetNotify();
    Layout();
}

Window* dskFrontEndConnect::GetPadEntryCtrl(unsigned /*slot*/)
{
    if(auto* connection = GetCtrl<ctrlConnectionProgress>(ID_Connection); connection && connection->IsConnecting())
        return connection->GetCtrl<Window>(ctrlConnectionProgress::ID_Abort);

    return GetCtrl<ctrlEdit>(ID_Host);
}

void dskFrontEndConnect::OnLayout()
{
    auto* connection = GetCtrl<ctrlConnectionProgress>(ID_Connection);
    const bool connecting = connection->IsConnecting();
    connection->SetPos(GetFrame().content.getOrigin());
    connection->Resize(GetFrame().content.getSize());
    for(unsigned id = ID_Host; id <= ID_Status; ++id)
        GetCtrl<Window>(id)->SetVisible(!connecting);
    for(unsigned id = ID_Host; id <= ID_Password; ++id)
        GetCtrl<Window>(ID_LabelStart + id)->SetVisible(!connecting);
    if(connecting)
        return;
    const std::array<unsigned, 7> rows{ID_Host, ID_Port, ID_Password, ID_Ipv6, ID_Coop, ID_Status, ID_Connect};
    const auto rects = frontend::LayoutList(GetFrame().content, rows.size(), 54, 680);
    for(unsigned i = 0; i < rows.size(); ++i)
    {
        auto* control = GetCtrl<Window>(rows[i]);
        const auto& rect = rects[i];
        control->SetPos(rect.getOrigin());
        control->Resize(rect.getSize());
        if(auto* label = GetCtrl<ctrlText>(ID_LabelStart + rows[i]))
        {
            label->SetPos(rect.getOrigin());
            label->setMaxWidth(static_cast<unsigned short>(rect.getSize().x));
            const auto height = NormalFont->getHeight() + 4u;
            control->SetPos(rect.getOrigin() + DrawPoint(0, height));
            control->Resize(Extent(rect.getSize().x, rect.getSize().y > height ? rect.getSize().y - height : 1));
        }
    }
    GetCtrl<ctrlText>(ID_Status)->setMaxWidth(static_cast<unsigned short>(rects[5].getSize().x));
}

void dskFrontEndConnect::SetStatus(const std::string& text, const unsigned color)
{
    auto* status = GetCtrl<ctrlText>(ID_Status);
    status->SetText(text);
    status->SetTextColor(color);
}

void dskFrontEndConnect::Msg_EditChange(const unsigned id)
{
    if(id == ID_Host)
        state_->host = GetCtrl<ctrlEdit>(id)->GetText();
    else if(id == ID_Port)
        state_->port = GetCtrl<ctrlEdit>(id)->GetText();
    SetStatus("", COLOR_RED);
}

void dskFrontEndConnect::Msg_EditEnter(const unsigned id)
{
    if(id == ID_Host || id == ID_Port)
    {
        GetCtrl<ctrlEdit>(id)->SetFocus(false);
        GetCtrl<ctrlEdit>(id + 1)->SetFocus();
    } else if(id == ID_Password)
        OnChoose(ID_Connect);
}

void dskFrontEndConnect::Msg_CheckboxChange(const unsigned id, const bool checked)
{
    if(id == ID_Ipv6)
        state_->ipv6 = checked;
    else if(id == ID_Coop)
        state_->coop = checked;
}

void dskFrontEndConnect::OnChoose(const unsigned id)
{
    if(id != ID_Connect || GetCtrl<ctrlConnectionProgress>(ID_Connection)->IsConnecting())
        return;
    const auto port = validate::checkPort(GetCtrl<ctrlEdit>(ID_Port)->GetText());
    if(!port)
    {
        SetStatus(_("Invalid port. The valid port-range is 1 to 65535!"), COLOR_RED);
        return;
    }
    const std::string host = GetCtrl<ctrlEdit>(ID_Host)->GetText();
    if(!GAMECLIENT.Connect(host, GetCtrl<ctrlEdit>(ID_Password)->GetText(), state_->serverType, *port, false,
                           state_->ipv6, state_->coop ? COOP_LEADER_HOST : 0xFF))
    {
        SetStatus(_("Connection failed!"), COLOR_RED);
        return;
    }
    SETTINGS.server.lastIP = host;
    SETTINGS.server.ipv6 = state_->ipv6;
    GetCtrl<ctrlButton>(ID_Connect)->SetEnabled(false);
    GetCtrl<ctrlConnectionProgress>(ID_Connection)->Start(state_->serverType);
    for(unsigned id = ID_Host; id <= ID_Password; ++id)
        GetCtrl<ctrlEdit>(id)->SetFocus(false);
    Layout();
    WINDOWMANAGER.GetPadInput().ClearFocus();
}

bool dskFrontEndConnect::GoBack()
{
    if(auto* connection = GetCtrl<ctrlConnectionProgress>(ID_Connection); connection && connection->IsConnecting())
    {
        connection->Abort();
        return true;
    }
    return dskFrontEndPage::GoBack();
}
