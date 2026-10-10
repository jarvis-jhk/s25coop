// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "frontend/LocalSeats.h"
#include "GameLobby.h"
#include "JoinPlayerInfo.h"
#include "helpers/containerUtils.h"
#include "input/PadRouter.h"
#include "network/GameClient.h"
#include "network/IGameLobbyController.h"
#include "world/ViewportLayout.h"
#include "s25util/Log.h"
#include <algorithm>

namespace frontend {

void LocalSeats::Build(const GameLobby& lobby, IGameLobbyController& controller, PadRouter& router,
                       const unsigned localPlayerId, const bool together)
{
    seats_.clear();
    lobby_ = &lobby;
    controller_ = &controller;
    router_ = &router;
    localPlayerId_ = localPlayerId;
    together_ = together;

    const unsigned numPlayers = lobby.getNumPlayers();
    const unsigned numSeats = together ? MAX_VIEWPORTS : std::min<unsigned>(MAX_VIEWPORTS, numPlayers);
    if(numSeats < 2)
        return; // one card for one player - nothing to hand out

    // Seat 1 is the host slot. It is always taken and cannot be left; mouse and keyboard stay its own.
    seats_.push_back(Seat{localPlayerId, InvalidPadDevice, true});
    if(together)
    {
        // s25coop: every further seat is another view on the host's player; no slot is taken
        while(seats_.size() < numSeats)
            seats_.push_back(Seat{localPlayerId, InvalidPadDevice, false});
    }

    // Then first the slots --local-players named - otherwise the command line would have seats that are on
    // no card -, then the remaining ones in ascending order.
    const std::vector<uint8_t> fromCmdLine = GAMECLIENT.GetAdditionalLocalPlayers();
    const auto canSeat = [&](const unsigned id) {
        if(id == localPlayerId || id >= numPlayers)
            return false;
        const PlayerState ps = lobby.getPlayer(id).ps;
        return ps != PlayerState::Occupied && ps != PlayerState::Locked;
    };
    const auto alreadySeated = [&](const unsigned id) {
        return helpers::contains_if(seats_, [id](const Seat& s) { return s.playerId == id; });
    };
    for(const uint8_t id : together ? std::vector<uint8_t>{} : fromCmdLine)
    {
        if(seats_.size() >= numSeats)
            break;
        if(canSeat(id) && !alreadySeated(id))
        {
            // applied = true, and what standing up restores is the ORDINARY AI, not what is there now: that
            // is already the dummy the lobby wrote for exactly this local player
            // (GameClient::ApplyAdditionalLocalPlayers). Restoring it would leave an idle AI behind.
            Seat seat{id, InvalidPadDevice, true};
            seat.applied = true;
            seats_.push_back(seat);
        }
    }
    for(unsigned id = 0; id < numPlayers && seats_.size() < numSeats; ++id)
    {
        if(canSeat(id) && !alreadySeated(id))
            seats_.push_back(Seat{id, InvalidPadDevice, false});
    }
    if(seats_.size() < 2)
        seats_.clear();
}

unsigned LocalSeats::NumTaken() const
{
    return static_cast<unsigned>(std::count_if(seats_.begin(), seats_.end(), [](const Seat& s) { return s.taken; }));
}

unsigned LocalSeats::SeatOfDevice(const PadDeviceId device) const
{
    if(device == InvalidPadDevice)
        return size();
    for(unsigned i = 0; i < seats_.size(); ++i)
    {
        if(seats_[i].device == device)
            return i;
    }
    return size();
}

bool LocalSeats::IsJoinable(const unsigned seat) const
{
    if(seat == 0 || seat >= seats_.size() || !lobby_)
        return false;
    if(together_)
        return true; // takes no slot, so nothing the host does to the slots closes it
    const PlayerState ps = lobby_->getPlayer(seats_[seat].playerId).ps;
    // Occupied: a real network connection sits there. Locked: the host closed the slot. Either way it is no
    // longer a seat to hand out. A seat we already hold stays ours, of course (standing up).
    return seats_[seat].applied || (ps != PlayerState::Occupied && ps != PlayerState::Locked);
}

bool LocalSeats::Press(const unsigned seat, const PadDeviceId device, const unsigned slot)
{
    if(seat == 0 || seat >= seats_.size())
        return false; // seat 1 is the host's and is not handed out
    if(device == InvalidPadDevice)
        return false; // mouse and keyboard do not sit down: a seat without a controller has no input
    if(slot == 0)
        return false; // whoever controls the host seat is seated already

    const unsigned mySeat = SeatOfDevice(device);
    if(mySeat != seat && !IsJoinable(seat))
        return false; // the host closed this slot or a network player sits on it
    Seat& target = seats_[seat];
    if(mySeat == seat)
    {
        // Standing up. Apply() restores EXACTLY what was on the slot before sitting down - not the dummy
        // from ApplyAdditionalLocalPlayers, or every leaver would leave an idle AI, and not a blanket
        // default, or the host would lose his setting (FINDING 2).
        target.taken = false;
        target.device = InvalidPadDevice;
    } else if(mySeat < seats_.size()                                  // a controller sits on exactly one seat
              || (target.taken && target.device != InvalidPadDevice)) // taken
        return false;
    else if(!target.taken)
    {
        target.taken = true;
        target.device = device;
    } else
        target.device = device; // a seat --local-players took gets its controller now

    Apply();
    return true;
}

void LocalSeats::SeatParty(const std::vector<PadDeviceId>& members)
{
    if(seats_.empty() || members.empty())
        return;
    for(unsigned i = 0; i < seats_.size(); ++i)
    {
        seats_[i].taken = i < members.size();
        seats_[i].device = seats_[i].taken ? members[i] : InvalidPadDevice;
    }
    // Bind the leader explicitly: an unjoined controller may have navigated the page before.
    router_->AssignSlot(members.front(), 0);
    Apply();
}

bool LocalSeats::PartyNeedsMoreSeats(const std::vector<PadDeviceId>& members) const
{
    if(members.size() > seats_.size())
        return true;
    if(together_ || !lobby_)
        return false;
    for(unsigned i = 1; i < std::min<size_t>(members.size(), seats_.size()); ++i)
    {
        const auto state = lobby_->getPlayer(seats_[i].playerId).ps;
        if(state == PlayerState::Locked || state == PlayerState::Occupied)
            return true;
    }
    return false;
}

void LocalSeats::StandAll()
{
    for(unsigned i = 1; i < seats_.size(); ++i)
    {
        seats_[i].taken = false;
        seats_[i].device = InvalidPadDevice;
    }
    Apply();
}

void LocalSeats::OnPlayersSwapped(const unsigned player1, const unsigned player2)
{
    if(localPlayerId_ == player1)
        localPlayerId_ = player2;
    else if(localPlayerId_ == player2)
        localPlayerId_ = player1;
    // FINDING D: a seat points at a SLOT, and a swap moves the players between slots. The seat follows its
    // player - with savedPs/savedAi, since what was on its slot before sitting down moved with it
    // (GameClient::OnGameMessage(GameMessage_Player_Swap) swaps the JoinPlayerInfos and renames its list of
    // additional local players the same way). Without this, standing up would restore the wrong slot and
    // leave the dummy AI on the one actually left - the idle AI Apply() exists to avoid.
    for(Seat& seat : seats_)
    {
        if(seat.playerId == player1)
            seat.playerId = player2;
        else if(seat.playerId == player2)
            seat.playerId = player1;
    }
}

LocalSeats::Applied LocalSeats::Apply()
{
    if(seats_.empty() || !controller_)
        return Applied::Nothing;

    if(together_)
    {
        // s25coop: views on the host's player, no slot changes (GameClient::SetSharedLocalViews)
        unsigned numShared = 0;
        for(unsigned i = 1; i < seats_.size(); ++i)
        {
            if(!seats_[i].taken)
                continue;
            ++numShared;
            if(seats_[i].device != InvalidPadDevice)
                router_->AssignSlot(seats_[i].device, numShared);
        }
        router_->RebalanceUnassigned();
        GAMECLIENT.SetAdditionalLocalPlayers({});
        GAMECLIENT.SetSharedLocalViews(numShared);
        return Applied::SharedViews;
    }

    std::vector<uint8_t> ids;
    for(unsigned i = 1; i < seats_.size(); ++i)
    {
        if(seats_[i].taken)
            ids.push_back(static_cast<uint8_t>(seats_[i].playerId));
    }
    const std::string err =
      GameClient::ValidateAdditionalLocalPlayers(*lobby_, localPlayerId_, ids, GAMECLIENT.IsAIBattleModeOn());
    if(!err.empty())
    {
        // Cannot happen from here - the seats are built from exactly the slots the check allows. If it does:
        // do NOT touch the game state.
        LOG.write("LocalSeats: seat assignment rejected: %1%\n") % err;
        return Applied::Nothing;
    }

    GAMECLIENT.SetAdditionalLocalPlayers(ids);
    // FINDING 2: touch ONLY the slots these seats hold. Remember on sitting down what was there, restore
    // exactly that on standing up. A seat nobody ever sat on is never touched - not even one the host has
    // since set with the mouse.
    for(unsigned i = 1; i < seats_.size(); ++i)
    {
        Seat& seat = seats_[i];
        if(seat.taken && !seat.applied)
        {
            const JoinPlayerInfo& before = lobby_->getPlayer(seat.playerId);
            seat.savedPs = before.ps;
            seat.savedAi = before.aiInfo;
            seat.applied = true;
        } else if(!seat.taken && seat.applied)
        {
            controller_->SetPlayerState(seat.playerId, seat.savedPs, seat.savedAi);
            seat.applied = false;
        }
    }
    // Restore first, then pin the taken ones - ApplyAdditionalLocalPlayers must run AFTER the default
    // assignment (network/GameClient.cpp).
    GameClient::ApplyAdditionalLocalPlayers(*controller_, ids);

    // And the controllers' view slots WITHOUT GAPS: view i belongs to ids[i-1], the pad slot is the view
    // number. Otherwise the order of first use would decide in the game, and whoever took seat 3 here would
    // sit on seat 2 there.
    unsigned viewIdx = 1;
    for(unsigned i = 1; i < seats_.size(); ++i)
    {
        if(!seats_[i].taken)
            continue;
        if(seats_[i].device != InvalidPadDevice)
            router_->AssignSlot(seats_[i].device, viewIdx);
        ++viewIdx;
    }
    // A controller displaced by that - one that only navigated and happened to sit on that slot - gets a
    // free one back. Otherwise it could never join itself (PadRouter::RebalanceUnassigned).
    router_->RebalanceUnassigned();
    return Applied::Slots;
}

bool LocalSeats::Sync()
{
    if(seats_.empty())
        return false;
    // Deliberately two statements and no ||: both checks must run, even if the first found something.
    const bool unplugged = DropDisconnected();
    const bool closed = DropClosedByTheHost();
    if(!unplugged && !closed)
        return false;
    Apply();
    return true;
}

bool LocalSeats::DropDisconnected()
{
    bool changed = false;
    for(unsigned i = 1; i < seats_.size(); ++i)
    {
        if(seats_[i].device == InvalidPadDevice || router_->HasDevice(seats_[i].device))
            continue;
        seats_[i].device = InvalidPadDevice;
        seats_[i].taken = false;
        changed = true;
    }
    return changed;
}

bool LocalSeats::DropClosedByTheHost()
{
    if(!lobby_ || together_)
        return false;
    bool changed = false;
    for(unsigned i = 1; i < seats_.size(); ++i)
    {
        Seat& seat = seats_[i];
        if(!seat.taken)
            continue;
        const PlayerState ps = lobby_->getPlayer(seat.playerId).ps;
        // Locked: the host closed the slot. Occupied: a real network connection sits on it. Exactly the two
        // states IsJoinable rejects for JOINING.
        //
        // WHY NOT "ps != AI", the criterion GameClient::SetupLocalPlayers depends on: our own switch to the
        // dummy AI goes through the server and reaches the lobby a few frames later. In that window a seat
        // just taken would look like one taken away. Our own writes can never produce these two states.
        // What slips through is caught by PrepareForStart.
        if(ps != PlayerState::Locked && ps != PlayerState::Occupied)
            continue;
        seat.taken = false;
        seat.device = InvalidPadDevice;
        // THE HOST DECIDED. `applied` drops HERE and not in Apply(), so that does not write back the state
        // from BEFORE sitting down: that would silently undo the closing (FINDING 2 the other way round).
        seat.applied = false;
        changed = true;
    }
    return changed;
}

bool LocalSeats::PrepareForStart()
{
    if(seats_.empty() || !lobby_)
        return true;
    // The ordinary merge first. It runs every frame anyway; once more here so being able to start does not
    // depend on a frame between the host's last change and the start button.
    Sync();
    if(together_)
        return true; // s25coop: shared views take no slot, so there is none to lose
    bool dropped = false;
    for(unsigned i = 1; i < seats_.size(); ++i)
    {
        Seat& seat = seats_[i];
        if(!seat.taken || lobby_->getPlayer(seat.playerId).ps == PlayerState::AI)
            continue;
        seat.taken = false;
        seat.device = InvalidPadDevice;
        seat.applied = false;
        dropped = true;
    }
    if(!dropped)
        return true;
    Apply();
    return false;
}

} // namespace frontend
