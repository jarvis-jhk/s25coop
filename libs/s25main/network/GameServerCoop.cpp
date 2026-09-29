// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

// s25coop: members, i.e. connections that control an existing world player together with it
// (doc/coop/SharedPlayerSlot.md). A member runs the world like any client and sends its commands every NWF, but nobody
// waits for them: the server appends them to the leader's next command set before that is stored and relayed, so every
// client, the replay and the async check see one player with a few more commands.

#include "GameServerCoop.h"
#include "GameMessage_GameCommand.h"
#include "GameServer.h"
#include "JoinPlayerInfo.h"
#include "RTTR_Version.h"
#include "helpers/containerUtils.h"
#include "network/GameMessages.h"
#include "s25util/Log.h"
#include "s25util/SocketSet.h"

/// Handles the messages of one member. Only what a member needs is accepted; the lobby settings of its own
/// (name, ready, nation, ...) are ignored because it has no slot, and anything else is a protocol error
class CoopMemberHandler : public GameMessageInterface
{
public:
    CoopMemberHandler(GameServer& server, GameServer::CoopMember& member) : server(server), member(member) {}

    bool OnNMSNull(unsigned) override { return true; }

    RTTR_IGNORE_OVERLOADED_VIRTUAL
    bool OnGameMessage(const GameMessage_Coop_JoinMember& msg) override
    {
        if(member.hasJoined() || server.state != GameServer::ServerState::Config)
            return false;
        server.JoinCoopMember(member, msg.player);
        return true;
    }

    bool OnGameMessage(const GameMessage_Server_Type& msg) override
    {
        if(!member.hasJoined() || member.versionOk || server.state != GameServer::ServerState::Config)
            return false;
        auto typeok = GameMessage_Server_TypeOK::StatusCode::Ok;
        if(msg.type != server.config.servertype)
            typeok = GameMessage_Server_TypeOK::StatusCode::InvalidServerType;
        else if(msg.revision != rttr::version::GetRevision())
            typeok = GameMessage_Server_TypeOK::StatusCode::WrongVersion;
        member.connection.sendMsg(GameMessage_Server_TypeOK(typeok, rttr::version::GetRevision()));
        member.versionOk = typeok == GameMessage_Server_TypeOK::StatusCode::Ok;
        if(!member.versionOk)
            server.KickCoopMember(member, "wrong server type or version");
        return true;
    }

    bool OnGameMessage(const GameMessage_Server_Password& msg) override
    {
        if(!member.versionOk || member.passwordOk || server.state != GameServer::ServerState::Config)
            return false;
        // A member never becomes host, but the host password is as good as the game password
        const bool ok = msg.password == server.config.password || msg.password == server.config.hostPassword;
        member.connection.sendMsg(GameMessage_Server_Password(ok ? "true" : "false"));
        member.passwordOk = ok;
        if(!ok)
            server.KickCoopMember(member, "wrong password");
        return true;
    }

    bool OnGameMessage(const GameMessage_Player_Name& msg) override
    {
        member.name = msg.playername;
        if(member.connection.isActive())
            server.coopMembersChanged_ = true;
        return true;
    }
    bool OnGameMessage(const GameMessage_Player_Portrait&) override { return true; }
    bool OnGameMessage(const GameMessage_Player_Ready&) override { return true; }
    bool OnGameMessage(const GameMessage_Player_Nation&) override { return true; }
    bool OnGameMessage(const GameMessage_Player_Team&) override { return true; }
    bool OnGameMessage(const GameMessage_Player_Color&) override { return true; }
    // Every client confirms a swap; only players have pending swaps to clear (a member follows its player's)
    bool OnGameMessage(const GameMessage_Player_SwapConfirm&) override { return true; }

    bool OnGameMessage(const GameMessage_MapRequest& msg) override
    {
        if(!member.passwordOk || server.state != GameServer::ServerState::Config)
            return false;
        return server.SendMap(member.connection, msg.requestInfo);
    }

