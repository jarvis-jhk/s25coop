// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dskParty.h"
#include "GameLobby.h"
#include "GameLobbyController.h"
#include "JoinPlayerInfo.h"
#include "Loader.h"
#include "WindowManager.h"
#include "controls/ctrlLobbyPlayerCard.h"
#include "controls/ctrlText.h"
#include "controls/ctrlTextButton.h"
#include "desktops/dskGameLoader.h"
#include "driver/KeyEvent.h"
#include "frontend/MenuRoutes.h"
#include "helpers/format.hpp"
#include "ingameWindows/iwMsgbox.h"
#include "input/LocalViewColor.h"
#include "input/MenuPadInput.h"
#include "input/PadRouter.h"
#include "lua/LuaInterfaceSettings.h"
#include "network/ClientError.h"
#include "network/GameClient.h"
#include "ogl/FontStyle.h"
#include "gameTypes/ServerType.h"
#include "s25util/Log.h"
#include <algorithm>
#include <array>
#include <mygettext/mygettext.h>
#include <optional>

namespace {
enum
{
    ID_mbLeave = 1,
    ID_mbError,
    ID_mbInfo,
};
} // namespace

bool dskParty::IsFor(const ServerType serverType)
{
    const auto lobby = GAMECLIENT.GetGameLobby();
    return serverType == ServerType::Local && frontend::GetMenuStyle() == frontend::MenuStyle::FrontEnd && lobby
           && lobby->isHost() && !lobby->isSavegame() && GAMECLIENT.IsHostingCampaign()
           && !GAMECLIENT.IsAIBattleModeOn();
}

dskParty::dskParty(std::shared_ptr<GameLobby> gameLobby, const unsigned playerId)
    : dskFrontEndPage(_("Party")), gameLobby_(std::move(gameLobby)), localPlayerId_(playerId)
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
        else if(!lua_->EventSettingsInit(true, false))
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
        if(!lua_)
            lobbyController_->RemoveLuaScript();
    }
    for(unsigned i = 0; i < gameLobby_->getNumPlayers(); ++i)
    {
        if(!gameLobby_->getPlayer(i).isHost)
            lobbyController_->SetPlayerState(i, PlayerState::AI, AI::Info(AI::Type::Default, AI::Level::Easy));
    }

    AddText(ID_Map, DrawPoint(0, 0), GAMECLIENT.GetMapTitle(), COLOR_YELLOW, FontStyle::CENTER, LargeFont);
    AddText(ID_Mode, DrawPoint(0, 0), _("Everyone plays the campaign's tribe together."), COLOR_YELLOW,
            FontStyle::CENTER, NormalFont);
    AddText(ID_Hint, DrawPoint(0, 0), _("More players: press A on a free seat."), COLOR_YELLOW, FontStyle::CENTER,
            NormalFont);
    CreateSeats();
    AddTextButton(ID_Start, DrawPoint(0, 0), Extent(1, 1), TextureColor::Green2, _("Start game"), LargeFont);
    SetEntryFocus(ID_Start);
    Layout();
    GAMECLIENT.SetInterface(this);
}

dskParty::~dskParty()
{
    GAMECLIENT.RemoveInterface(this);
}

void dskParty::CreateSeats()
{
    seats_.Build(*gameLobby_, *lobbyController_, WINDOWMANAGER.GetPadInput().GetRouter(), localPlayerId_, true);
    for(unsigned i = 0; i < seats_.size(); ++i)
    {
        AddCtrl(std::make_unique<ctrlLobbyPlayerCard>(this, ID_Seat + i, DrawPoint(0, 0), Extent(1, 1), NormalFont,
                                                      LobbyPlayerCardModel(i, CardSnapshot(i))));
    }
    UpdateCards();
}

void dskParty::SetActive(const bool activate)
{
    dskFrontEndPage::SetActive(activate);
    if(!activate || activated_)
        return;
    activated_ = true;
    if(lua_)
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
    LobbyPlayerCardModel::Snapshot snapshot{{player.color, player.nation, player.team, true}, {}, {}};
    for(unsigned id = 0; id < gameLobby_->getNumPlayers(); ++id)
    {
        const auto& other = gameLobby_->getPlayer(id);
        if(id != seats_[seat].playerId && other.isUsed())
            snapshot.takenColors.push_back(other.color);
    }
    std::string reason;
    if(!seats_[seat].taken)
        reason = _("Press A to join");
    else if(seat != 0)
        reason = _("Shared tribe: host chooses");
    constexpr std::array ownKeys = {"ownColor", "ownNation", "ownTeam"};
    for(unsigned row = 0; row < ownKeys.size(); ++row)
    {
        snapshot.lockReasons[row] = reason;
        if(reason.empty() && lua_ && !lua_->IsChangeAllowed(ownKeys[row], true))
            snapshot.lockReasons[row] = _("Fixed by map script");
    }
    snapshot.lockReasons[static_cast<unsigned>(LobbyPlayerCardModel::Row::SharedTribe)] = _("One tribe required");
    return snapshot;
}

