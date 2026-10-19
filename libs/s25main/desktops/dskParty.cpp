// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dskParty.h"
#include "GameLobby.h"
#include "GameLobbyController.h"
#include "GlobalGameSettings.h"
#include "JoinPlayerInfo.h"
#include "Loader.h"
#include "WindowManager.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlChat.h"
#include "controls/ctrlEdit.h"
#include "controls/ctrlLobbyPlayerCard.h"
#include "controls/ctrlText.h"
#include "controls/ctrlTextButton.h"
#include "coop/CoopLobby.h"
#include "desktops/dskGameLoader.h"
#include "driver/KeyEvent.h"
#include "frontend/MenuRoutes.h"
#include "helpers/MaxEnumValue.h"
#include "helpers/containerUtils.h"
#include "helpers/format.hpp"
#include "ingameWindows/iwAddons.h"
#include "ingameWindows/iwMsgbox.h"
#include "input/LocalViewColor.h"
#include "input/MenuPadInput.h"
#include "input/PadRouter.h"
#include "lua/LuaInterfaceSettings.h"
#include "network/ClientError.h"
#include "network/GameClient.h"
#include "ogl/FontStyle.h"
#include "gameTypes/GameSettingTypes.h"
#include "gameTypes/ServerType.h"
#include "gameData/const_gui_ids.h"
#include "s25util/Log.h"
#include "s25util/MyTime.h"
#include "s25util/Socket.h"
#include <algorithm>
#include <array>
#include <mygettext/mygettext.h>
#include <optional>
#ifndef _WIN32
#    include <arpa/inet.h>
#    include <unistd.h>
#endif

namespace {
enum
{
    ID_mbLeave = 1,
    ID_mbError,
    ID_mbInfo,
    ID_mbEconomy,
    ID_mbPeaceful,
    ID_mbRemove,
};

/// One step through the values of an enum, wrapping at both ends.
template<class T>
T stepEnum(const T value, const bool forward, const unsigned count)
{
    const auto index = static_cast<unsigned>(value);
    return static_cast<T>(forward ? (index + 1) % count : (index + count - 1) % count);
}

std::string goalName(const GameObjective goal)
{
    switch(goal)
    {
        case GameObjective::None: return _("None");
        case GameObjective::Conquer3_4: return _("Conquer 3/4 of map");
        case GameObjective::TotalDomination: return _("Total domination");
        case GameObjective::EconomyMode: return _("Economy mode");
        default: return _("Tournament"); // only offered in an online lobby game
    }
}

std::string goodsName(const StartWares wares)
{
    switch(wares)
    {
        case StartWares::VLow: return _("Very Low");
        case StartWares::Low: return _("Low");
        case StartWares::Normal: return _("Normal");
        case StartWares::ALot: return _("A lot");
        case StartWares::Minimal: return _("Minimal");
        case StartWares::MinimalPlus: return _("Minimal+");
    }
    return "";
}

std::string explorationName(const Exploration exploration)
{
    switch(exploration)
    {
        case Exploration::Disabled: return _("Off (all visible)");
        case Exploration::Classic: return _("Classic (Settlers 2)");
        case Exploration::FogOfWar: return _("Fog of War");
        case Exploration::FogOfWarExplored: return _("FoW - all explored");
    }
    return "";
}

std::string speedName(const GameSpeed speed)
{
    switch(speed)
    {
        case GameSpeed::VerySlow: return _("Very slow");
        case GameSpeed::Slow: return _("Slow");
        case GameSpeed::Normal: return _("Normal");
        case GameSpeed::Fast: return _("Fast");
        case GameSpeed::VeryFast: return _("Very fast");
    }
    return "";
}

std::string opponentsName(const dskParty::Opponents opponents)
{
    switch(opponents)
    {
        case dskParty::Opponents::Easy: return _("Computer (easy)");
        case dskParty::Opponents::Medium: return _("Computer (medium)");
        case dskParty::Opponents::Hard: return _("Computer (hard)");
        case dskParty::Opponents::None: return _("None (closed)");
    }
    return "";
}

const char* onOff(const bool on)
{
    return on ? _("On") : _("Off");
}

/// The IPv4 address other computers on this network reach this one at: the one of the interface the default
/// route leaves through. Connecting a UDP socket only picks the route, no packet is sent. Empty when offline.
std::string localNetworkAddress()
{
    const SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock == INVALID_SOCKET)
        return "";
    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(9);
    inet_pton(AF_INET, "192.0.2.1", &target.sin_addr); // TEST-NET-1: routed like the internet, never answered
    std::string result;
    sockaddr_in own{};
    socklen_t length = sizeof(own);
    if(connect(sock, reinterpret_cast<const sockaddr*>(&target), sizeof(target)) == 0
       && getsockname(sock, reinterpret_cast<sockaddr*>(&own), &length) == 0 && own.sin_addr.s_addr != 0)
    {
        std::array<char, INET_ADDRSTRLEN> text{};
        if(inet_ntop(AF_INET, &own.sin_addr, text.data(), text.size()))
            result = text.data();
    }
    closesocket(sock);
    return result;
}
} // namespace

bool dskParty::IsFor(const ServerType serverType)
{
    const auto lobby = GAMECLIENT.GetGameLobby();
    if(frontend::GetMenuStyle() != frontend::MenuStyle::FrontEnd || !lobby || GAMECLIENT.IsAIBattleModeOn())
        return false;
    if(serverType == ServerType::Local)
        return lobby->isHost();
    // Network games (F7d, F7e): every host and every joined player, co-players of somebody else's tribe included.
    return true;
}