    bool OnGameMessage(const GameMessage_Map_Checksum& msg) override
    {
        if(!member.passwordOk || server.state != GameServer::ServerState::Config || member.connection.isActive())
            return false;
        const MapInfo& mapinfo = server.mapinfo;
        const bool checksumok = msg.mapChecksum == mapinfo.mapChecksum && msg.luaChecksum == mapinfo.luaChecksum;
        GameServerPlayer& connection = member.connection;
        connection.sendMsgAsync(new GameMessage_Map_ChecksumOK(checksumok, !connection.isMapSending()));
        if(!checksumok)
        {
            if(connection.isMapSending())
                server.KickCoopMember(member, "wrong map checksum");
            return true;
        }
        // Unlike a player it takes no slot: nobody else is told, the lobby does not change
        connection.sendMsgAsync(new GameMessage_Server_Name(server.config.gamename));
        connection.sendMsgAsync(new GameMessage_Player_List(server.playerInfos));
        connection.sendMsgAsync(new GameMessage_GGSChange(server.ggs_));
        connection.setActive();
        server.coopMembersChanged_ = true;
        LOG.write("SERVER: %1% joined player %2% as a member\n") % member.name % unsigned(connection.playerId);
        return true;
    }

    bool OnGameMessage(const GameMessage_Pong&) override
    {
        if(member.connection.isActive())
            member.connection.calcPingTime();
        return true;
    }

    bool OnGameMessage(const GameMessage_Chat& msg) override
    {
        if(!member.connection.isActive())
            return false;
        server.SendToAll(GameMessage_Chat(member.connection.playerId, msg.destination, msg.text));
        return true;
    }

    bool OnGameMessage(const GameMessage_GameCommand& msg) override
    {
        if(!member.connection.isActive())
            return false;
        if(server.state == GameServer::ServerState::Loading)
            return msg.cmds.gcs.empty();
        if(server.state != GameServer::ServerState::Game)
            return false;
        // The member's checksum is not compared yet (step 5 needs checksums tagged with their GF)
        if(!msg.cmds.gcs.empty())
        {
            auto& pending = server.coopMemberCmds_[member.connection.playerId];
            // Far more than people can click between two NWFs of their leader; more is a broken or hostile client
            if(pending.size() + msg.cmds.gcs.size() > GameServer::maxCoopMemberCmds)
                server.KickCoopMember(member, "too many orders");
            else
                pending.insert(pending.end(), msg.cmds.gcs.begin(), msg.cmds.gcs.end());
        }
        return true;
    }

    // Answered with the server's own logs only; members are not part of the async check yet
    bool OnGameMessage(const GameMessage_AsyncLog&) override { return true; }
    RTTR_POP_DIAGNOSTIC

private:
    GameServer& server;
    GameServer::CoopMember& member;
};

unsigned GameServer::GetNumCoopMembers() const
{
    return static_cast<unsigned>(helpers::count_if(coopMembers_, [](const auto& member) {
        return member->connection.isActive() && member->connection.socket.isValid();
    }));
}

bool GameServer::AcceptCoopMember(const Socket& socket)
{
    if(!allowCoopMembers_ || coopMembers_.size() >= maxCoopMembers)
        return false;
    coopMembers_.push_back(std::make_unique<CoopMember>(socket, nextCoopMemberId_++));
    return true;
}

void GameServer::SetAllowCoopMembers(bool allow)
{
    allowCoopMembers_ = allow;
    coopMembersChanged_ = true;
}

bool GameServer::CanJoinCoopMember(uint8_t leader) const
{
    // Only a human player can be joined; AIs, free and closed slots cannot
    return leader < playerInfos.size() && playerInfos[leader].ps == PlayerState::Occupied;
}

void GameServer::JoinCoopMember(CoopMember& member, uint8_t leader)
{
    const bool ok = CanJoinCoopMember(leader);
    member.connection.sendMsg(GameMessage_Coop_JoinMember(ok ? leader : GameMessageWithPlayer::NO_PLAYER_ID));
    if(ok)
        member.connection.playerId = leader;
    else
        KickCoopMember(member, "the requested player cannot be joined");
}

