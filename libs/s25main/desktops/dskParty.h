// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "dskFrontEndPage.h"
#include "frontend/LocalSeats.h"
#include "input/LobbyPlayerCardModel.h"
#include "network/ClientInterface.h"
#include <memory>

class GameLobby;
class GameLobbyController;
class LuaInterfaceSettings;
enum class ServerType : uint16_t;

/// The full-screen party page (doc/coop/FrontEnd.md, slice F7b): who plays, on which seat, then Start.
///
/// It replaces the classic lobby for self-hosted local campaign and scenario starts from the new menus.
/// Those have no rules to choose (the mission script owns them), so the page is only the seats: one big
/// card per seat, drawn from the same frontend::LocalSeats as the classic lobby's seat panel. Every seat
/// plays the campaign's tribe. Ordinary maps (rules and own tribes: F7c), saves and online games keep
/// dskGameLobby for now.
class dskParty final : public dskFrontEndPage, public ClientInterface
{
public:
    /// Should iwConnecting open this page instead of dskGameLobby for the game it just connected to?
    static bool IsFor(ServerType serverType);

    dskParty(std::shared_ptr<GameLobby> gameLobby, unsigned playerId);
    ~dskParty() override;

    enum ControlIds
    {
        ID_Seat = ID_FIRST_FREE,
        ID_SeatLast = ID_Seat + 3,
        ID_Start,
        ID_Map,
        ID_Mode,
        ID_Hint,
    };

    void SetActive(bool activate = true) override;
    unsigned GetNumPadSlots() const override;
    bool Msg_PadCommand(unsigned slot, PadButton button) override;
    bool Msg_KeyDown(const KeyEvent& ke) override;
    Window* GetPadEntryCtrl(unsigned slot) override;
    bool HandlesPadControlCommands() const override { return !seats_.empty(); }
    /// A guest's controller must not answer the host's questions (leave, errors).
    bool AllowsPadWindowInput(unsigned slot) const override { return seats_.empty() || slot == 0; }
    bool HasBackAction() const override { return true; }
    void Msg_PaintBefore() override;
    void Msg_MsgBoxResult(unsigned msgbox_id, MsgboxResult mbr) override;

    const frontend::LocalSeats& GetSeats() const { return seats_; }

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

    void CreateSeats();
    void SeatJoinedParty();
    /// Seats changed (LocalSeats::Apply/Sync): refresh the cards.
    void UpdateCards();
    LobbyPlayerCardModel::Snapshot CardSnapshot(unsigned seat) const;
    bool HandleCardInput(unsigned slot, PadButton button);
    void ApplyCardValue(unsigned seat, bool forward);
    void OnSeatPressed(unsigned seat, unsigned slot);
    void Start();
    void AskToLeave();
    void Leave();

    std::shared_ptr<GameLobby> gameLobby_;
    unsigned localPlayerId_;
    std::unique_ptr<GameLobbyController> lobbyController_;
    std::unique_ptr<LuaInterfaceSettings> lua_;
    /// Always "together": a campaign has one human tribe. The own-slot mode arrives with ordinary maps (F7c).
    frontend::LocalSeats seats_;
    bool activated_ = false;
    bool countdown_ = false;
};
