// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "GlobalGameSettings.h"
#include "ILobbyClient.hpp"
#include "dskFrontEndPage.h"
#include "frontend/LocalSeats.h"
#include "input/LobbyPlayerCardModel.h"
#include "network/ClientInterface.h"
#include "gameTypes/AIInfo.h"
#include "gameTypes/PlayerState.h"
#include "gameTypes/ServerType.h"
#include "gameData/MaxPlayers.h"
#include <memory>
#include <optional>
#include <utility>
#include <vector>

class GameLobby;
class GameLobbyController;
class LuaInterfaceSettings;

/// The full-screen party page (doc/coop/FrontEnd.md, slice F7b): who plays, on which seat, then Start.
///
/// It replaces the classic lobby for every self-hosted local start from the new menus: one big card per seat,
/// drawn from the same frontend::LocalSeats as the classic lobby's seat panel. Campaigns, scenarios and saves
/// have no rules to choose (the mission script or the save owns them) and every seat plays the host's tribe; a
/// save's tribes keep their colour, nation and team. An ordinary map (F7c) seats every player on a tribe of
/// their own, or all on one ("Tribes" on the host's card and in the rules), and has a rules drawer: goals,
/// goods, exploration, speed, computer opponents, team options and the game options window. A network game
/// on an ordinary map hosted from the new menus (F7d) opens it too: one card per tribe, the network players'
/// read-only with their name and whether they are ready, the host's own editable; a chat line and how to join.
/// A player who joins such a game from the new menus sees the same page (F7d2): their own card, Ready instead of
/// Start, the rules read-only. Network campaigns and saves hosted here, and joining as a co-player, keep
/// dskGameLobby.
class dskParty final : public dskFrontEndPage, public ClientInterface
{
public:
    /// Should iwConnecting open this page instead of dskGameLobby for the game it just connected to?
    static bool IsFor(ServerType serverType);

    dskParty(std::shared_ptr<GameLobby> gameLobby, unsigned playerId, ServerType serverType = ServerType::Local,
             std::unique_ptr<ILobbyClient> lobbyClient = nullptr);
    ~dskParty() override;

    enum ControlIds
    {
        ID_Seat = ID_FIRST_FREE,
        ID_SeatLast = ID_Seat + 3,
        ID_Start,
        ID_Map,
        ID_Mode,
        ID_Hint,
        ID_Rules,
        /// The rules drawer: a label and a value button per rule, in the order of Rule.
        ID_RuleLabel,
        ID_RuleLabelLast = ID_RuleLabel + 9,
        ID_Rule,
        ID_RuleLast = ID_Rule + 9,
        ID_RulesDone,
        /// Network game (F7d): one card per tribe of the map, how to join, and the chat.
        ID_Net,
        ID_NetLast = ID_Net + MAX_PLAYERS - 1,
        ID_Address,
        ID_Chat,
        ID_ChatEdit,
        ID_ChatSend,
        /// Host: allow co-players. A joined player: play another player's tribe together with them.
        ID_Coop,
    };
    /// The rows of the rules drawer; ID_Rule + Rule is its value button.
    enum class Rule : unsigned
    {
        Tribes,
        Goals,
        Goods,
        Exploration,
        Speed,
        Opponents,
        LockTeams,
        TeamView,
        RandomStart,
        Options,
    };
    /// Who plays the tribes no seat took, applied when the game starts: computer players of a level, or none.
    enum class Opponents : unsigned
    {
        Easy, ///< the first three in the order of AI::Level
        Medium,
        Hard,
        None,
    };

    void SetActive(bool activate = true) override;
    unsigned GetNumPadSlots() const override;
    bool Msg_PadCommand(unsigned slot, PadButton button) override;
    bool Msg_KeyDown(const KeyEvent& ke) override;
    Window* GetPadEntryCtrl(unsigned slot) override;
    bool HandlesPadControlCommands() const override { return !seats_.empty() || online_; }
    /// A guest's controller must not answer the host's questions (leave, errors).
    bool AllowsPadWindowInput(unsigned slot) const override { return seats_.empty() || slot == 0; }
    bool HasBackAction() const override { return true; }
    /// Back (header button, B, Escape) closes the rules drawer before it asks to leave.
    bool GoBack() override;
    void Msg_PaintBefore() override;
    void Msg_MsgBoxResult(unsigned msgbox_id, MsgboxResult mbr) override;

    const frontend::LocalSeats& GetSeats() const { return seats_; }
    /// A network game: tribe cards and chat instead of local seats.
    bool IsOnline() const { return online_; }
    /// An ordinary map: own tribes possible, rules drawer shown.
    bool HasRules() const { return ordinary_; }
    bool IsRulesOpen() const { return rulesOpen_; }
    Opponents GetOpponents() const { return opponents_; }
    /// The rules as the host set them; the lobby's copy follows once the server has echoed every change.
    const GlobalGameSettings& GetRules() const { return rules_; }

protected:
    void OnLayout() override;
    void OnChoose(unsigned ctrl_id) override;
    bool OnBackAtRoot() override;

private:
    void CI_Error(ClientError ce) override;
    void CI_NewPlayer(unsigned playerId) override;
    void CI_PlayerLeft(unsigned playerId) override;
    void CI_GameLoading(std::shared_ptr<Game> game) override;
    void CI_PlayerDataChanged(unsigned playerId) override;
    void CI_PlayersSwapped(unsigned player1, unsigned player2) override;
    void CI_CancelCountdown(bool error) override;
    void CI_GGSChanged(const GlobalGameSettings& ggs) override;
    void CI_ReadyChanged(unsigned playerId, bool ready) override;
    void CI_PingChanged(unsigned playerId, unsigned short ping) override;
    void CI_Chat(unsigned playerId, ChatDestination cd, const std::string& msg) override;
    void CI_Countdown(unsigned remainingTimeInSec) override;
    void CI_CoopMembersChanged() override;
    void Msg_EditEnter(unsigned ctrl_id) override;