dskParty::dskParty(std::shared_ptr<GameLobby> gameLobby, const unsigned playerId, const ServerType serverType,
                   std::unique_ptr<ILobbyClient> lobbyClient)
    : dskFrontEndPage(_("Party")), gameLobby_(std::move(gameLobby)), localPlayerId_(playerId), serverType_(serverType),
      lobbyClient_(std::move(lobbyClient)), online_(serverType != ServerType::Local), host_(gameLobby_->isHost()),
      ordinary_(!gameLobby_->isSavegame() && !GAMECLIENT.IsHostingCampaign())
{
    lobbyController_ = std::make_unique<GameLobbyController>(gameLobby_, GAMECLIENT.GetMainPlayer());
    // The mission script sets up the slots exactly as in the classic lobby: init now, "ready" on activation.
    if(!GAMECLIENT.GetLuaFilePath().empty())
    {
        lua_ = std::make_unique<LuaInterfaceSettings>(*lobbyController_, GAMECLIENT);
        std::string error;
        if(!lua_->loadScript(GAMECLIENT.GetLuaFilePath()))
            error = _("Lua script was found but failed to load. Map might not work as expected!");
        else if(!lua_->CheckScriptVersion())
            error = _("Lua script uses a different version and cannot be used. Map might not work as expected!");
        else if(!lua_->EventSettingsInit(!online_, gameLobby_->isSavegame()))
        {
            LOG.write(_("Lua was disabled by the script itself\n"));
            lua_.reset();
        }
        if(!error.empty())
        {
            WINDOWMANAGER.ShowAfterSwitch(std::make_unique<iwMsgbox>(_("Error"), error, this, MsgboxButton::Ok,
                                                                     MsgboxIcon::ExclamationRed, ID_mbInfo));
            lua_.reset();
        }
        if(!lua_ && host_)
            lobbyController_->RemoveLuaScript();
    }
    if(gameLobby_->isSavegame() && host_)
    {
        // The saved tribes stay as they were, but every one of them needs somebody to play it: the party shares
        // the host's tribe and nobody can take another seat here. A tribe a further local player played in its
        // own slot (an idle AI since saving) and one a network player left (a free slot, which the server would
        // refuse to start with) get a real AI, so they keep playing instead of standing still.
        const auto formerLocal = frontend::LocalSeats::FormerLocalPlayers(*gameLobby_, localPlayerId_);
        // Online a past human's tribe stays open for whoever joins; one nobody took gets its AI at Start.
        for(unsigned i = 0; i < gameLobby_->getNumPlayers(); ++i)
        {
            if((!online_ && gameLobby_->getPlayer(i).ps == PlayerState::Free) || helpers::contains(formerLocal, i))
                lobbyController_->SetPlayerState(i, PlayerState::AI, AI::Info(AI::Type::Default, AI::Level::Easy));
        }
    } else if(!online_) // a network game keeps its open slots for the players who join
    {
        for(unsigned i = 0; i < gameLobby_->getNumPlayers(); ++i)
        {
            if(!gameLobby_->getPlayer(i).isHost)
                lobbyController_->SetPlayerState(i, PlayerState::AI, AI::Info(AI::Type::Default, AI::Level::Easy));
        }
    }

    rules_ = gameLobby_->getSettings();
    AddText(ID_Map, DrawPoint(0, 0), GAMECLIENT.GetMapTitle(), COLOR_YELLOW, FontStyle::CENTER, LargeFont);
    AddText(ID_Mode, DrawPoint(0, 0), "", COLOR_YELLOW, FontStyle::CENTER, NormalFont);
    AddText(ID_Hint, DrawPoint(0, 0), _("More players: press A on a free seat."), COLOR_YELLOW, FontStyle::CENTER,
            NormalFont)
      ->SetVisible(!online_);
    // Own tribes where the map has room for more than the host; the joined party may still need one tribe
    // together (SeatJoinedParty). Online every player has a tribe of their own, and there are no local seats.
    together_ = !online_ && !CanOwnTribes();
    if(online_)
        CreateNetCards();
    else
        CreateSeats();
    UpdateModeText();
    AddTextButton(ID_Start, DrawPoint(0, 0), Extent(1, 1), TextureColor::Green2, host_ ? _("Start game") : _("Ready"),
                  LargeFont);
    if(ordinary_)
    {
        AddTextButton(ID_Rules, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, _("Rules"), LargeFont);
        constexpr std::array<const char*, 10> labels = {
          gettext_noop("Tribes:"),      gettext_noop("Goals:"),           gettext_noop("Goods at start:"),
          gettext_noop("Exploration:"), gettext_noop("Speed:"),           gettext_noop("Computer players:"),
          gettext_noop("Lock teams:"),  gettext_noop("Shared team view"), gettext_noop("Random start locations"),
          gettext_noop("Addons:")};
        for(unsigned i = 0; i < labels.size(); ++i)
        {
            AddText(ID_RuleLabel + i, DrawPoint(0, 0), _(labels[i]), COLOR_YELLOW, FontStyle::VCENTER, NormalFont)
              ->SetVisible(false);
            AddTextButton(ID_Rule + i, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, "", NormalFont)
              ->SetVisible(false);
        }
        AddTextButton(ID_RulesDone, DrawPoint(0, 0), Extent(1, 1), TextureColor::Green2, _("Done"), NormalFont)
          ->SetVisible(false);
        UpdateRules();
    }
    if(online_)
    {
        AddText(ID_Address, DrawPoint(0, 0), JoinText(), COLOR_WHITE, FontStyle::CENTER, NormalFont);
        AddChatCtrl(ID_Chat, DrawPoint(0, 0), Extent(100, 60), TextureColor::Grey, NormalFont);
        AddEdit(ID_ChatEdit, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, NormalFont, 200)
          ->SetControllerKeyboardEnabled();
        AddTextButton(ID_ChatSend, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, _("Send"), NormalFont);
        AddTextButton(ID_Coop, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, "", NormalFont,
                      host_ ? _("Let others join a tribe and play it together with its player") :
                              _("Give up your own slot and play the chosen player's tribe together with them"))
          ->SetVisible(false);
        // The Internet lobby lists the game only while its session knows we are in it.
        if(serverType_ == ServerType::Lobby && lobbyClient_ && lobbyClient_->IsLoggedIn())
            lobbyClient_->SendServerJoinRequest();
        UpdateCoop();
    }
    SetEntryFocus(ID_Start);
    if(GAMECLIENT.IsCoopMember())
        BecomeCoPlayer();
    Layout();
    GAMECLIENT.SetInterface(this);
}

dskParty::~dskParty()
{
    GAMECLIENT.RemoveInterface(this);
}

bool dskParty::CanOwnTribes() const
{
    return ordinary_ && !online_ && gameLobby_->getNumPlayers() >= 2;
}

void dskParty::CreateSeats()
{
    for(unsigned id = ID_Seat; id <= ID_SeatLast; ++id)
        DeleteCtrl(id);
    seats_.Build(*gameLobby_, *lobbyController_, WINDOWMANAGER.GetPadInput().GetRouter(), localPlayerId_, together_);
    for(unsigned i = 0; i < seats_.size(); ++i)
    {
        AddCtrl(std::make_unique<ctrlLobbyPlayerCard>(this, ID_Seat + i, DrawPoint(0, 0), Extent(1, 1), NormalFont,
                                                      LobbyPlayerCardModel(i, CardSnapshot(i))))
          ->SetVisible(!rulesOpen_);
    }
    UpdateCards();
}

void dskParty::SetActive(const bool activate)
{
    dskFrontEndPage::SetActive(activate);
    if(!activate || activated_)
        return;
    activated_ = true;
    if(lua_ && host_)
    {
        try
        {
            lua_->EventSettingsReady();
        } catch(const LuaExecutionError&)
        {
            WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(
              _("Error"), _("Lua script was found but failed to load. Map might not work as expected!"), this,
              MsgboxButton::Ok, MsgboxIcon::ExclamationRed, ID_mbInfo));
            lua_.reset();
        }
    }
    SeatJoinedParty();
}

void dskParty::SeatJoinedParty()
{
    auto& input = WINDOWMANAGER.GetPadInput();
    auto& router = input.GetRouter();
    std::vector<PadDeviceId> members;
    for(const auto device : input.GetParty().Members())
    {
        if(router.HasDevice(device))
            members.push_back(device);
    }
    if(members.empty())
        return;
    // More joined players than the map has tribes for: they play one tribe together instead of leaving some out.
    // Online there are no local seats yet (one controller per computer); the first member is the host.
    if(!online_ && !together_ && seats_.PartyNeedsMoreSeats(members) && members.size() > 1u)
    {
        seats_.StandAll(); // a --local-players seat gets its slot's computer player back
        together_ = true;
        CreateSeats();
        UpdateModeText();
        UpdateRules();
        Layout();
    }
    if(seats_.empty())
    {
        router.AssignSlot(members.front(), 0);
        return;
    }
    if(seats_.PartyNeedsMoreSeats(members))
    {
        WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(
          _("Not enough seats"), helpers::format(_("At most %1% players can play on one screen."), seats_.size()), this,
          MsgboxButton::Ok, MsgboxIcon::ExclamationRed, ID_mbInfo));
        members.resize(seats_.size());
    }
    // Home has a single navigation slot; the party needs one per seat before the guests are bound.
    router.SetNumSlots(GetNumPadSlots());
    seats_.SeatParty(members);
    UpdateCards();
}

LobbyPlayerCardModel::Snapshot dskParty::CardSnapshot(const unsigned seat) const
{
    const auto& player = gameLobby_->getPlayer(seats_[seat].playerId);
    LobbyPlayerCardModel::Snapshot snapshot{{player.color, player.nation, player.team, together_}, {}, {}};
    for(unsigned id = 0; id < gameLobby_->getNumPlayers(); ++id)
    {
        const auto& other = gameLobby_->getPlayer(id);
        if(id != seats_[seat].playerId && other.isUsed())
            snapshot.takenColors.push_back(other.color);
    }
    std::string reason;
    if(!seats_[seat].taken)
        reason = _("Press A to join");
    else if(gameLobby_->isSavegame())
        reason = _("Saved game");
    else if(together_ && seat != 0)
        reason = _("Shared tribe: host chooses");
    else if(seat != 0 && (player.ps != PlayerState::AI || player.aiInfo.type != AI::Type::Dummy))
        reason = _("Seat unavailable"); // own tribe: until the server confirms the seat's slot
    constexpr std::array ownKeys = {"ownColor", "ownNation", "ownTeam"};
    constexpr std::array aiKeys = {"aiColor", "aiNation", "aiTeam"};
    for(unsigned row = 0; row < ownKeys.size(); ++row)
    {
        snapshot.lockReasons[row] = reason;
        if(reason.empty() && lua_ && !lua_->IsChangeAllowed(seat == 0 ? ownKeys[row] : aiKeys[row], true))
            snapshot.lockReasons[row] = _("Fixed by map script");
    }
    // Changing the seat mode stands everybody up, so only the host chooses it for the group.
    auto& modeLock = snapshot.lockReasons[static_cast<unsigned>(LobbyPlayerCardModel::Row::SharedTribe)];
    if(!CanOwnTribes())
        modeLock = _("One tribe required");
    else if(seat != 0)
        modeLock = _("Host chooses seat mode");
    return snapshot;
}

