// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "driver/PadEvent.h"
#include "gameTypes/AIInfo.h"
#include "gameTypes/PlayerState.h"
#include <cstdint>
#include <vector>

class GameLobby;
class IGameLobbyController;
class PadRouter;

namespace frontend {

/// Who sits where in front of the TV in a self-hosted local game (doc/coop/FrontEnd.md, F7).
///
/// The seat logic of the local lobby, without its controls: which seats there are, who took which one
/// with which controller, and how that is written into the game state (GameClient's additional local
/// players or shared views, the slots' player state, the pad router's view slots). It lives here so the
/// classic lobby and the full-screen party page share one implementation instead of two copies of the
/// fixes noted below (FINDING 2, A, D). It shows no window: every call that cannot be repaired silently
/// tells the caller, which decides what to show.
///
/// A seat is NOT a simulation slot: `playerId` is the slot on the map, the index in the vector is the
/// number of the view on screen. Both orders must agree, because dskGameInterface::CreateViews builds the
/// views as "main player first, then GetAdditionalLocalPlayers() in vector order".
class LocalSeats
{
public:
    struct Seat
    {
        unsigned playerId = 0;
        /// Controller on this seat. InvalidPadDevice = none; the seat can still be taken, namely when it
        /// came from --local-players.
        PadDeviceId device = InvalidPadDevice;
        bool taken = false;
        /// Do WE hold this slot? Only then may standing up touch it.
        ///
        /// FINDING 2: without this distinction every join reset ALL untaken seats to the default AI - a
        /// slot the host had closed became AI again, a hard AI became easy. The seats are built once;
        /// what the host sets afterwards is not their business.
        bool applied = false;
        /// What was on the slot before we took it - the target of standing up.
        PlayerState savedPs = PlayerState::AI;
        AI::Info savedAi = AI::Info(AI::Type::Default, AI::Level::Easy);
    };

    /// What Apply() did, so the caller knows what to redraw.
    enum class Applied
    {
        /// Nothing: no seats, or the assignment was rejected (state untouched).
        Nothing,
        /// Shared views on the host's player; no slot changed.
        SharedViews,
        /// Own slots; the rows of every seat's player may have changed.
        Slots
    };

    /// Build the seats of a lobby. `together` = every seat is another view on the host's player.
    /// Seat 0 is always the host's slot. The slots --local-players named come first (with `together` off),
    /// then the remaining free ones in ascending order. Less than two seats means there are none.
    void Build(const GameLobby& lobby, IGameLobbyController& controller, PadRouter& router, unsigned localPlayerId,
               bool together);
    void Clear() { seats_.clear(); }

    bool empty() const { return seats_.empty(); }
    unsigned size() const { return static_cast<unsigned>(seats_.size()); }
    const Seat& operator[](unsigned seat) const { return seats_[seat]; }
    const std::vector<Seat>& Get() const { return seats_; }
    bool IsTogether() const { return together_; }
    unsigned NumTaken() const;

    /// Seat of this controller, or size() if it has none.
    unsigned SeatOfDevice(PadDeviceId device) const;
    /// May somebody sit down here NOW? Asks the CURRENT game state, not the one when the seats were built:
    /// the host may have closed the slot since, and then a join would silently undo his decision.
    bool IsJoinable(unsigned seat) const;
    /// A controller pressed a seat. Its own seat = stand up, a free one = sit down, a --local-players seat
    /// without controller = take its controller. `slot` 0 (whoever controls the host seat) and a press
    /// without a controller do nothing. Returns whether the seats changed (then Apply() has run).
    bool Press(unsigned seat, PadDeviceId device, unsigned slot);
    /// Seat the joined `members` in order (seat 0 = first member, who also gets view slot 0) and apply.
    void SeatParty(const std::vector<PadDeviceId>& members);
    /// Do the joined `members` lack a seat: more members than seats, or a seat a member would get closed
    /// or occupied since the seats were built (Lua's settings-ready callback may close one)?
    bool PartyNeedsMoreSeats(const std::vector<PadDeviceId>& members) const;
    /// Everybody but the host stands up, and that is applied. Done before the seat mode changes, so the
    /// slots taken in the old mode get back what was there before.
    void StandAll();
    /// The slot of a seat's player moved (GameMessage_Player_Swap): the seat follows its player, together
    /// with savedPs/savedAi. Seat ORDER stays: it is the order of the views (FINDING D).
    void OnPlayersSwapped(unsigned player1, unsigned player2);

    /// Write the seats into the game state: GameClient's additional local players or shared views, the
    /// slots' player state and the controllers' view slots.
    Applied Apply();
    /// Merge seat and game state: unplugged controllers and seats the host closed since. Returns whether
    /// anything changed (then Apply() has run).
    bool Sync();
    /// Last check before the countdown, on exactly the criterion GameClient::SetupLocalPlayers fails the
    /// start on: an additional local slot must be an AI. A seat failing it is vacated. Returns false if
    /// seats were dropped; the state is repaired then and the same button starts on the next press, the
    /// caller only says why a seat became empty.
    bool PrepareForStart();

private:
    /// An unplugged controller's seat becomes explicitly free. Otherwise the PadRouter would pull a
    /// neighbouring controller into the freed slot by itself, and a bystander would sit on seat 2 (XR-115).
    bool DropDisconnected();
    /// FINDING A: the host closed or gave away a seat somebody still SITS on. IsJoinable only protected
    /// JOINING. What remained was a slot both closed and additional local player - exactly what
    /// GameClient::SetupLocalPlayers fails the start on. The host decides; the seat is vacated.
    bool DropClosedByTheHost();

    std::vector<Seat> seats_;
    bool together_ = false;
    unsigned localPlayerId_ = 0;
    const GameLobby* lobby_ = nullptr;
    IGameLobbyController* controller_ = nullptr;
    PadRouter* router_ = nullptr;
};

} // namespace frontend