bool GameServer::OnGameMessage(const GameMessage_Coop_JoinMember& msg)
{
    GameServerPlayer* player = GetNetworkPlayer(msg.senderPlayerID);
    if(state != ServerState::Config || !player || player->isMapSending())
    {
        KickPlayer(msg.senderPlayerID, KickReason::InvalidMsg, __LINE__);
        return true;
    }
    if(player->isActive())
    {
        SwitchToCoopMember(*player, msg.player);
        return true;
    }
    // A connection that got a free slot asks to be a member instead: hand its socket over and leave the slot free
    if(!allowCoopMembers_ || coopMembers_.size() >= maxCoopMembers)
    {
        KickPlayer(msg.senderPlayerID, KickReason::InvalidMsg, __LINE__);
        return true;
    }
    coopMembers_.push_back(std::make_unique<CoopMember>(player->socket, nextCoopMemberId_++));
    // Not closed: the member holds the other reference. Run() drops the invalid player afterwards
    player->socket = Socket();
    player->recvQueue.clear();
    player->sendQueue.clear();
    JoinCoopMember(*coopMembers_.back(), msg.player);
    return true;
}

void GameServer::SwitchToCoopMember(GameServerPlayer& player, uint8_t leader)
{
    const uint8_t oldId = player.playerId;
    // Refused: it simply stays the player it was. The host cannot leave its slot, it runs the server
    if(!allowCoopMembers_ || coopMembers_.size() >= maxCoopMembers || leader == oldId || IsHost(oldId)
       || !CanJoinCoopMember(leader))
    {
        player.sendMsgAsync(new GameMessage_Coop_JoinMember(GameMessageWithPlayer::NO_PLAYER_ID));
        return;
    }
    coopMembers_.push_back(std::make_unique<CoopMember>(player.socket, nextCoopMemberId_++));
    CoopMember& member = *coopMembers_.back();
    // It went through the handshake as a player already
    member.name = playerInfos[oldId].name;
    member.versionOk = member.passwordOk = true;
    member.connection.playerId = leader;
    // Keep what is queued in both directions: broadcasts it has not got yet, and whatever it sent after this request
    // (the member handler takes it from here; executeMsgs stops as the player's queue is now empty)
    using std::swap;
    swap(member.connection.sendQueue, player.sendQueue);
    swap(member.connection.recvQueue, player.recvQueue);
    member.connection.setActive();
    member.connection.sendMsgAsync(new GameMessage_Coop_JoinMember(leader));
    // Not closed: the member holds the other reference. Run() drops the invalid player afterwards
    player.socket = Socket();
    coopMembersChanged_ = true;
    LOG.write("SERVER: Player %1% (%2%) now plays player %3% as a member\n") % unsigned(oldId) % member.name
      % unsigned(leader);

    // Its slot is free again, as if it had left
    playerInfos[oldId].ps = PlayerState::Free;
    playerInfos[oldId].isReady = false;
    SendToAll(GameMessage_Player_Kicked(oldId, KickReason::NoCause, 0));
    CancelCountdown();
    AnnounceStatusChange();
}

bool GameServer::OnGameMessage(const GameMessage_Coop_AllowMembers& msg)
{
    if(state != ServerState::Config || !IsHost(msg.senderPlayerID))
    {
        KickPlayer(msg.senderPlayerID, KickReason::InvalidMsg, __LINE__);
        return true;
    }
    SetAllowCoopMembers(msg.allowed);
    return true;
}

bool GameServer::OnGameMessage(const GameMessage_Coop_KickMember& msg)
{
    if(!IsHost(msg.senderPlayerID))
    {
        KickPlayer(msg.senderPlayerID, KickReason::InvalidMsg, __LINE__);
        return true;
    }
    // Unknown ids are no error: the member may have left in the meantime
    for(const auto& member : coopMembers_)
    {
        if(member->id == msg.id)
            KickCoopMember(*member, "kicked by the host");
    }
    return true;
}

void GameServer::BroadcastCoopMembers()
{
    if(!coopMembersChanged_)
        return;
    coopMembersChanged_ = false;
    std::vector<CoopMemberInfo> members;
    members.reserve(coopMembers_.size());
    for(const auto& member : coopMembers_)
    {
        if(member->connection.isActive() && member->connection.socket.isValid())
            members.push_back(
              CoopMemberInfo{member->id, static_cast<uint8_t>(member->connection.playerId), member->name});
    }
    SendToAll(GameMessage_Coop_Members(allowCoopMembers_, std::move(members)));
}