void dskParty::UpdateCards()
{
    const bool sharedCursors = together_ && seats_.NumTaken() > 1;
    unsigned viewIndex = 0;
    for(unsigned i = 0; i < seats_.size(); ++i)
    {
        auto* card = GetCtrl<ctrlLobbyPlayerCard>(ID_Seat + i);
        const auto& seat = seats_[i];
        card->GetModel().UpdateSnapshot(CardSnapshot(i));
        card->SetJoined(seat.taken);
        card->SetCursorColor(sharedCursors && seat.taken ? std::optional<unsigned>(
                               LocalViewColor(viewIndex, card->GetModel().GetSnapshot().values.color, true)) :
                                                           std::nullopt);
        if(seat.taken)
            ++viewIndex;
        std::string text;
        if(i == 0)
            text = helpers::format(_("Player %1%: host"), i + 1);
        else if(!seat.taken)
            text = helpers::format(_("Player %1%: press A to join"), i + 1);
        else
            text = helpers::format(_("Player %1%"), i + 1);
        if(card->GetText() != text)
            card->SetText(text);
    }
}

void dskParty::CreateNetCards()
{
    for(unsigned i = 0; i < gameLobby_->getNumPlayers() && i < MAX_PLAYERS; ++i)
    {
        AddCtrl(std::make_unique<ctrlLobbyPlayerCard>(this, ID_Net + i, DrawPoint(0, 0), Extent(1, 1), NormalFont,
                                                      LobbyPlayerCardModel(i, NetCardSnapshot(i)),
                                                      i != localPlayerId_ || GAMECLIENT.IsCoopMember()));
    }
    UpdateNetCards();
}

LobbyPlayerCardModel::Snapshot dskParty::NetCardSnapshot(const unsigned playerId) const
{
    const auto& player = gameLobby_->getPlayer(playerId);
    LobbyPlayerCardModel::Snapshot snapshot{{player.color, player.nation, player.team, false}, {}, {}};
    for(unsigned id = 0; id < gameLobby_->getNumPlayers(); ++id)
    {
        const auto& other = gameLobby_->getPlayer(id);
        if(id != playerId && other.isUsed())
            snapshot.takenColors.push_back(other.color);
    }
    std::string reason;
    if(playerId != localPlayerId_)
    {
        if(player.ps != PlayerState::Occupied)
            reason = _("Host chooses");
        else
            reason = host_ ? _("A: remove this player") : _("Chosen by that player");
    } else if(GAMECLIENT.IsCoopMember())
        reason = _("Chosen by that player"); // a co-player plays the tribe its player set up
    else if(gameLobby_->isSavegame())
        reason = _("Saved game"); // the server would accept a change the saved world does not have
    constexpr std::array ownKeys = {"ownColor", "ownNation", "ownTeam"};
    for(unsigned row = 0; row < ownKeys.size(); ++row)
    {
        snapshot.lockReasons[row] = reason;
        if(reason.empty() && lua_ && !lua_->IsChangeAllowed(ownKeys[row], true))
            snapshot.lockReasons[row] = _("Fixed by map script");
    }
    snapshot.lockReasons[static_cast<unsigned>(LobbyPlayerCardModel::Row::SharedTribe)] =
      ordinary_ ? _("Online: a tribe for each player") : _("One tribe required");
    return snapshot;
}

std::string dskParty::NetCardText(const unsigned playerId) const
{
    const auto& player = gameLobby_->getPlayer(playerId);
    switch(player.ps)
    {
        case PlayerState::Free: return _("Open: waiting for a player");
        case PlayerState::Locked: return _("Closed");
        case PlayerState::AI: return helpers::format(_("Computer: %1%"), player.name);
        case PlayerState::Occupied: break;
    }
    // Co-players share the tribe of the player they joined; they are named on that card.
    std::string coPlayers;
    for(const CoopMemberInfo& member : GAMECLIENT.GetCoopMembers())
    {
        if(member.leader == playerId)
            coPlayers += " + " + member.name;
    }
    if(playerId == localPlayerId_ && !GAMECLIENT.IsCoopMember())
        return helpers::format(_("%1% (you)"), player.name) + coPlayers;
    // Network players: whether they are ready is what the host waits for; the ping says how well they are connected.
    return helpers::format(_("%1%: %2%, %3% ms"), player.name, player.isReady ? _("ready") : _("not ready"),
                           player.ping)
           + coPlayers;
}

void dskParty::UpdateNetCards()
{
    if(!online_)
        return;
    for(unsigned i = 0; i < gameLobby_->getNumPlayers() && i < MAX_PLAYERS; ++i)
    {
        auto* card = GetCtrl<ctrlLobbyPlayerCard>(ID_Net + i);
        card->GetModel().UpdateSnapshot(NetCardSnapshot(i));
        card->SetJoined(gameLobby_->getPlayer(i).isUsed());
        card->SetActivatable(host_ && i != localPlayerId_ && gameLobby_->getPlayer(i).ps == PlayerState::Occupied);
        const std::string text = NetCardText(i);
        if(card->GetText() != text)
            card->SetText(text);
    }
}

std::string dskParty::JoinText() const
{
    if(!host_)
    {
        for(unsigned id = 0; id < gameLobby_->getNumPlayers(); ++id)
        {
            if(gameLobby_->getPlayer(id).isHost)
                return helpers::format(_("Hosted by %1%"), gameLobby_->getPlayer(id).name);
        }
        return "";
    }
    const unsigned port = GAMECLIENT.GetServerPort();
    const std::string address = localNetworkAddress();
    if(serverType_ == ServerType::Lobby)
        return helpers::format(_("Others join through the Internet lobby: \"%1%\""), GAMECLIENT.GetGameName());
    if(address.empty())
        return helpers::format(_("Others join with this computer's address, port %1%"), port);
    if(serverType_ == ServerType::LAN)
        return helpers::format(_("Others find this game under LAN, or join with %1%, port %2%"), address, port);
    // Over the internet they need the router's public address and a forwarded port; at home this one works.
    return helpers::format(_("Others join with %1%, port %2% (from the internet: your public address)"), address, port);
}

std::string dskParty::NotReady() const
{
    std::string names;
    for(unsigned id = 0; id < gameLobby_->getNumPlayers(); ++id)
    {
        const auto& player = gameLobby_->getPlayer(id);
        if(id != localPlayerId_ && player.ps == PlayerState::Occupied && !player.isReady)
            names += (names.empty() ? "" : ", ") + player.name;
    }
    return names;
}

bool dskParty::HandleNetCardInput(const PadButton button)
{
    auto* card = GetCtrl<ctrlLobbyPlayerCard>(ID_Net + localPlayerId_);
    auto& focus = WINDOWMANAGER.GetPadInput().GetFocus(0);
    if(!card || focus.GetFocused() != card || GAMECLIENT.IsCoopMember())
        return false;
    auto& model = card->GetModel();
    switch(button)
    {
        case PadButton::DpadUp:
            if(model.GetRow() == LobbyPlayerCardModel::Row::Color)
                return false;
            model.MoveRow(false);
            return true;
        case PadButton::DpadDown:
            // The mode row is fixed online; down from the team leaves the card.
            if(model.GetRow() == LobbyPlayerCardModel::Row::Team)
                return false;
            model.MoveRow(true);
            return true;
        case PadButton::DpadLeft:
        case PadButton::DpadRight:
        {
            if(countdown_)
                return true;
            model.UpdateSnapshot(NetCardSnapshot(localPlayerId_));
            if(const auto proposal = model.ProposeValue(button == PadButton::DpadRight))
                ApplyValue(model, localPlayerId_, *proposal);
            return true;
        }
        default: return false;
    }
}

void dskParty::SendChat()
{
    auto* edit = GetCtrl<ctrlEdit>(ID_ChatEdit);
    if(!edit->GetText().empty())
        GAMECLIENT.Command_Chat(edit->GetText(), ChatDestination::All);
    edit->SetText("");
}

void dskParty::Msg_EditEnter(const unsigned ctrl_id)
{
    if(ctrl_id == ID_ChatEdit)
        SendChat();
}

