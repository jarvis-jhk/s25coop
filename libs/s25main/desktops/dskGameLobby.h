// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Desktop.h"
#include "driver/PadEvent.h"
#include "frontend/LocalSeats.h"
#include "input/LobbyPlayerCardModel.h"
#include "network/ClientInterface.h"
#include "gameTypes/AIInfo.h"
#include "gameTypes/PlayerState.h"
#include "gameTypes/ServerType.h"
#include "liblobby/LobbyInterface.h"
#include <memory>
#include <vector>

class ctrlChat;
class GameLobby;
class LobbyPlayerInfo;
class LuaInterfaceSettings;
class GameLobbyController;
class ILobbyClient;
enum class Team : uint8_t;

/// Desktop für das Hosten-eines-Spiels-Fenster
class dskGameLobby final : public Desktop, public ClientInterface, public LobbyInterface
{
public:
    dskGameLobby(ServerType serverType, std::shared_ptr<GameLobby> gameLobby, unsigned playerId,
                 std::unique_ptr<ILobbyClient> lobbyClient);
    ~dskGameLobby();

    /// Größe ändern-Reaktionen die nicht vom Skaling-Mechanismus erfasst werden.
    void Resize(const Extent& newSize) override;
    void SetActive(bool activate = true) override;

    // --- Gamepad ------------------------------------------------------------------------------
    bool WantsPadInput() const override { return true; }
    /// Der EINZIGE Bildschirm mit mehr als einem Padfokus: hier sollen ja mehrere Leute
    /// gleichzeitig beitreten. Ausserhalb des Zuordnungsbildschirms bleibt es bei einem.
    unsigned GetNumPadSlots() const override;
    bool Msg_PadCommand(unsigned slot, PadButton button) override;
    Window* GetPadEntryCtrl(unsigned slot) override;
    bool HandlesPadControlCommands() const override { return !seats_.empty(); }
    bool AllowsPadWindowInput(unsigned slot) const override { return seats_.empty() || slot == 0; }

private:
    /// Woraus die Beschriftung einer Sitzkarte entsteht - und damit das Einzige, was sie
    /// aendern kann.
    enum class SeatCardKind
    {
        Host,
        Closed,
        Free,
        Slot,
        Pad
    };

    /// Der Zustand, aus dem die AKTUELL angezeigte Beschriftung einer Sitzkarte gebaut wurde.
    ///
    /// Msg_PaintBefore laeuft je Frame und ruft UpdateSeatPanel; ohne dieses Gedaechtnis
    /// entstuenden dort je Frame bis zu vier formatierte Zeichenketten, von denen sich
    /// zwischen zwei Frames fast nie eine aendert.
    struct SeatCardLabel
    {
        SeatCardKind kind = SeatCardKind::Host;
        unsigned value = 0;
        bool valid = false;
    };

    /// Gibt es hier ueberhaupt Sitzplaetze zu vergeben? Nur in einer selbst gehosteten,
    /// rein lokalen Partie ohne Savegame und ohne KI-Schlacht - genau die Bedingungen, unter
    /// denen GameClient::ValidateAdditionalLocalPlayers zustimmt und SetupLocalPlayers die
    /// Zusatzspieler beim Start annimmt (network/GameClient.cpp:1897-1939, 2007-2008).
    bool AreLocalSeatsAvailable() const;
    void CreateSeatPanel();
    /// Carry title-page joins into a local lobby once; later seat edits remain the players' choice.
    void SeatJoinedParty();
    bool joinedPartyHandled_ = false;
    bool joinedPartyNeedsSeats_ = false;
    /// s25coop: switch the seats between "own slot" and "together with seat 1" (one tribe). Stands everybody up.
    void SetSeatsTogether(bool together);
    void UpdateSeatPanel();
    LobbyPlayerCardModel::Snapshot SeatCardSnapshot(unsigned seat) const;
    bool HandleSeatCardInput(unsigned slot, PadButton button);
    void ApplySeatCardValue(unsigned seat, bool forward);
    /// Sitzzustand und Spielzustand zusammenfuehren: abgezogene Pads und Sitze, die der Host
    /// inzwischen geschlossen hat. Laeuft je Frame aus Msg_PaintBefore.
    void SyncSeatsWithGameState();
    /// After the seats changed the game state (frontend::LocalSeats::Apply): redraw the rows and cards.
    void ShowAppliedSeats();
    /// A controller pressed a seat card (frontend::LocalSeats::Press).
    void OnSeatButton(unsigned seat, unsigned slot);
    /// Letzte Klammer vor dem Countdown. Liefert false, wenn dieser Startversuch abgebrochen
    /// werden soll - der Zustand ist dann repariert und der Grund genannt.
    bool PrepareSeatsForStart();
    /// Die Rueckfrage vor dem Verlassen der Lobby - siehe Msg_PadCommand.
    void AskToLeave();

