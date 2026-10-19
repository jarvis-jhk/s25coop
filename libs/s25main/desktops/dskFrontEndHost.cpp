// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dskFrontEndHost.h"
#include "Loader.h"
#include "Settings.h"
#include "WindowManager.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlCheck.h"
#include "controls/ctrlConnectionProgress.h"
#include "controls/ctrlEdit.h"
#include "controls/ctrlText.h"
#include "desktops/dskFrontEndCampaigns.h"
#include "desktops/dskFrontEndLoad.h"
#include "desktops/dskFrontEndMaps.h"
#include "frontend/CampaignCatalog.h"
#include "frontend/SaveCatalog.h"
#include "input/MenuPadInput.h"
#include "network/CreateServerInfo.h"
#include "network/GameClient.h"
#include "network/GameServer.h"
#include "ogl/glFont.h"
#include "liblobby/LobbyClient.h"
#include "s25util/StringConversion.h"
#include "s25util/colors.h"
#include <array>
#include <exception>

namespace {
const char* proxyError()
{
    return _("You can't create a game while a proxy server is active\nDisable the use of a proxy server first!");
}
} // namespace

dskFrontEndHost::dskFrontEndHost(const ServerType type) : dskFrontEndPage(_("Create Game")), type_(type)
{
    UseTiles(Extent(5, 2), Extent(340, 130));
    AddItem(ID_Campaigns, _("Campaigns"));
    AddItem(ID_Maps, _("Maps & scenarios"));
    AddItem(ID_Saves, _("Load game"));
    Layout();
}

dskFrontEndHost::dskFrontEndHost(const ServerType type, MapDescription map, const std::string& name,
                                 std::optional<CampaignStart> campaign)
    : dskFrontEndPage(_("Create Game")), type_(type), map_(std::move(map)), campaign_(std::move(campaign))
{
    const std::array<std::string, 3> labels{_("Game's Name:"), _("Server-Port:"), _("Password (if needed):")};
    for(unsigned i = 0; i < labels.size(); ++i)
    {
        const unsigned id = ID_Name + i;
        AddText(ID_LabelStart + id, DrawPoint(0, 0), labels[i], COLOR_YELLOW, FontStyle{}, NormalFont);
        auto* edit = AddEdit(id, DrawPoint(0, 0), Extent(1, 1), TextureColor::Green2, NormalFont, 0, id == ID_Password);
        edit->SetControllerKeyboardEnabled();
    }
    GetCtrl<ctrlEdit>(ID_Name)->SetText(name);
    GetCtrl<ctrlEdit>(ID_Port)->SetText(SETTINGS.server.localPort);
    GetCtrl<ctrlEdit>(ID_Port)->SetType(EditType::Number);
    AddCheckBox(ID_Ipv6, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, _("Use IPv6:"), NormalFont)
      ->setChecked(SETTINGS.server.ipv6);
    AddTextButton(ID_Start, DrawPoint(0, 0), Extent(1, 1), TextureColor::Green2, _("Create Game"), NormalFont);
    AddText(ID_Status, DrawPoint(0, 0), "", COLOR_RED, FontStyle{}, NormalFont);
    AddCtrl(std::make_unique<ctrlConnectionProgress>(this, ID_Connection, [this](ClientError error) {
        this->GetCtrl<ctrlButton>(ID_Start)->SetEnabled();
        SetStatus(ClientErrorToStr(error));
        Layout();
        WINDOWMANAGER.GetPadInput().ClearFocus();
    }));
    for(const unsigned id : {ID_Name, ID_Port, ID_Password})
        GetCtrl<ctrlEdit>(id)->SetNotify();
    Layout();
}

Window* dskFrontEndHost::GetPadEntryCtrl(const unsigned slot)
{
    if(auto* connection = GetCtrl<ctrlConnectionProgress>(ID_Connection); connection && connection->IsConnecting())
        return connection->GetCtrl<Window>(ctrlConnectionProgress::ID_Abort);

    return map_ ? GetCtrl<Window>(ID_Name) : dskFrontEndPage::GetPadEntryCtrl(slot);
}

void dskFrontEndHost::SetStatus(const std::string& text)
{
    GetCtrl<ctrlText>(ID_Status)->SetText(text);
}

void dskFrontEndHost::OnLayout()
{
    if(!map_)
        return;
    auto* connection = GetCtrl<ctrlConnectionProgress>(ID_Connection);
    const bool connecting = connection->IsConnecting();
    connection->SetPos(GetFrame().content.getOrigin());
    connection->Resize(GetFrame().content.getSize());
    for(unsigned id = ID_Name; id <= ID_Status; ++id)
        GetCtrl<Window>(id)->SetVisible(!connecting);
    for(unsigned id = ID_Name; id <= ID_Password; ++id)
        GetCtrl<Window>(ID_LabelStart + id)->SetVisible(!connecting);
    if(connecting)
        return;
    const auto& area = GetFrame().content;
    layoutUnavailable_ = area.getSize().x < 480 || area.getSize().y < 250;
    for(unsigned id = ID_Name; id < ID_Status; ++id)
        GetCtrl<Window>(id)->SetVisible(!layoutUnavailable_);
    for(unsigned id = ID_Name; id <= ID_Password; ++id)
        GetCtrl<Window>(ID_LabelStart + id)->SetVisible(!layoutUnavailable_);
    auto* status = GetCtrl<ctrlText>(ID_Status);
    status->SetPos(area.getOrigin());
    status->setMaxWidth(area.getSize().x);
    if(layoutUnavailable_)
    {
        SetStatus(_("Reduce the UI scale or enlarge the window."));
        return;
    }
    const std::array<unsigned, 6> rows{ID_Name, ID_Port, ID_Password, ID_Ipv6, ID_Status, ID_Start};
    const auto rects = frontend::LayoutList(area, rows.size(), 54, 680);
    for(unsigned i = 0; i < rows.size(); ++i)
    {
        auto* control = GetCtrl<Window>(rows[i]);
        const auto& rect = rects[i];
        control->SetPos(rect.getOrigin());
        control->Resize(rect.getSize());
        if(auto* label = GetCtrl<ctrlText>(ID_LabelStart + rows[i]))
        {
            label->SetPos(rect.getOrigin());
            label->setMaxWidth(rect.getSize().x);
            const unsigned height = NormalFont->getHeight() + 4u;
            control->SetPos(rect.getOrigin() + DrawPoint(0, height));
            control->Resize(Extent(rect.getSize().x, rect.getSize().y > height ? rect.getSize().y - height : 1));
        }
    }
}