void dskParty::OnLayout()
{
    const auto area = GetFrame().content;
    const int width = static_cast<int>(area.getSize().x);
    const int centerX = area.left + width / 2;
    GetCtrl<ctrlText>(ID_Map)->SetPos(DrawPoint(centerX, area.top));
    GetCtrl<ctrlText>(ID_Mode)->SetPos(DrawPoint(centerX, area.top + 30));
    GetCtrl<ctrlText>(ID_Hint)->SetPos(DrawPoint(centerX, area.bottom - 70));
    // Online the cards start below the join line, and the chat takes the room above the Start row.
    const int cardsTop = area.top + (online_ ? 74 : 56);
    auto* start = GetCtrl<Window>(ID_Start);
    const unsigned startWidth = std::min(320u, area.getSize().x);
    start->Resize(Extent(startWidth, 40));
    start->SetPos(DrawPoint(centerX - static_cast<int>(startWidth / 2), area.bottom - 44));
    if(ordinary_)
    {
        // Rules sits left of Start; Start keeps the centre, where B, A and the mouse expect it.
        const unsigned rulesWidth = std::min(200u, area.getSize().x / 4);
        auto* rules = GetCtrl<Window>(ID_Rules);
        rules->Resize(Extent(rulesWidth, 40));
        rules->SetPos(DrawPoint(start->GetPos().x - static_cast<int>(rulesWidth) - 16, area.bottom - 44));
        // The drawer: one row per rule below the texts, label left and value right, Done last.
        const unsigned numRules = ID_RuleLast - ID_Rule + 1;
        const Rect drawer(DrawPoint(area.left, cardsTop),
                          Extent(area.getSize().x, std::max(0, area.bottom - cardsTop)));
        const auto rows = frontend::LayoutList(drawer, numRules + 1, 40, 760);
        for(unsigned i = 0; i < numRules; ++i)
        {
            const auto& row = rows[i];
            const unsigned half = row.getSize().x / 2;
            auto* label = GetCtrl<ctrlText>(ID_RuleLabel + i);
            label->SetPos(DrawPoint(row.left, row.top + static_cast<int>(row.getSize().y / 2)));
            label->setMaxWidth(half);
            auto* value = GetCtrl<Window>(ID_Rule + i);
            value->SetPos(DrawPoint(row.left + static_cast<int>(half), row.top));
            value->Resize(Extent(row.getSize().x - half, row.getSize().y));
        }
        auto* done = GetCtrl<Window>(ID_RulesDone);
        done->SetPos(rows.back().getOrigin());
        done->Resize(rows.back().getSize());
    }
    if(online_)
    {
        GetCtrl<ctrlText>(ID_Address)->SetPos(DrawPoint(centerX, area.top + 50));
        GetCtrl<ctrlText>(ID_Address)->setMaxWidth(area.getSize().x);
        const int sendWidth = 100;
        const int editTop = area.bottom - 44 - 8 - 28;
        const int chatHeight = std::clamp(static_cast<int>(area.getSize().y) / 4, 60, 160);
        const int chatTop = editTop - 4 - chatHeight;
        auto* chat = GetCtrl<Window>(ID_Chat);
        chat->SetPos(DrawPoint(area.left, chatTop));
        chat->Resize(Extent(area.getSize().x, chatHeight));
        auto* edit = GetCtrl<Window>(ID_ChatEdit);
        edit->SetPos(DrawPoint(area.left, editTop));
        edit->Resize(Extent(std::max(1, width - sendWidth - 8), 28));
        if(auto* coop = GetCtrl<Window>(ID_Coop))
        {
            // Right of Start, mirroring Rules on its left.
            const unsigned coopWidth = std::min(260u, area.getSize().x / 4);
            coop->Resize(Extent(coopWidth, 40));
            coop->SetPos(
              DrawPoint(centerX + static_cast<int>(std::min(320u, area.getSize().x) / 2) + 16, area.bottom - 44));
        }
        auto* send = GetCtrl<Window>(ID_ChatSend);
        send->SetPos(DrawPoint(area.right - sendWidth, editTop));
        send->Resize(Extent(sendWidth, 28));
        // A card needs about 110 pixels for its rows; eight tribes go four across.
        const unsigned numCards = std::min(gameLobby_->getNumPlayers(), MAX_PLAYERS);
        const Rect cardArea(DrawPoint(area.left, cardsTop),
                            Extent(area.getSize().x, std::max(0, chatTop - 8 - cardsTop)));
        const auto grid = frontend::LayoutTiles(cardArea, numCards, Extent(5, 2), Extent(300, 120));
        for(unsigned i = 0; i < numCards; ++i)
        {
            auto* card = GetCtrl<Window>(ID_Net + i);
            card->SetPos(grid.tiles[i].getOrigin());
            card->Resize(grid.tiles[i].getSize());
        }
        return;
    }
    if(seats_.empty())
        return;
    // Cards fill the room between the texts and the Start row: four across when wide, else two by two.
    const Rect cardArea(DrawPoint(area.left, cardsTop),
                        Extent(area.getSize().x, std::max(0, area.bottom - 80 - cardsTop)));
    const auto grid = frontend::LayoutTiles(cardArea, seats_.size(), Extent(3, 2), Extent(300, 200));
    for(unsigned i = 0; i < seats_.size(); ++i)
    {
        auto* card = GetCtrl<Window>(ID_Seat + i);
        card->SetPos(grid.tiles[i].getOrigin());
        card->Resize(grid.tiles[i].getSize());
    }
}

unsigned dskParty::GetNumPadSlots() const
{
    return seats_.empty() ? 1u : seats_.size();
}

bool dskParty::HandleCardInput(const unsigned slot, const PadButton button)
{
    auto& input = WINDOWMANAGER.GetPadInput();
    auto& focus = input.GetFocus(slot);
    const unsigned seat = slot == 0 ? 0 : seats_.SeatOfDevice(input.GetActingDevice());
    if(seat >= seats_.size())
    {
        // A controller without a seat may only look for a free seat and take it.
        const auto* target = dynamic_cast<const ctrlLobbyPlayerCard*>(focus.GetFocused());
        if(button == PadButton::A)
        {
            if(target)
                OnSeatPressed(target->GetModel().GetSeatId(), slot);
            return true;
        }
        unsigned index = target ? target->GetModel().GetSeatId() : 1u;
        if(button == PadButton::DpadLeft || button == PadButton::DpadUp || button == PadButton::LeftShoulder)
            index = index > 1 ? index - 1 : 1;
        else if(button == PadButton::DpadRight || button == PadButton::DpadDown || button == PadButton::RightShoulder)
            index = std::min(index + 1, seats_.size() - 1);
        focus.FocusCtrl(GetCtrl<Window>(ID_Seat + index));
        return true;
    }
    auto* card = GetCtrl<ctrlLobbyPlayerCard>(ID_Seat + seat);
    // The host moves freely between its card and Start; only on its card do the D-pad rows apply.
    if(slot == 0 && focus.GetFocused() != card)
        return false;
    if(slot != 0)
        focus.FocusCtrl(card);
    switch(button)
    {
        case PadButton::DpadUp:
            if(slot == 0 && card->GetModel().GetRow() == LobbyPlayerCardModel::Row::Color)
                return false;
            card->GetModel().MoveRow(false);
            break;
        case PadButton::DpadDown:
            if(slot == 0 && card->GetModel().GetRow() == LobbyPlayerCardModel::Row::SharedTribe)
            {
                // Down from the last row reaches Start, wherever the grid put the host's card.
                focus.FocusCtrl(GetCtrl<Window>(ID_Start));
                break;
            }
            card->GetModel().MoveRow(true);
            break;
        case PadButton::DpadLeft: ApplyCardValue(seat, false); break;
        case PadButton::DpadRight: ApplyCardValue(seat, true); break;
        case PadButton::A: break;
        default: return slot != 0; // a guest never reaches the host's controls
    }
    return true;
}

void dskParty::ApplyCardValue(const unsigned seat, const bool forward)
{
    if(countdown_)
        return;
    auto& model = GetCtrl<ctrlLobbyPlayerCard>(ID_Seat + seat)->GetModel();
    // Authority is read again right before sending; the card changes only with the server's broadcast.
    model.UpdateSnapshot(CardSnapshot(seat));
    const auto proposal = model.ProposeValue(forward);
    if(!proposal)
        return;
    ApplyValue(model, seats_[seat].playerId, *proposal);
}

void dskParty::ApplyValue(const LobbyPlayerCardModel& model, const unsigned playerId,
                          const LobbyPlayerCardModel::Values& value)
{
    if(!host_)
    {
        // A joined player changes only their own tribe, through the server; as in the classic lobby the change is
        // shown at once and makes them not ready, so they confirm the game they will play.
        auto& player = gameLobby_->getPlayer(playerId);
        switch(model.GetRow())
        {
            case LobbyPlayerCardModel::Row::Color:
                player.color = value.color;
                GAMECLIENT.Command_SetColor(value.color);
                break;
            case LobbyPlayerCardModel::Row::Nation:
                player.nation = value.nation;
                GAMECLIENT.Command_SetNation(value.nation);
                break;
            case LobbyPlayerCardModel::Row::Team:
                player.team = value.team;
                GAMECLIENT.Command_SetTeam(value.team);
                break;
            case LobbyPlayerCardModel::Row::SharedTribe: return;
        }
        SetReady(false);
        UpdateNetCards();
        return;
    }
    switch(model.GetRow())
    {
        case LobbyPlayerCardModel::Row::Color: lobbyController_->SetColor(playerId, value.color); break;
        case LobbyPlayerCardModel::Row::Nation: lobbyController_->SetNation(playerId, value.nation); break;
        case LobbyPlayerCardModel::Row::Team: lobbyController_->SetTeam(playerId, value.team); break;
        case LobbyPlayerCardModel::Row::SharedTribe:
            SetTogether(value.sharedTribe);
            // The host's controller stays on the mode row of its rebuilt card.
            if(auto* hostCard = GetCtrl<ctrlLobbyPlayerCard>(ID_Seat))
                hostCard->GetModel().SelectRow(LobbyPlayerCardModel::Row::SharedTribe);
            break;
    }
}