void dskParty::UpdateCards()
{
    const bool sharedCursors = seats_.NumTaken() > 1;
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

void dskParty::OnLayout()
{
    const auto area = GetFrame().content;
    const int width = static_cast<int>(area.getSize().x);
    const int centerX = area.left + width / 2;
    GetCtrl<ctrlText>(ID_Map)->SetPos(DrawPoint(centerX, area.top));
    GetCtrl<ctrlText>(ID_Mode)->SetPos(DrawPoint(centerX, area.top + 30));
    GetCtrl<ctrlText>(ID_Hint)->SetPos(DrawPoint(centerX, area.bottom - 70));
    auto* start = GetCtrl<Window>(ID_Start);
    const unsigned startWidth = std::min(320u, area.getSize().x);
    start->Resize(Extent(startWidth, 40));
    start->SetPos(DrawPoint(centerX - static_cast<int>(startWidth / 2), area.bottom - 44));
    if(seats_.empty())
        return;
    // Cards fill the room between the texts and the Start row: four across when wide, else two by two.
    const Rect cardArea(DrawPoint(area.left, area.top + 56),
                        Extent(area.getSize().x, std::max(0, static_cast<int>(area.getSize().y) - 56 - 80)));
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
    auto& model = GetCtrl<ctrlLobbyPlayerCard>(ID_Seat + seat)->GetModel();
    // Authority is read again right before sending; the card changes only with the server's broadcast.
    model.UpdateSnapshot(CardSnapshot(seat));
    const auto proposal = model.ProposeValue(forward);
    if(!proposal)
        return;
    const unsigned playerId = seats_[seat].playerId;
    switch(model.GetRow())
    {
        case LobbyPlayerCardModel::Row::Color: lobbyController_->SetColor(playerId, proposal->color); break;
        case LobbyPlayerCardModel::Row::Nation: lobbyController_->SetNation(playerId, proposal->nation); break;
        case LobbyPlayerCardModel::Row::Team: lobbyController_->SetTeam(playerId, proposal->team); break;
        case LobbyPlayerCardModel::Row::SharedTribe: break; // locked: one tribe
    }
}

void dskParty::OnSeatPressed(const unsigned seat, const unsigned slot)
{
    if(seats_.Press(seat, WINDOWMANAGER.GetPadInput().GetActingDevice(), slot))
        UpdateCards();
}

bool dskParty::Msg_PadCommand(const unsigned slot, const PadButton button)
{
    // A previous button of this batch may have opened a question; it owns the input now.
    if(WINDOWMANAGER.GetTopMostWindow() || WINDOWMANAGER.IsSwitchPending())
        return true;
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
    if(seats_.empty())
        return false;
    return HandleCardInput(slot, button);
}

bool dskParty::Msg_KeyDown(const KeyEvent& ke)
{
    if(ke.kt == KeyType::Escape)
    {
        AskToLeave();
        return true;
    }
    return false;
}

Window* dskParty::GetPadEntryCtrl(const unsigned slot)
{
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
    else if(ctrl_id >= ID_Seat && ctrl_id <= ID_SeatLast)
    {
        // A mouse click carries no controller and is dropped by LocalSeats::Press: a seat without a controller
        // would be a player without input. A controller's A on a free card arrives here.
        const auto& input = WINDOWMANAGER.GetPadInput();
        if(seats_.SeatOfDevice(input.GetActingDevice()) == seats_.size())
            OnSeatPressed(ctrl_id - ID_Seat, input.GetActingSlot());
    }
}

void dskParty::Start()
{
    if(countdown_)
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
    countdown_ = true;
    lobbyController_->StartCountdown(5);
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
}

void dskParty::CI_NewPlayer(const unsigned playerId)
{
    // The constructor's AI slots come back as joins too; the mission script hears of them as in the lobby.
    if(lua_)
        lua_->EventPlayerJoined(playerId);
    UpdateCards();
}

void dskParty::CI_PlayerLeft(const unsigned playerId)
{
    if(lua_)
        lua_->EventPlayerLeft(playerId);
    UpdateCards();
}

void dskParty::CI_PlayersSwapped(const unsigned player1, const unsigned player2)
{
    if(player1 == localPlayerId_)
        localPlayerId_ = player2;
    else if(localPlayerId_ == player2)
        localPlayerId_ = player1;
    seats_.OnPlayersSwapped(player1, player2);
    UpdateCards();
}

void dskParty::CI_CancelCountdown(const bool error)
{
    countdown_ = false;
    if(error)
    {
        WINDOWMANAGER.Show(std::make_unique<iwMsgbox>(_("Error"), _("The game could not be started."), this,
                                                      MsgboxButton::Ok, MsgboxIcon::ExclamationRed, ID_mbInfo));
    }
}