    /// Network game: one card per tribe; only our own can be edited.
    void CreateNetCards();
    void UpdateNetCards();
    LobbyPlayerCardModel::Snapshot NetCardSnapshot(unsigned playerId) const;
    std::string NetCardText(unsigned playerId) const;
    /// D-pad on our own tribe card. false = not on it, the page's focus moves.
    bool HandleNetCardInput(PadButton button);
    /// "Join with 192.168.1.5, port 3665" and the like, for the players at other computers.
    std::string JoinText() const;
    /// The network players who are not ready yet, by name; empty = everybody is.
    std::string NotReady() const;
    void SendChat();
    /// A joined player: Ready / Not ready.
    void SetReady(bool ready);
    void UpdateCoop();

    void CreateSeats();
    /// An ordinary map with room for more than one tribe.
    bool CanOwnTribes() const;
    /// Switch between own tribes and one tribe together. Everybody stands up in the old mode (so the slots get
    /// back what was there) and the same controllers sit down again in the new one, as far as there are seats.
    void SetTogether(bool together);
    /// The party fits on the map's free tribes?
    bool FitsOwnTribes() const;
    void UpdateModeText();
    void OpenRules(bool open);
    void UpdateRules();
    /// One step of a rule; Options opens the game options window instead.
    void StepRule(Rule rule, bool forward);
    /// Send rules_ to the server.
    void SendRules();
    bool IsRuleAllowed(Rule rule) const;
    /// The economy/peaceful questions of the classic lobby. false = a question is shown, do not start yet.
    bool CheckOptions();
    /// The slots the computer-players rule acts on: ordinary computer players nobody sits on.
    std::vector<unsigned> OpponentSlots() const;
    /// Their common level, None if there are none, empty if the map set different ones.
    std::optional<Opponents> CurrentOpponents() const;
    /// Computer opponents on the tribes nobody sits on, just before the countdown, if the host chose them.
    void ApplyOpponents();
    /// Online campaigns and saves: every open slot gets a computer player for the start
    void FillOpenSlots();
    /// Our client now plays somebody else's tribe as its co-player
    void BecomeCoPlayer();
    /// The first rule the host may change (Done if none).
    Window* FirstRule();
    void SeatJoinedParty();
    /// Seats changed (LocalSeats::Apply/Sync): refresh the cards.
    void UpdateCards();
    LobbyPlayerCardModel::Snapshot CardSnapshot(unsigned seat) const;
    bool HandleCardInput(unsigned slot, PadButton button);
    void ApplyCardValue(unsigned seat, bool forward);
    /// Apply a proposed card value to a tribe: the row the model is on.
    void ApplyValue(const LobbyPlayerCardModel& model, unsigned playerId, const LobbyPlayerCardModel::Values& value);
    void OnSeatPressed(unsigned seat, unsigned slot);
    void Start();
    void AskToLeave();
    void Leave();

    std::shared_ptr<GameLobby> gameLobby_;
    unsigned localPlayerId_;
    ServerType serverType_;
    /// Internet lobby games: keeps the lobby session, which lists the game, alive while the page is open.
    std::unique_ptr<ILobbyClient> lobbyClient_;
    bool online_;
    /// We host the game (always so for a local one). A joined player only edits their own card and gets ready.
    bool host_;
    /// Playing another player's tribe as its co-player (F7e)
    bool coPlayer_ = false;
    std::unique_ptr<GameLobbyController> lobbyController_;
    std::unique_ptr<LuaInterfaceSettings> lua_;
    /// "Together" for a campaign (one human tribe) and a loaded save (shared by the party); an ordinary map with
    /// room for the party starts in own tribes.
    frontend::LocalSeats seats_;
    bool ordinary_ = false;
    /// The mode of the seats built (and the next build).
    bool together_ = true;
    bool rulesOpen_ = false;
    Opponents opponents_ = Opponents::Easy;
    /// The host changed the computer players; until then they stay as the map or its script set them.
    bool opponentsChosen_ = false;
    /// Slots changed for the start, with what they were (a closed computer player, an open network slot);
    /// given back if the server refuses it.
    struct Restore
    {
        unsigned id;
        PlayerState ps;
        AI::Info ai;
    };
    std::vector<Restore> closedByOpponents_;
    /// The host's rules are edited here and sent whole. The server echoes each send; an echo of an older send
    /// would otherwise overwrite a newer edit in the lobby's copy, and the next step would send that stale
    /// value back. So the lobby's copy is adopted only when no send is outstanding (Lua's own changes then).
    GlobalGameSettings rules_;
    unsigned pendingRules_ = 0;
    /// The economy/peaceful question was answered with "No": start as it is.
    bool forceOptions_ = false;
    /// The network player the host was asked about removing.
    unsigned removeTarget_ = 0;
    bool activated_ = false;
    bool countdown_ = false;
};