bool dskParty::FitsOwnTribes() const
{
    frontend::LocalSeats own;
    own.Build(*gameLobby_, *lobbyController_, WINDOWMANAGER.GetPadInput().GetRouter(), localPlayerId_, false);
    // Only the number of taken seats matters to the check, not which controllers they are.
    return !own.empty() && !own.PartyNeedsMoreSeats(std::vector<PadDeviceId>(seats_.NumTaken(), InvalidPadDevice));
}

void dskParty::SetTogether(const bool together)
{
    if(together == together_ || countdown_ || !CanOwnTribes())
        return;
    if(!together && !FitsOwnTribes())
    {
        WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(
          _("Not enough seats"), _("This map has no tribe of their own for every player. Play one tribe together."),
          this, MsgboxButton::Ok, MsgboxIcon::ExclamationRed, ID_mbInfo));
        return;
    }
    std::vector<PadDeviceId> guests;
    for(unsigned i = 1; i < seats_.size(); ++i)
    {
        if(seats_[i].taken && seats_[i].device != InvalidPadDevice)
            guests.push_back(seats_[i].device);
    }
    // Standing up in the old mode gives the slots back what was there before anybody sat down.
    seats_.StandAll();
    GAMECLIENT.SetSharedLocalViews(0);
    together_ = together;
    CreateSeats();
    auto& router = WINDOWMANAGER.GetPadInput().GetRouter();
    router.SetNumSlots(GetNumPadSlots());
    for(const PadDeviceId device : guests)
    {
        for(unsigned i = 1; i < seats_.size(); ++i)
        {
            if(!seats_[i].taken && seats_.IsJoinable(i))
            {
                seats_.Press(i, device, std::max(1u, router.GetSlot(device)));
                break;
            }
        }
    }
    UpdateCards();
    UpdateModeText();
    UpdateRules();
    Layout();
}

void dskParty::UpdateModeText()
{
    std::string text;
    if(online_ && GAMECLIENT.IsCoopMember())
        text = helpers::format(_("You play together with %1%"), gameLobby_->getPlayer(localPlayerId_).name);
    else if(online_ && !ordinary_ && !gameLobby_->isSavegame())
        text = _("Everyone plays the campaign's tribe together.");
    else if(online_)
        text = _("Every player has a tribe of their own.");
    else if(gameLobby_->isSavegame())
        text = _("Everyone plays the saved tribe together.");
    else if(!ordinary_)
        text = _("Everyone plays the campaign's tribe together.");
    else if(together_)
        text = _("Everyone plays one tribe together.");
    else
        text = _("Everyone plays a tribe of their own.");
    GetCtrl<ctrlText>(ID_Mode)->SetText(text);
}

bool dskParty::IsRuleAllowed(const Rule rule) const
{
    if(!host_)
        return rule == Rule::Options; // a joined player may look at the add-ons, nothing else
    if(countdown_)
        return false;
    switch(rule)
    {
        case Rule::Tribes: return CanOwnTribes();
        case Rule::Speed: return true;
        case Rule::Opponents: return !lua_ || lua_->IsChangeAllowed("playerState");
        case Rule::Options: return true; // read-only when the script allows no addon, see StepRule
        default: return !lua_ || lua_->IsChangeAllowed("general");
    }
}

void dskParty::UpdateRules()
{
    if(!ordinary_)
        return;
    const GlobalGameSettings& ggs = rules_;
    // MSVC cannot pick the const/non-const GetCtrl overload through a captured this, so go through a reference
    Window& self = *this;
    const auto set = [this, &self](const Rule rule, const std::string& text) {
        auto* button = self.GetCtrl<ctrlTextButton>(ID_Rule + static_cast<unsigned>(rule));
        if(button->GetText() != text)
            button->SetText(text);
        button->SetEnabled(IsRuleAllowed(rule));
    };
    set(Rule::Tribes, together_ ? _("One tribe together") : _("Own tribes"));
    set(Rule::Goals, goalName(ggs.objective));
    set(Rule::Goods, goodsName(ggs.startWares));
    set(Rule::Exploration, explorationName(ggs.exploration));
    set(Rule::Speed, speedName(ggs.speed));
    const auto current = opponentsChosen_ ? std::optional<Opponents>(opponents_) : CurrentOpponents();
    set(Rule::Opponents, current ? opponentsName(*current) : _("As set by the map"));
    set(Rule::LockTeams, onOff(ggs.lockedTeams));
    set(Rule::TeamView, onOff(ggs.teamView));
    set(Rule::RandomStart, onOff(ggs.randomStartPosition));
    const bool addonsAllowed =
      host_ && (!lua_ || lua_->IsChangeAllowed("addonsAll") || lua_->IsChangeAllowed("addonsSome"));
    set(Rule::Options, addonsAllowed ? _("Change Settings...") : _("View Settings..."));
    // During the countdown the drawer would hide "Cancel start" and the chat.
    GetCtrl<ctrlButton>(ID_Rules)->SetEnabled(!countdown_);
}

void dskParty::StepRule(const Rule rule, const bool forward)
{
    if(!IsRuleAllowed(rule))
        return;
    GlobalGameSettings& ggs = rules_;
    switch(rule)
    {
        case Rule::Tribes: SetTogether(!together_); return;
        case Rule::Opponents:
            if(!opponentsChosen_)
                opponents_ = CurrentOpponents().value_or(Opponents::Easy);
            opponentsChosen_ = true;
            opponents_ = stepEnum(opponents_, forward, 4);
            UpdateRules();
            return;
        case Rule::Options:
        {
            if(WINDOWMANAGER.FindNonModalWindow(CGI_ADDONS, SHARED_WINDOW_OWNER))
                return;
            std::unique_ptr<iwAddons> window;
            if(host_ && (!lua_ || lua_->IsChangeAllowed("addonsAll")))
                window = std::make_unique<iwAddons>(ggs, this, AddonChangeAllowed::All);
            else if(host_ && lua_->IsChangeAllowed("addonsSome"))
                window =
                  std::make_unique<iwAddons>(ggs, this, AddonChangeAllowed::WhitelistOnly, lua_->GetAllowedAddons());
            else
                window = std::make_unique<iwAddons>(ggs, this, AddonChangeAllowed::None);
            WINDOWMANAGER.Show(std::move(window));
            return;
        }
        case Rule::Goals:
            // The tournament goals belong to the online lobby; locally the four ordinary ones cycle.
            ggs.objective = static_cast<GameObjective>(
              stepEnum(static_cast<unsigned>(std::min(ggs.objective, GameObjective::EconomyMode)), forward, 4));
            break;
        case Rule::Goods:
            ggs.startWares = stepEnum(ggs.startWares, forward, helpers::NumEnumValues_v<StartWares>);
            break;
        case Rule::Exploration:
            ggs.exploration = stepEnum(ggs.exploration, forward, helpers::NumEnumValues_v<Exploration>);
            break;
        case Rule::Speed: ggs.speed = stepEnum(ggs.speed, forward, helpers::NumEnumValues_v<GameSpeed>); break;
        case Rule::LockTeams: ggs.lockedTeams = !ggs.lockedTeams; break;
        case Rule::TeamView: ggs.teamView = !ggs.teamView; break;
        case Rule::RandomStart: ggs.randomStartPosition = !ggs.randomStartPosition; break;
    }
    SendRules();
}

void dskParty::SendRules()
{
    ++pendingRules_;
    lobbyController_->ChangeGlobalGameSettings(rules_);
    UpdateRules();
}