void dskFrontEndHost::Msg_EditChange(unsigned /*id*/)
{
    SetStatus("");
}

void dskFrontEndHost::Msg_EditEnter(const unsigned id)
{
    if(id == ID_Name || id == ID_Port)
    {
        GetCtrl<ctrlEdit>(id)->SetFocus(false);
        GetCtrl<ctrlEdit>(id + 1)->SetFocus();
    } else if(id == ID_Password)
        OnChoose(ID_Start);
}

void dskFrontEndHost::OnChoose(const unsigned id)
{
    if(layoutUnavailable_ || (map_ && GetCtrl<ctrlConnectionProgress>(ID_Connection)->IsConnecting()))
        return;
    if(id == ID_Start && map_)
    {
        Start();
        return;
    }
    if(map_)
        return;
    if(id == ID_Campaigns)
    {
        Open([type = type_, selection = std::make_shared<boost::filesystem::path>()] {
            return std::make_unique<dskFrontEndCampaigns>(selection, type);
        });
    } else if(id == ID_Maps)
    {
        Open([type = type_, state = std::make_shared<dskFrontEndMaps::BrowseState>()] {
            return std::make_unique<dskFrontEndMaps>(state, type);
        });
    } else if(id == ID_Saves)
        Open([type = type_] { return std::make_unique<dskFrontEndLoad>(boost::filesystem::path{}, type); });
}

void dskFrontEndHost::Start()
{
    if(SETTINGS.proxy.type != ProxyType::None)
    {
        SetStatus(proxyError());
        return;
    }
    if(type_ == ServerType::Lobby && !LOBBYCLIENT.IsLoggedIn())
    {
        SetStatus(_("Lost connection to lobby!"));
        return;
    }
    if(campaign_)
    {
        try
        {
            const auto fresh = frontend::ReadCampaign(campaign_->folder);
            const auto& description = fresh.description;
            if(description.uid != campaign_->uid || campaign_->mission >= description.getNumMaps()
               || description.getMapFilePath(campaign_->mission) != map_->map_path
               || description.getLuaFilePath(campaign_->mission) != map_->lua_path)
            {
                SetStatus(_("Campaign changed. Refresh before choosing a mission."));
                return;
            }
            if(!SETTINGS.campaigns.isChapterPlayable(description, campaign_->mission))
            {
                SetStatus(_("Locked"));
                return;
            }
        } catch(const std::runtime_error& error)
        {
            SetStatus(error.what());
            return;
        }
    }
    const auto name = GetCtrl<ctrlEdit>(ID_Name)->GetText();
    if(name.empty())
    {
        SetStatus(_("Please enter a name for the game"));
        return;
    }
    const auto port = validate::checkPort(GetCtrl<ctrlEdit>(ID_Port)->GetText());
    if(!port)
    {
        SetStatus(_("Invalid port. The valid port-range is 1 to 65535!"));
        return;
    }
    if(map_->map_type == MapType::Savegame && !frontend::ReadSaveEntry(map_->map_path))
    {
        SetStatus(_("The specified file couldn't be loaded!"));
        return;
    }
    const bool ipv6 = GetCtrl<ctrlCheck>(ID_Ipv6)->isChecked();
    const CreateServerInfo info(type_, *port, name, GetCtrl<ctrlEdit>(ID_Password)->GetText(), ipv6,
                                SETTINGS.global.useUPNP);
    bool hosted = false;
    try
    {
        hosted = GAMECLIENT.HostGame(info, *map_);
    } catch(const std::exception&)
    {
        // The file can change again after metadata validation; partially started hosts must not survive a retry.
        GAMECLIENT.Stop();
        GAMESERVER.Stop();
    }
    if(!hosted)
    {
        SetStatus(_("Hosting of game not possible"));
        return;
    }
    SETTINGS.server.ipv6 = ipv6;
    GAMECLIENT.SetHostingCampaign(campaign_.has_value());
    // A campaign has one human tribe and a save is usually one too: whoever joins plays it together with the host.
    if(campaign_ || map_->map_type == MapType::Savegame)
        GAMESERVER.SetAllowCoopMembers(true);
    GetCtrl<ctrlButton>(ID_Start)->SetEnabled(false);
    GetCtrl<ctrlConnectionProgress>(ID_Connection)->Start(type_);
    for(unsigned id = ID_Name; id <= ID_Password; ++id)
        GetCtrl<ctrlEdit>(id)->SetFocus(false);
    Layout();
    WINDOWMANAGER.GetPadInput().ClearFocus();
}

bool dskFrontEndHost::GoBack()
{
    if(auto* connection = GetCtrl<ctrlConnectionProgress>(ID_Connection); connection && connection->IsConnecting())
    {
        connection->Abort();
        return true;
    }
    return dskFrontEndPage::GoBack();
}