void GameServer::KickCoopMember(CoopMember& member, const char* reason)
{
    if(!member.connection.socket.isValid())
        return;
    if(member.connection.isActive())
        coopMembersChanged_ = true;
    LOG.write("SERVER: Member %1% of player %2% removed: %3%\n") % member.name % unsigned(member.connection.playerId)
      % reason;
    member.connection.closeConnection();
}

void GameServer::KickCoopMembersOf(uint8_t leader)
{
    for(const auto& memberPtr : coopMembers_)
    {
        CoopMember& member = *memberPtr;
        if(member.connection.playerId == leader)
            KickCoopMember(member, "its player left");
    }
    coopMemberCmds_.erase(leader);
}

void GameServer::SwapCoopMembers(uint8_t player1, uint8_t player2)
{
    // Members follow their player to its new slot, as its own connection does; their clients do the same when they get
    // the swap message. Orders still buffered go along with them.
    bool changed = false;
    for(const auto& memberPtr : coopMembers_)
    {
        GameServerPlayer& connection = memberPtr->connection;
        if(connection.playerId == player1)
            connection.playerId = player2;
        else if(connection.playerId == player2)
            connection.playerId = player1;
        else
            continue;
        changed |= connection.isActive();
    }
    using std::swap;
    swap(coopMemberCmds_[player1], coopMemberCmds_[player2]);
    coopMembersChanged_ |= changed;
}

void GameServer::ReceiveCoopMemberMsgs()
{
    if(coopMembers_.empty())
        return;
    SocketSet set;
    for(const auto& memberPtr : coopMembers_)
    {
        CoopMember& member = *memberPtr;
        set.Add(member.connection.socket);
    }
    if(set.Select(0, 0) > 0)
    {
        for(const auto& memberPtr : coopMembers_)
        {
            CoopMember& member = *memberPtr;
            if(set.InSet(member.connection.socket) && !member.connection.receiveMsgs())
                KickCoopMember(member, "connection lost");
        }
    }
    set.Clear();
    for(const auto& memberPtr : coopMembers_)
    {
        CoopMember& member = *memberPtr;
        if(member.connection.socket.isValid())
            set.Add(member.connection.socket);
    }
    if(set.Select(0, 2) > 0)
    {
        for(const auto& memberPtr : coopMembers_)
        {
            CoopMember& member = *memberPtr;
            if(set.InSet(member.connection.socket))
                KickCoopMember(member, "socket error");
        }
    }
    // No handler adds or erases members (kicked ones are erased after sending), so the references stay valid
    for(const auto& memberPtr : coopMembers_)
    {
        CoopMember& member = *memberPtr;
        if(member.connection.hasTimedOut())
            KickCoopMember(member, "timeout");
        else
            member.connection.doPing();
        CoopMemberHandler handler(*this, member);
        while(member.connection.socket.isValid() && !member.connection.recvQueue.empty())
        {
            const auto msg = member.connection.recvQueue.pop();
            if(!msg->run(&handler, member.connection.playerId))
                KickCoopMember(member, "unexpected message");
        }
    }
}

void GameServer::SendCoopMemberMsgs()
{
    for(const auto& memberPtr : coopMembers_)
    {
        CoopMember& member = *memberPtr;
        if(member.connection.socket.isValid() && !member.connection.sendMsgs(10))
            KickCoopMember(member, "sending failed");
    }
    helpers::erase_if(coopMembers_, [](const auto& member) { return !member->connection.socket.isValid(); });
}

void GameServer::AppendCoopMemberCmds(uint8_t leader, PlayerGameCommands& cmds) const
{
    if(state != ServerState::Game)
        return;
    const auto it = coopMemberCmds_.find(leader);
    if(it != coopMemberCmds_.end())
        cmds.gcs.insert(cmds.gcs.end(), it->second.begin(), it->second.end());
}