void dskParty::OpenRules(const bool open)
{
    if(!ordinary_ || open == rulesOpen_)
        return;
    rulesOpen_ = open;
    for(unsigned i = 0; i < seats_.size(); ++i)
        GetCtrl<Window>(ID_Seat + i)->SetVisible(!open);
    GetCtrl<Window>(ID_Start)->SetVisible(!open && !coPlayer_); // a co-player has no Start or Ready
    GetCtrl<Window>(ID_Rules)->SetVisible(!open);
    GetCtrl<Window>(ID_Hint)->SetVisible(!open && !online_);
    if(online_)
    {
        for(unsigned id = ID_Net; id <= ID_ChatSend; ++id)
        {
            if(auto* control = GetCtrl<Window>(id))
                control->SetVisible(!open);
        }
    }
    for(unsigned i = 0; i <= ID_RuleLast - ID_Rule; ++i)
    {
        GetCtrl<Window>(ID_RuleLabel + i)->SetVisible(open);
        GetCtrl<Window>(ID_Rule + i)->SetVisible(open);
    }
    GetCtrl<Window>(ID_RulesDone)->SetVisible(open);
    UpdateRules();
    UpdateCoop();
    Layout();
    // The host's controller lands on the first rule, and back on Rules when the drawer closes.
    auto& focus = WINDOWMANAGER.GetPadInput().GetFocus(0);
    if(focus.IsActive())
        focus.FocusCtrl(open ? FirstRule() : GetCtrl<Window>(ID_Rules));
}

Window* dskParty::FirstRule()
{
    // Tribes is disabled on a one-tribe map; focus must not stay on the hidden Rules button.
    for(unsigned id = ID_Rule; id <= ID_RuleLast; ++id)
    {
        if(GetCtrl<ctrlButton>(id)->GetEnabled())
            return GetCtrl<Window>(id);
    }
    return GetCtrl<Window>(ID_RulesDone);
}

void dskParty::OnSeatPressed(const unsigned seat, const unsigned slot)
{
    // Start validated these seats; a join or stand-up after it would load a party nobody checked.
    if(countdown_)
        return;
    if(seats_.Press(seat, WINDOWMANAGER.GetPadInput().GetActingDevice(), slot))
        UpdateCards();
}

bool dskParty::Msg_PadCommand(const unsigned slot, const PadButton button)
{
    // A previous button of this batch may have opened a question; it owns the input now.
    if(WINDOWMANAGER.GetTopMostWindow() || WINDOWMANAGER.IsSwitchPending())
        return true;
    if(rulesOpen_)
    {
        // The drawer is the host's; guests wait on their (hidden) cards meanwhile.
        if(slot != 0)
            return true;
        if(button == PadButton::B || button == PadButton::Start)
        {
            OpenRules(false);
            return true;
        }
        const auto* focused = WINDOWMANAGER.GetPadInput().GetFocus(0).GetFocused();
        const unsigned id = focused ? focused->GetID() : 0u;
        if((button == PadButton::DpadLeft || button == PadButton::DpadRight) && id >= ID_Rule && id <= ID_RuleLast)
        {
            const auto rule = static_cast<Rule>(id - ID_Rule);
            if(rule != Rule::Options)
                StepRule(rule, button == PadButton::DpadRight);
            return true;
        }
        return false; // up/down/A: the page's ordinary focus and buttons
    }
    if(button == PadButton::Start)
    {
        // Starting acts for everybody, so it belongs to the host seat's controller like leaving does.
        if(slot != 0)
            return false;
        Start();
        return true;
    }
    if(button == PadButton::B)
    {
        const unsigned mySeat = seats_.SeatOfDevice(WINDOWMANAGER.GetPadInput().GetActingDevice());
        if(mySeat > 0 && mySeat < seats_.size())
        {
            OnSeatPressed(mySeat, slot); // B on an own seat = stand up
            return true;
        }
        if(slot != 0)
            return false; // a controller without a seat must not end the game for all
        AskToLeave();
        return true;
    }
    if(online_)
        return slot == 0 && HandleNetCardInput(button);
    if(seats_.empty())
        return false;
    return HandleCardInput(slot, button);
}

bool dskParty::Msg_KeyDown(const KeyEvent& ke)
{
    if(ke.kt == KeyType::Escape)
    {
        if(rulesOpen_)
            OpenRules(false);
        else
            AskToLeave();
        return true;
    }
    return false;
}

Window* dskParty::GetPadEntryCtrl(const unsigned slot)
{
    if(rulesOpen_ && slot == 0)
        return FirstRule();
    if(slot == 0 || seats_.empty())
        return dskFrontEndPage::GetPadEntryCtrl(slot);
    const auto& router = WINDOWMANAGER.GetPadInput().GetRouter();
    for(unsigned i = 1; i < seats_.size(); ++i)
    {
        if(seats_[i].taken && seats_[i].device != InvalidPadDevice && router.GetSlot(seats_[i].device) == slot)
            return GetCtrl<Window>(ID_Seat + i);
    }
    // A newcomer starts on the first free seat, where pressing A joins.
    for(unsigned i = 1; i < seats_.size(); ++i)
    {
        if(!seats_[i].taken && seats_.IsJoinable(i))
            return GetCtrl<Window>(ID_Seat + i);
    }
    return GetCtrl<Window>(ID_Seat + 1);
}

void dskParty::OnChoose(const unsigned ctrl_id)
{
    if(ctrl_id == ID_Start)
        Start();
    else if(ctrl_id == ID_Rules)
        OpenRules(true);
    else if(ctrl_id == ID_RulesDone)
        OpenRules(false);
    else if(ctrl_id == ID_ChatSend)
        SendChat();
    else if(ctrl_id == ID_Coop && host_)
    {
        if(!countdown_)
            lobbyController_->SetCoopMembersAllowed(!GAMECLIENT.AreCoopMembersAllowed());
    } else if(ctrl_id >= ID_Net && ctrl_id <= ID_NetLast && online_)
    {
        const unsigned playerId = ctrl_id - ID_Net;
        if(playerId == localPlayerId_ && !GAMECLIENT.IsCoopMember())
        {
            // A or a click on the own card: the next value of its selected row (the D-pad steps both ways).
            if(countdown_)
                return;
            auto& model = GetCtrl<ctrlLobbyPlayerCard>(ctrl_id)->GetModel();
            model.UpdateSnapshot(NetCardSnapshot(playerId));
            if(const auto proposal = model.ProposeValue(true))
                ApplyValue(model, playerId, *proposal);
        } else if(host_ && !countdown_ && gameLobby_->getPlayer(playerId).ps == PlayerState::Occupied)
        {
            // Somebody who never gets ready would otherwise hold up the game for everyone.
            removeTarget_ = playerId;
            WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(
              _("Remove player"), helpers::format(_("Remove %1% from the game?"), gameLobby_->getPlayer(playerId).name),
              this, MsgboxButton::YesNo, MsgboxIcon::QuestionRed, ID_mbRemove));
        }
    } else if(ctrl_id == ID_Coop)
    {
        const auto leaders = coop::lobby::leaderCandidates(*gameLobby_, localPlayerId_);
        if(!leaders.empty() && GAMECLIENT.AreCoopMembersAllowed() && !GAMECLIENT.IsCoopSwitchPending())
        {
            GAMECLIENT.JoinCoopMember(static_cast<uint8_t>(leaders.front()));
            UpdateCoop(); // disabled until the server answered
        }
    } else if(ctrl_id >= ID_Rule && ctrl_id <= ID_RuleLast)
        StepRule(static_cast<Rule>(ctrl_id - ID_Rule), true); // A or a click: the next value
    else if(ctrl_id >= ID_Seat && ctrl_id <= ID_SeatLast)
    {
        // A mouse click carries no controller and is dropped by LocalSeats::Press: a seat without a controller
        // would be a player without input. A controller's A on a free card arrives here.
        const auto& input = WINDOWMANAGER.GetPadInput();
        if(seats_.SeatOfDevice(input.GetActingDevice()) == seats_.size())
            OnSeatPressed(ctrl_id - ID_Seat, input.GetActingSlot());
    }
}

void dskParty::SetReady(const bool ready)
{
    // A co-player has no readiness of its own: its player's counts.
    if(host_ || GAMECLIENT.IsCoopMember())
        return;
    auto& player = gameLobby_->getPlayer(localPlayerId_);
    if(player.isReady != ready)
    {
        player.isReady = ready;
        GAMECLIENT.Command_SetReady(ready);
    }
    auto* button = GetCtrl<ctrlTextButton>(ID_Start);
    const std::string text = ready ? _("Not Ready") : _("Ready");
    if(button->GetText() != text)
        button->SetText(text);
    UpdateNetCards();
}

void dskParty::UpdateCoop()
{
    auto* button = GetCtrl<ctrlTextButton>(ID_Coop);
    if(!button)
        return;
    if(host_)
    {
        // The host decides whether others may join a tribe as co-players (several people, one tribe).
        button->SetVisible(!rulesOpen_);
        button->SetEnabled(!countdown_);
        const std::string text =
          GAMECLIENT.AreCoopMembersAllowed() ? _("Co-players: allowed") : _("Co-players: not allowed");
        if(button->GetText() != text)
            button->SetText(text);
        return;
    }
    // Like the classic lobby's co-player row: with the host's permission, play a player's tribe together with them.
    const auto leaders = coop::lobby::leaderCandidates(*gameLobby_, localPlayerId_);
    const bool show =
      GAMECLIENT.AreCoopMembersAllowed() && !GAMECLIENT.IsCoopMember() && !leaders.empty() && !rulesOpen_;
    button->SetVisible(show);
    button->SetEnabled(show && !GAMECLIENT.IsCoopSwitchPending());
    if(show)
    {
        const std::string text = helpers::format(_("Play %1%'s tribe"), gameLobby_->getPlayer(leaders.front()).name);
        if(button->GetText() != text)
            button->SetText(text);
    }
}