    void SetPlayerReady(unsigned char player, bool ready);
    // GGS von den Controls auslesen
    void UpdateGGS();
    /// Aktualisiert eine Spielerreihe (löscht Controls und legt neue an)
    void UpdatePlayerRow(unsigned row);
    /// s25coop: the co-player row above the chat (host: members and "Remove"; player: whom to play together with)
    void UpdateCoopRow();
    /// Whether this row is the player we control on our own. A co-player (member) acts for its player in the game but
    /// changes nothing of it in the lobby.
    bool IsOwnRow(unsigned row) const;

    /// Füllt die Felder einer Reihe aus
    void ChangeTeam(unsigned player, Team);
    void ChangeStartWares(unsigned player);
    void ChangeReady(unsigned player, bool ready);
    void ChangeNation(unsigned player, Nation);
    void ChangePortrait(unsigned player, unsigned portraitIndex);
    void ChangePing(unsigned playerId);
    void ChangeColor(unsigned player, unsigned color);

    void Msg_PaintBefore() override;
    void Msg_Group_ButtonClick(unsigned group_id, unsigned ctrl_id) override;
    void Msg_Group_CheckboxChange(unsigned group_id, unsigned ctrl_id, bool checked) override;
    void Msg_Group_ComboSelectItem(unsigned group_id, unsigned ctrl_id, unsigned selection) override;
    void Msg_ButtonClick(unsigned ctrl_id) override;
    void Msg_EditEnter(unsigned ctrl_id) override;
    void Msg_MsgBoxResult(unsigned msgbox_id, MsgboxResult mbr) override;
    void Msg_ComboSelectItem(unsigned ctrl_id, unsigned selection) override;
    void Msg_CheckboxChange(unsigned ctrl_id, bool checked) override;
    void Msg_OptionGroupChange(unsigned ctrl_id, unsigned selection) override;

    void CI_Error(ClientError ce) override;

    void CI_NewPlayer(unsigned playerId) override;
    void CI_PlayerLeft(unsigned playerId) override;

    void CI_GameLoading(std::shared_ptr<Game> game) override;

    void CI_PlayerDataChanged(unsigned playerId) override;
    void CI_PingChanged(unsigned playerId, unsigned short ping) override;
    void CI_ReadyChanged(unsigned playerId, bool ready) override;
    void CI_PlayersSwapped(unsigned player1, unsigned player2) override;
    void CI_GGSChanged(const GlobalGameSettings& ggs) override;
    void CI_CoopMembersChanged() override;

    void CI_Chat(unsigned playerId, ChatDestination cd, const std::string& msg) override;
    void CI_Countdown(unsigned remainingTimeInSec) override;
    void CI_CancelCountdown(bool error) override;

    void FlashGameChat();

    void LC_Status_Error(const std::string& error) override;
    void LC_Chat(const std::string& player, const std::string& text) override;

    /// Addon options check with regards to peaceful mode and economy mode
    bool forceOptions = false;
    bool checkOptions();

    void GoBack();
    bool IsSinglePlayer() const { return serverType == ServerType::Local; }
    /// Check whether the given setting can be changed, i.e. is not disabled by lua
    bool IsChangeAllowed(const std::string& setting) const;

    const ServerType serverType;
    std::shared_ptr<GameLobby> gameLobby_;
    unsigned localPlayerId_;
    std::unique_ptr<ILobbyClient> lobbyClient_;
    bool hasCountdown_;
    std::unique_ptr<LuaInterfaceSettings> lua;
    std::unique_ptr<GameLobbyController> lobbyController;
    bool wasActivated, allowAddonChange;
    ctrlChat *gameChat, *lobbyChat;
    unsigned lobbyChatTabAnimId, localChatTabAnimId;
    /// What the entries of the co-player combo box stand for: member ids (host) or player ids (others)
    std::vector<unsigned> coopChoices_;
    /// Sitz 0 ist immer der Host. Leer, wenn es hier keinen Splitscreen geben kann.
    frontend::LocalSeats seats_;
    std::vector<SeatCardLabel> seatLabels_;
    /// s25coop: every seat plays the host's tribe (shared views, doc/coop/SharedLocalViews.md) instead of a slot.
    /// The mode chosen for the next build of the seats; seats_.IsTogether() is the one built.
    bool seatsTogether_ = false;
};
