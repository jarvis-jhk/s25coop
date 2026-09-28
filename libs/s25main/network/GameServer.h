// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "FramesInfo.h"
#include "GameMessageInterface.h"
#include "GameProtocol.h"
#include "GlobalGameSettings.h"
#include "JoinPlayerInfo.h"
#include "NWFInfo.h"
#include "gameTypes/MapDescription.h"
#include "gameTypes/MapInfo.h"
#include "gameTypes/ServerType.h"
#include "liblobby/LobbyInterface.h"
#include "s25util/LANDiscoveryService.h"
#include "s25util/Singleton.h"
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct CreateServerInfo;
class GameMessage;
class GameMessageWithPlayer;
class GameMessage_GameCommand;
class GameServerPlayer;
struct AIServerPlayer;
class CoopMemberHandler;

class GameServer :
    public Singleton<GameServer, SingletonPolicies::WithLongevity>,
    public GameMessageInterface,
    public LobbyInterface
{
public:
    static constexpr unsigned Longevity = 6;
    using SteadyClock = std::chrono::steady_clock;

    GameServer();
    ~GameServer();

    /// Starts the server
    bool Start(const CreateServerInfo& csi, const MapDescription& map, const std::string& hostPw);

    void Run();

    void RunStateGame();

    void RunStateConfig();

    void Stop();

    /// s25coop: number of members that completed joining a player
    unsigned GetNumCoopMembers() const;
    /// s25coop: accept connections that ask to control a human player together with it. Off by default: a member
    /// gives orders for somebody else's player, so the host has to want that
    void SetAllowCoopMembers(bool allow);

    /// Assign players that do not have a fixed team, return true if any player was assigned.
    static bool assignPlayersOfRandomTeams(std::vector<JoinPlayerInfo>& playerInfos);

private:
    friend class CoopMemberHandler;

    bool StartGame();

    unsigned CalcNWFLength(std::chrono::milliseconds minDuration) const;

    GameServerPlayer* GetNetworkPlayer(unsigned playerId);
    /// Swap players ingame or during config
    void SwapPlayer(uint8_t player1, uint8_t player2);

    void SendToAll(const GameMessage& msg);
    void SendNWFDone(const NWFServerInfo& info);

    /// Kick a player (free slot and set socket to invalid. Does NOT remove it from NetworkPlayers)
    void KickPlayer(uint8_t playerId, KickReason cause, uint32_t param);

    void ClientWatchDog();

    void WaitForClients();
    void FillPlayerQueues();

    unsigned GetNumFilledSlots() const;
    /// Notifies listeners (e.g. Lobby) that the game status has changed (e.g player count)
    void AnnounceStatusChange();
    void SetPaused(bool paused);

    void LC_Status_Error(const std::string& error) override;
    void LC_Created() override;

    RTTR_IGNORE_OVERLOADED_VIRTUAL
    bool OnGameMessage(const GameMessage_Pong& msg) override;
    bool OnGameMessage(const GameMessage_Server_Type& msg) override;
    bool OnGameMessage(const GameMessage_Server_Password& msg) override;
    bool OnGameMessage(const GameMessage_Chat& msg) override;
    bool OnGameMessage(const GameMessage_GGSChange& msg) override;
    bool OnGameMessage(const GameMessage_Player_State& msg) override;
    bool OnGameMessage(const GameMessage_Player_Name& msg) override;
    bool OnGameMessage(const GameMessage_Player_Portrait& msg) override;
    bool OnGameMessage(const GameMessage_Player_Nation& msg) override;
    bool OnGameMessage(const GameMessage_Player_Team& msg) override;
    bool OnGameMessage(const GameMessage_Player_Color& msg) override;
    bool OnGameMessage(const GameMessage_Player_Ready& msg) override;
    bool OnGameMessage(const GameMessage_Player_Swap& msg) override;
    bool OnGameMessage(const GameMessage_Player_SwapConfirm& msg) override;
    bool OnGameMessage(const GameMessage_MapRequest& msg) override;
    bool OnGameMessage(const GameMessage_Map_Checksum& msg) override;
    bool OnGameMessage(const GameMessage_GameCommand& msg) override;
    bool OnGameMessage(const GameMessage_Speed& msg) override;
    bool OnGameMessage(const GameMessage_AsyncLog& msg) override;
    bool OnGameMessage(const GameMessage_RemoveLua& msg) override;
    bool OnGameMessage(const GameMessage_Countdown& msg) override;
    bool OnGameMessage(const GameMessage_CancelCountdown& msg) override;
    bool OnGameMessage(const GameMessage_Pause& msg) override;
    bool OnGameMessage(const GameMessage_SkipToGF& msg) override;
    bool OnGameMessage(const GameMessage_Coop_JoinMember& msg) override;
    bool OnGameMessage(const GameMessage_Coop_AllowMembers& msg) override;
    bool OnGameMessage(const GameMessage_Coop_KickMember& msg) override;
    RTTR_POP_DIAGNOSTIC

    /// Send the map info (requestInfo) or the map and lua data. False if the data was requested twice
    bool SendMap(GameServerPlayer& player, bool requestInfo);

    // s25coop members: connections that play an existing world player together with it. They are not in
    // playerInfos, networkPlayers or nwfInfo; their game commands are merged into their leader's (GameServerCoop.cpp)
    struct CoopMember;
    /// Connections that are or want to become members; more are refused like players on a full server
    static constexpr unsigned maxCoopMembers = 16;
    /// Member orders buffered for one leader until its next command set
    static constexpr unsigned maxCoopMemberCmds = 1000;
    /// Accept a connection for which there is no free slot: it may only become a member. False if members are not
    /// allowed or too many connections are waiting
    bool AcceptCoopMember(const Socket& socket);
    /// Answer a member's join request for the given leader; closes the connection if refused
    void JoinCoopMember(CoopMember& member, uint8_t leader);
    /// A player in the lobby asks to become a member of leader instead: its slot is freed, its connection kept
    void SwitchToCoopMember(GameServerPlayer& player, uint8_t leader);
    /// Whether leader is a player a member may join
    bool CanJoinCoopMember(uint8_t leader) const;
    /// Tell everybody who is a member of whom, if that (or who is listening) changed since the last time
    void BroadcastCoopMembers();
    void ReceiveCoopMemberMsgs();
    void SendCoopMemberMsgs();
    void KickCoopMember(CoopMember& member, const char* reason);
    /// Close the connections of all members of this player
    void KickCoopMembersOf(uint8_t leader);
    /// Append the member commands that arrived since the last seal to the commands of their leader
    void AppendCoopMemberCmds(uint8_t leader, PlayerGameCommands& cmds) const;

    void CancelCountdown();
    bool ArePlayersReady() const;
    /// Some player data has changed. Set non-ready and cancel countdown
    void PlayerDataChanged(unsigned playerIdx);

    /// Sets the color of this player to the given color.
    /// If ensureUnique is true and the color is already used it will be set to the next free one
    /// Sends a notification to all players if the color was changed
    void CheckAndSetColor(unsigned playerIdx, unsigned newColor, bool ensureUnique);

    /// Handles advancing of GFs, actions of AI and potentially the NWF
    void ExecuteGameFrame();
    void ExecuteNWF();

    bool CheckForAsync();
    boost::filesystem::path SaveAsyncLog();
    void SendAsyncLog(const boost::filesystem::path& asyncLogFilePath);

    void CheckAndKickLaggingPlayers();
    bool CheckForLaggingPlayers();
    JoinPlayerInfo& GetJoinPlayer(unsigned playerIdx);

    /// Is the player with the given idx the host?
    bool IsHost(unsigned playerIdx) const;
    /// Get the player this message concerns. which is msg.player, msg.senderPlayer or -1 on error/wrong values
    int GetTargetPlayer(const GameMessageWithPlayer& msg);

    unsigned skiptogf;

    enum class ServerState
    {
        Stopped,
        Config,
        Loading,
        Game
    } state;

    FramesInfo framesinfo;
    unsigned currentGF;

    struct ServerConfig
    {
        ServerConfig();
        void Clear();

        ServerType servertype;
        std::string gamename;
        std::string hostPassword, password;
        unsigned short port;
        bool ipv6;
    } config;

    MapInfo mapinfo;

    Socket serversocket;
    std::vector<JoinPlayerInfo> playerInfos;
    std::vector<GameServerPlayer> networkPlayers;
    /// Owned by pointer: never moved, so references held while handling a member's messages stay valid
    std::vector<std::unique_ptr<CoopMember>> coopMembers_;
    /// Commands of members per leader, in arrival order, not yet sealed into a leader's command set
    std::map<uint8_t, std::vector<gc::GameCommandPtr>> coopMemberCmds_;
    bool allowCoopMembers_ = false;
    /// The member list, the allowed flag or the set of connections that must hear about them changed
    bool coopMembersChanged_ = false;
    uint32_t nextCoopMemberId_ = 1;
    NWFInfo nwfInfo;
    GlobalGameSettings ggs_;

    /// der Spielstartcountdown
    class CountDown
    {
        bool isActive;
        unsigned remainingSecs;
        std::chrono::steady_clock::time_point lasttime;

    public:
        CountDown();
        /// Starts a countdown at curTime of timeInSec seconds
        void Start(unsigned timeInSec);
        void Stop();
        /// Updates the state and returns true on change. Stops 1s after remainingSecs reached zero
        bool Update();
        bool IsActive() const { return isActive; }
        unsigned GetRemainingSecs() const { return remainingSecs; }
    } countdown;

    struct AsyncLog;
    /// AsyncLogs of all players
    std::vector<AsyncLog> asyncLogs;
    /// Time at which the loading started
    std::chrono::steady_clock::time_point loadStartTime;

    LANDiscoveryService lanAnnouncer;
    void RunStateLoading();
};

///////////////////////////////////////////////////////////////////////////////
// Makros / Defines
#define GAMESERVER GameServer::inst()