void dskParty::CI_CoopMembersChanged()
{
    // Our switch to co-play was accepted: from now on we act for the player we joined.
    if(online_ && GAMECLIENT.IsCoopMember() && !coPlayer_)
    {
        BecomeCoPlayer();
        Layout();
        return;
    }
    if(online_)
        UpdateNetCards();
    UpdateCoop();
}

void dskParty::BecomeCoPlayer()
{
    // A co-player plays its player's tribe (doc/coop/SharedPlayerSlot.md): no card of its own, no readiness, no
    // Start; the leader's card names it, and the chat and Leave stay.
    OpenRules(false); // the new cards belong under a closed drawer
    coPlayer_ = true;
    localPlayerId_ = GAMECLIENT.GetPlayerId();
    for(unsigned i = 0; i < MAX_PLAYERS; ++i)
        DeleteCtrl(ID_Net + i);
    CreateNetCards();
    GetCtrl<Window>(ID_Start)->SetVisible(false);
    SetEntryFocus(ID_ChatEdit);
    // Already on screen (the switch was accepted here): the controller must not stay on a hidden or deleted control.
    auto& focus = WINDOWMANAGER.GetPadInput().GetFocus(0);
    if(focus.GetRoot() == this)
    {
        const auto* focused = focus.GetFocused();
        if(!focused || !focused->IsVisible() || (focused->GetID() >= ID_Net && focused->GetID() <= ID_NetLast))
            focus.FocusCtrl(GetCtrl<Window>(ID_ChatEdit));
    }
    UpdateModeText();
    UpdateCoop();
}

void dskParty::Start()
{
    if(coPlayer_)
        return;
    if(!host_)
    {
        SetReady(!gameLobby_->getPlayer(localPlayerId_).isReady);
        return;
    }
    if(countdown_)
    {
        // Online the countdown runs for seconds and someone may still be missing: the host can stop it.
        if(online_)
            lobbyController_->CancelCountdown();
        return;
    }
    OpenRules(false);
    if(online_)
    {
        const std::string waiting = NotReady();
        if(!waiting.empty())
        {
            WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(
              _("Not ready"), helpers::format(_("Waiting for these players to be ready: %1%"), waiting), this,
              MsgboxButton::Ok, MsgboxIcon::ExclamationRed, ID_mbInfo));
            return;
        }
    }
    if(ordinary_ && !CheckOptions())
        return;
    seats_.Sync();
    if(!seats_.PrepareForStart())
    {
        UpdateCards();
        WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(_("Error"), ClientErrorToStr(ClientError::LocalPlayerSetup), this,
                                                      MsgboxButton::Ok, MsgboxIcon::ExclamationRed, ID_mbInfo));
        return;
    }
    auto& host = gameLobby_->getPlayer(localPlayerId_);
    if(!host.isReady)
    {
        host.isReady = true;
        GAMECLIENT.Command_SetReady(true);
    }
    if(lua_)
        lua_->EventPlayerReady(localPlayerId_);
    if(ordinary_)
        ApplyOpponents();
    else if(online_)
        FillOpenSlots();
    countdown_ = true;
    UpdateRules();
    if(online_)
    {
        GetCtrl<ctrlTextButton>(ID_Start)->SetText(_("Cancel start"));
        UpdateCoop();
    }
    lobbyController_->StartCountdown(5);
}

static_assert(static_cast<unsigned>(AI::Level::Easy) == static_cast<unsigned>(dskParty::Opponents::Easy)
              && static_cast<unsigned>(AI::Level::Hard) == static_cast<unsigned>(dskParty::Opponents::Hard));

std::vector<unsigned> dskParty::OpponentSlots() const
{
    std::vector<unsigned> ids;
    for(unsigned id = 0; id < gameLobby_->getNumPlayers(); ++id)
    {
        if(id == localPlayerId_)
            continue;
        const auto seat = std::find_if(seats_.Get().begin(), seats_.Get().end(),
                                       [id](const frontend::LocalSeats::Seat& s) { return s.playerId == id; });
        if(seat != seats_.Get().end() && seat->taken)
            continue;
        const auto& player = gameLobby_->getPlayer(id);
        // Online an open slot nobody joined is the host's to fill or close at Start, like a computer player.
        if(online_ && player.ps == PlayerState::Free)
        {
            ids.push_back(id);
            continue;
        }
        const bool computer = player.ps == PlayerState::AI && player.aiInfo.type == AI::Type::Default;
        // A seat stood up a moment ago still shows its placeholder until the server echoes the restore, which
        // it handles before anything sent after it.
        const bool restoring = seat != seats_.Get().end() && player.ps == PlayerState::AI
                               && player.aiInfo.type == AI::Type::Dummy && seat->savedPs == PlayerState::AI
                               && seat->savedAi.type == AI::Type::Default;
        if(computer || restoring)
            ids.push_back(id);
    }
    return ids;
}

std::optional<dskParty::Opponents> dskParty::CurrentOpponents() const
{
    std::optional<Opponents> current;
    for(const unsigned id : OpponentSlots())
    {
        const auto& player = gameLobby_->getPlayer(id);
        const auto level = player.aiInfo.type == AI::Type::Dummy ? AI::Level::Easy : player.aiInfo.level;
        const auto opponents =
          player.ps == PlayerState::Free ? Opponents::None : static_cast<Opponents>(static_cast<unsigned>(level));
        if(current && *current != opponents)
            return std::nullopt; // the map script set different levels
        current = opponents;
    }
    return current.value_or(Opponents::None);
}

void dskParty::ApplyOpponents()
{
    // Untouched, the computer players stay as the map (or its script) set them up; open network slots are closed,
    // since the server starts no game with one.
    const bool chosen = opponentsChosen_ && IsRuleAllowed(Rule::Opponents);
    if(!chosen && !online_)
        return;
    for(const unsigned id : OpponentSlots())
    {
        const auto& player = gameLobby_->getPlayer(id);
        const bool open = player.ps == PlayerState::Free;
        if(!chosen && !open)
            continue;
        if(!chosen || opponents_ == Opponents::None)
        {
            // Reopened if the server refuses the start (CI_CancelCountdown), or the seat could not be joined again.
            closedByOpponents_.push_back({id, open ? PlayerState::Free : PlayerState::AI,
                                          player.aiInfo.type == AI::Type::Dummy || open ?
                                            AI::Info(AI::Type::Default, AI::Level::Easy) :
                                            player.aiInfo});
            lobbyController_->CloseSlot(id);
        } else if(open)
        {
            closedByOpponents_.push_back({id, PlayerState::Free, AI::Info(AI::Type::Default, AI::Level::Easy)});
            lobbyController_->SetPlayerState(
              id, PlayerState::AI,
              AI::Info(AI::Type::Default, static_cast<AI::Level>(static_cast<unsigned>(opponents_))));
        } else
        {
            const auto level = static_cast<AI::Level>(static_cast<unsigned>(opponents_));
            if(player.aiInfo.type != AI::Type::Default || player.aiInfo.level != level)
                lobbyController_->SetPlayerState(id, PlayerState::AI, AI::Info(AI::Type::Default, level));
        }
    }
}

void dskParty::FillOpenSlots()
{
    // A campaign or save has no rules drawer: a tribe nobody joined plays on as a computer player (the server starts
    // no game with an open slot, and a saved tribe cannot be closed). Opened again if the start is refused.
    for(unsigned id = 0; id < gameLobby_->getNumPlayers(); ++id)
    {
        if(gameLobby_->getPlayer(id).ps != PlayerState::Free)
            continue;
        closedByOpponents_.push_back({id, PlayerState::Free, AI::Info(AI::Type::Default, AI::Level::Easy)});
        lobbyController_->SetPlayerState(id, PlayerState::AI, AI::Info(AI::Type::Default, AI::Level::Easy));
    }
}

