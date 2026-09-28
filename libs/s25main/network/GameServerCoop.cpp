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
        return true;
    }
    bool OnGameMessage(const GameMessage_Player_Portrait&) override { return true; }
    bool OnGameMessage(const GameMessage_Player_Ready&) override { return true; }
    bool OnGameMessage(const GameMessage_Player_Nation&) override { return true; }
    bool OnGameMessage(const GameMessage_Player_Team&) override { return true; }
    bool OnGameMessage(const GameMessage_Player_Color&) override { return true; }

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
    return static_cast<unsigned>(helpers::count_if(coopMembers_, [](const CoopMember& member) {
        return member.connection.isActive() && member.connection.socket.isValid();
    }));
}

bool GameServer::AcceptCoopMember(const Socket& socket)
{
    if(!allowCoopMembers_ || coopMembers_.size() >= maxCoopMembers)
        return false;
    coopMembers_.emplace_back(socket);
    return true;
}

void GameServer::JoinCoopMember(CoopMember& member, uint8_t leader)
{
    // Only a human player can be joined; AIs, free and closed slots cannot
    const bool ok = leader < playerInfos.size() && playerInfos[leader].ps == PlayerState::Occupied;
    member.connection.sendMsg(GameMessage_Coop_JoinMember(ok ? leader : GameMessageWithPlayer::NO_PLAYER_ID));
    if(ok)
        member.connection.playerId = leader;
    else
        KickCoopMember(member, "the requested player cannot be joined");
}

bool GameServer::OnGameMessage(const GameMessage_Coop_JoinMember& msg)
{
    // A connection that got a free slot asks to be a member instead: hand its socket over and leave the slot free
    GameServerPlayer* player = GetNetworkPlayer(msg.senderPlayerID);
    if(state != ServerState::Config || !player || player->isActive() || player->isMapSending() || !allowCoopMembers_
       || coopMembers_.size() >= maxCoopMembers)
    {
        KickPlayer(msg.senderPlayerID, KickReason::InvalidMsg, __LINE__);
        return true;
    }
    coopMembers_.emplace_back(player->socket);
    // Not closed: the member holds the other reference. Run() drops the invalid player afterwards
    player->socket = Socket();
    player->recvQueue.clear();
    player->sendQueue.clear();
    JoinCoopMember(coopMembers_.back(), msg.player);
    return true;
}

void GameServer::KickCoopMember(CoopMember& member, const char* reason)
{
    if(!member.connection.socket.isValid())
        return;
    LOG.write("SERVER: Member %1% of player %2% removed: %3%\n") % member.name % unsigned(member.connection.playerId)
      % reason;
    member.connection.closeConnection();
}

void GameServer::KickCoopMembersOf(uint8_t leader)
{
    for(CoopMember& member : coopMembers_)
    {
        if(member.connection.playerId == leader)
            KickCoopMember(member, "its player left");
    }
    coopMemberCmds_.erase(leader);
}

void GameServer::ReceiveCoopMemberMsgs()
{
    if(coopMembers_.empty())
        return;
    SocketSet set;
    for(const CoopMember& member : coopMembers_)
        set.Add(member.connection.socket);
    if(set.Select(0, 0) > 0)
    {
        for(CoopMember& member : coopMembers_)
        {
            if(set.InSet(member.connection.socket) && !member.connection.receiveMsgs())
                KickCoopMember(member, "connection lost");
        }
    }
    set.Clear();
    for(const CoopMember& member : coopMembers_)
    {
        if(member.connection.socket.isValid())
            set.Add(member.connection.socket);
    }
    if(set.Select(0, 2) > 0)
    {
        for(CoopMember& member : coopMembers_)
        {
            if(set.InSet(member.connection.socket))
                KickCoopMember(member, "socket error");
        }
    }
    // No handler adds or erases members (kicked ones are erased after sending), so the references stay valid
    for(CoopMember& member : coopMembers_)
    {
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
    for(CoopMember& member : coopMembers_)
    {
        if(member.connection.socket.isValid() && !member.connection.sendMsgs(10))
            KickCoopMember(member, "sending failed");
    }
    helpers::erase_if(coopMembers_, [](const CoopMember& member) { return !member.connection.socket.isValid(); });
}

void GameServer::AppendCoopMemberCmds(uint8_t leader, PlayerGameCommands& cmds) const
{
    if(state != ServerState::Game)
        return;
    const auto it = coopMemberCmds_.find(leader);
    if(it != coopMemberCmds_.end())
        cmds.gcs.insert(cmds.gcs.end(), it->second.begin(), it->second.end());
}