bool dskParty::CheckOptions()
{
    if(forceOptions_)
        return true;
    const GlobalGameSettings& ggs = rules_;
    if(ggs.objective == GameObjective::EconomyMode && !ggs.isEnabled(AddonId::PEACEFULMODE))
    {
        WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(
          _("Economy mode"),
          _("You chose the economy mode. In economy mode the player or team that collects the most of certain goods "
            "wins (check the Economic progress window in game).\n\n"
            "Some players like to play this objective in peaceful mode. Would you like to adjust settings for a "
            "peaceful game?\n"
            "Choosing Yes will activate peaceful mode, ban catapults and disable buildings receiving coins by default. "
            "After clicking Yes you will be able to review the changes and then start the game by clicking the Start "
            "game button again.\n"
            "Choosing No will start the game without any changes."),
          this, MsgboxButton::YesNoCancel, MsgboxIcon::QuestionGreen, ID_mbEconomy));
        return false;
    }
    if(ggs.isEnabled(AddonId::PEACEFULMODE)
       && (ggs.objective == GameObjective::Conquer3_4 || ggs.objective == GameObjective::TotalDomination))
    {
        WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(
          _("Peaceful mode"),
          _("You chose a war based victory condition but peaceful mode is still active. Would you like to deactivate "
            "peaceful mode before you start? Choosing No will start the game, Yes will let you review the changes."),
          this, MsgboxButton::YesNoCancel, MsgboxIcon::QuestionRed, ID_mbPeaceful));
        return false;
    }
    return true;
}

bool dskParty::GoBack()
{
    if(rulesOpen_)
    {
        OpenRules(false);
        return true;
    }
    return dskFrontEndPage::GoBack();
}

bool dskParty::OnBackAtRoot()
{
    AskToLeave();
    return true;
}

void dskParty::AskToLeave()
{
    // Leaving stops the hosted game; focus starts on "No" so a reflexive B, A stays here.
    WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(_("Return"), _("Do you really want to leave this game?"), this,
                                                  MsgboxButton::YesNo, MsgboxIcon::QuestionRed, ID_mbLeave));
}

void dskParty::Leave()
{
    GAMECLIENT.Stop();
    WINDOWMANAGER.Switch(frontend::MainMenu());
}

void dskParty::Msg_PaintBefore()
{
    dskFrontEndPage::Msg_PaintBefore();
    // An unplugged controller frees its seat; the cards follow.
    if(seats_.Sync())
        UpdateCards();
}

void dskParty::Msg_MsgBoxResult(const unsigned msgbox_id, const MsgboxResult mbr)
{
    if((msgbox_id == ID_mbLeave && mbr == MsgboxResult::Yes) || msgbox_id == ID_mbError)
        Leave();
    else if(msgbox_id == ID_mbRemove && mbr == MsgboxResult::Yes)
    {
        // The slot may have changed while the question was open; only remove who was asked about.
        if(removeTarget_ < gameLobby_->getNumPlayers() && !countdown_
           && gameLobby_->getPlayer(removeTarget_).ps == PlayerState::Occupied)
            lobbyController_->SetPlayerState(removeTarget_, PlayerState::Free, AI::Info()); // the server kicks
    } else if(msgbox_id == CGI_ADDONS && mbr == MsgboxResult::Yes)
    {
        // iwAddons wrote the host's rules in place; send them like every other rule.
        SendRules();
    } else if(msgbox_id == ID_mbEconomy || msgbox_id == ID_mbPeaceful)
    {
        auto& ggs = rules_;
        if(mbr == MsgboxResult::Yes)
        {
            // Yes = adjust and review, exactly as in the classic lobby; Start again starts.
            if(msgbox_id == ID_mbEconomy)
            {
                ggs.setSelection(AddonId::PEACEFULMODE, true);
                ggs.setSelection(AddonId::NO_COINS_DEFAULT, true);
                ggs.setSelection(AddonId::NO_ARMOR_DEFAULT, true);
                ggs.setSelection(AddonId::LIMIT_CATAPULTS, 2);
            } else
                ggs.setSelection(AddonId::PEACEFULMODE, false);
            SendRules();
        } else if(mbr == MsgboxResult::No)
        {
            forceOptions_ = true;
            Start();
            forceOptions_ = false;
        }
    }
}

void dskParty::CI_GGSChanged(const GlobalGameSettings& /*ggs*/)
{
    if(pendingRules_ > 0)
        --pendingRules_;
    if(pendingRules_ == 0)
        rules_ = gameLobby_->getSettings();
    UpdateRules();
    // The rules changed: a joined player confirms again, as in the classic lobby.
    SetReady(false);
}

void dskParty::CI_Error(const ClientError ce)
{
    WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(_("Error"), ClientErrorToStr(ce), this, MsgboxButton::Ok,
                                                  MsgboxIcon::ExclamationRed, ID_mbError));
}

void dskParty::CI_GameLoading(std::shared_ptr<Game> game)
{
    WINDOWMANAGER.Switch(std::make_unique<dskGameLoader>(std::move(game)));
}

void dskParty::CI_PlayerDataChanged(unsigned /*playerId*/)
{
    UpdateCards();
    if(online_)
    {
        UpdateNetCards();
        UpdateModeText(); // a co-player's line names its player
    }
    UpdateRules();
}

void dskParty::CI_ReadyChanged(const unsigned playerId, const bool ready)
{
    // As in the classic lobby: the script hears of the other players' readiness; the host's is sent on Start.
    if(ready && lua_ && host_ && playerId != localPlayerId_)
        lua_->EventPlayerReady(playerId);
    if(online_)
    {
        UpdateNetCards();
        UpdateCoop();
    }
}

void dskParty::CI_NewPlayer(const unsigned playerId)
{
    // The constructor's AI slots come back as joins too; the mission script hears of them as in the lobby.
    if(lua_ && host_)
        lua_->EventPlayerJoined(playerId);
    UpdateCards();
    if(online_)
    {
        UpdateNetCards();
        UpdateCoop();
    }
}

void dskParty::CI_PlayerLeft(const unsigned playerId)
{
    if(lua_ && host_)
        lua_->EventPlayerLeft(playerId);
    UpdateCards();
    if(online_)
    {
        UpdateNetCards();
        UpdateCoop();
    }
}

void dskParty::CI_PlayersSwapped(const unsigned player1, const unsigned player2)
{
    if(player1 == localPlayerId_)
        localPlayerId_ = player2;
    else if(localPlayerId_ == player2)
        localPlayerId_ = player1;
    seats_.OnPlayersSwapped(player1, player2);
    UpdateCards();
    if(online_)
    {
        // Only our own card is editable, and it moved with us.
        for(unsigned i = 0; i < gameLobby_->getNumPlayers() && i < MAX_PLAYERS; ++i)
            DeleteCtrl(ID_Net + i);
        CreateNetCards();
        UpdateModeText();
        Layout();
    }
}

void dskParty::CI_CancelCountdown(const bool error)
{
    countdown_ = false;
    // The start was refused: the tribes closed for it become computer players again, so their seats stay joinable.
    for(const auto& restore : closedByOpponents_)
        lobbyController_->SetPlayerState(restore.id, restore.ps, restore.ai);
    closedByOpponents_.clear();
    UpdateRules();
    if(online_)
    {
        if(host_)
            GetCtrl<ctrlTextButton>(ID_Start)->SetText(_("Start game"));
        UpdateCoop();
        GetCtrl<ctrlChat>(ID_Chat)->AddMessage("", "", 0, _("Start aborted"), 0xFFFFCC00);
    }
    if(error)
    {
        WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(
          _("Error"),
          online_ ? _("Game can only be started as soon as everybody has a unique color,everyone is "
                      "ready and all free slots are closed.") :
                    _("The game could not be started."),
          this, MsgboxButton::Ok, MsgboxIcon::ExclamationRed, ID_mbInfo));
    }
}

void dskParty::CI_Chat(const unsigned playerId, const ChatDestination /*cd*/, const std::string& msg)
{
    if(!online_ || playerId >= gameLobby_->getNumPlayers())
        return;
    const auto& player = gameLobby_->getPlayer(playerId);
    GetCtrl<ctrlChat>(ID_Chat)->AddMessage(s25util::Time::FormatTime("(%H:%i:%s)"), player.name, player.color, msg,
                                           COLOR_YELLOW);
}

void dskParty::CI_Countdown(const unsigned remainingTimeInSec)
{
    if(!online_)
        return;
    GetCtrl<ctrlChat>(ID_Chat)->AddMessage("", "", 0,
                                           remainingTimeInSec > 0 ?
                                             helpers::format(_("Starting in %1%..."), remainingTimeInSec) :
                                             std::string(_("Starting game, please wait")),
                                           0xFFFFBB00);
}

void dskParty::CI_PingChanged(unsigned /*playerId*/, unsigned short /*ping*/)
{
    if(online_)
    {
        UpdateNetCards();
        UpdateCoop();
    }
}
