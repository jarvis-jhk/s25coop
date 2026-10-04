// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

// s25coop network test harness (ROADMAP M2 step 0): one process hosts a game over localhost, others join it, all
// without video or audio, through the real GameServer and GameClient. The server compares every player's checksum at
// every network frame, so a run that reaches the requested game frame without an async report proves the clients
// stayed in lockstep. Exit codes: 0 ok, 1 setup error, 2 Lua error in the test script, 3 async, 4 connection error or
// timeout.

#include "Game.h"
#include "GameCommands.h"
#include "GameLobby.h"
#include "GameLobbyController.h"
#include "GameManager.h"
#include "GamePlayer.h"
#include "JoinPlayerInfo.h"
#include "RttrConfig.h"
#include "Settings.h"
#include "WindowManager.h"
#include "drivers/AudioDriverWrapper.h"
#include "drivers/VideoDriverWrapper.h"
#include "files.h"
#include "helpers/EnumRange.h"
#include "lua/LuaInterfaceBase.h"
#include "lua/LuaInterfaceGame.h"
#include "network/ClientError.h"
#include "network/ClientInterface.h"
#include "network/CreateServerInfo.h"
#include "network/GameClient.h"
#include "network/GameMessages.h"
#include "network/GameServer.h"
#include "network/NetworkPlayer.h"
#include "random/Random.h"
#include "world/GameWorld.h"
#include "gameTypes/BuildingQuality.h"
#include "gameTypes/GameTypesOutput.h"
#include "gameTypes/MapDescription.h"
#include "gameData/GameConsts.h"
#include "s25util/Log.h"
#include "s25util/Serializer.h"
#include "s25util/strAlgos.h"
#include <boost/filesystem.hpp>
#include <boost/nowide/args.hpp>
#include <boost/nowide/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/nowide/iostream.hpp>
#include <boost/program_options.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <optional>
#include <sstream>
#include <thread>

namespace bfs = boost::filesystem;
namespace bnw = boost::nowide;
namespace po = boost::program_options;
using namespace std::chrono_literals;

namespace {
enum ExitCode
{
    Ok = 0,
    SetupError = 1,
    LuaError = 2,
    Async = 3,
    Failed = 4
};

struct Callbacks : ClientInterface
{
    bool connected = false;
    bool loadingPending = false;
    bool started = false;
    std::optional<std::string> async;
    std::optional<ClientError> error;
    std::optional<uint8_t> extraLocalSlot;
    unsigned playersLeft = 0;

    void CI_NextConnectState(ConnectState cs) override
    {
        if(cs == ConnectState::Finished)
        {
            connected = true;
            // Install before Run can decode an immediately following game-start message; Connect clears requests.
            if(extraLocalSlot)
                GAMECLIENT.SetAdditionalLocalPlayers({*extraLocalSlot});
        }
    }
    void CI_Error(ClientError e) override
    {
        if(!error)
            error = e;
    }
    // GameLoaded must not be called from inside StartGame, which is still loading the world when this fires
    void CI_GameLoading(std::shared_ptr<Game> loaded) override
    {
        loadingPending = true;
        game = std::move(loaded);
    }
    std::shared_ptr<Game> game;
    void CI_GameStarted() override { started = true; }
    void CI_Async(const std::string& checksums) override
    {
        if(!async)
            async = checksums;
    }
    void CI_PlayerLeft(unsigned) override { playersLeft++; }
    unsigned membersChanged = 0;
    void CI_CoopMembersChanged() override { membersChanged++; }
};

struct Options
{
    bool host = false;
    uint16_t port = 0;
    unsigned maxGF = 0;
    unsigned players = 2;
    bfs::path map;
    /// Host: the game is a savegame, whose slots are fixed
    bool isSavegame = false;
    /// Host: save the game at maxGF (after the test script ran)
    bfs::path saveAt;
    std::vector<std::string> ais;
    bfs::path out;
    bfs::path waitFor;
    std::optional<unsigned> desyncAtGF;
    /// Join: request a distinct local player slot to exercise the network-session guard
    std::optional<uint8_t> extraLocalSlot;
    /// Join as a member of this player instead of taking a slot
    std::optional<uint8_t> memberOf;
    /// Host: members to wait for before starting
    unsigned members = 0;
    /// Host: allow members through the lobby message (as the GUI does) instead of on the server directly
    bool membersViaLobby = false;
    /// Host: slots after the human ones that stay free for a joiner, closed once the members are there
    unsigned openSlots = 0;
    /// Host: kick every member once they are all there, then play without them
    bool kickMembers = false;
    /// Host: once the members are there, swap our slot with this one (they must follow us)
    std::optional<uint8_t> swapWith;
    /// Host: swap at once instead, before anybody joined
    bool swapEarly = false;
    /// Join: take a slot first, then become a member of this player from the lobby
    std::optional<uint8_t> switchToMember;
    /// Join: ask to switch at once, without waiting until the host allows members
    bool switchNow = false;
    /// Join: being kicked by the host in the lobby is the expected outcome
    bool expectKick = false;
    /// Test only: authenticated member sends a forbidden host-management message in the lobby
    std::string memberHostProbe;
    /// Order a woodcutter near our HQ at this GF
    std::optional<unsigned> buildAtGF;
    /// Stop running for 2 s at this GF, as a slow machine or a hiccup on the line would
    std::optional<unsigned> stallAtGF;
    /// Log the checksum every this many GFs (0 = never), to find where two processes diverged
    unsigned traceEvery = 0;
    /// Choose these start goods for our own slot in the lobby
    std::optional<StartWares> startWares;
    /// Run in the map script's Lua state once the game started; its onTestEnd(gf) is called at maxGF
    bfs::path testScript;
    std::chrono::seconds timeout{300};
};

/// 0xFF means "none" in the protocol
void checkPlayerIndex(unsigned player)
{
    if(player >= 0xFF)
        throw po::error("a player index must be below 255");
}

AI::Info parseAI(const std::string& name)
{
    if(name == "aijh")
        return AI::Info(AI::Type::Default, AI::Level::Hard);
    if(name == "dummy")
        return AI::Info(AI::Type::Dummy);
    throw std::runtime_error("Unknown AI: " + name);
}

std::optional<StartWares> parseStartWares(const std::string& name)
{
    for(const auto value : helpers::enumRange<StartWares>())
    {
        std::stringstream s;
        s << value;
        // Printed as "StartWares::Minimal"
        std::string printed = s25util::toLower(s.str());
        printed = printed.substr(printed.rfind(':') + 1);
        if(printed == name)
            return value;
    }
    throw std::runtime_error("Unknown start goods: " + name);
}

/// What each player's HQ holds when the game starts: shows whose start goods were used
std::string describeStartGoods(const Game& game)
{
    std::string result = "Start goods:";
    for(unsigned i = 0; i < game.world_.GetNumPlayers(); i++)
    {
        const GamePlayer& player = game.world_.GetPlayer(i);
        if(!player.isUsed())
            continue;
        const Inventory& inv = player.GetInventory();
        result += " p" + std::to_string(i) + " boards " + std::to_string(inv[GoodType::Boards]) + " stones "
                  + std::to_string(inv[GoodType::Stones]) + " helpers " + std::to_string(inv[Job::Helper]) + ";";
    }
    return result;
}

/// What must be equal in every process at the end: the world checksum and the woodcutters (sites included) of each
/// player, which shows whether an order arrived
std::string describeMembers()
{
    std::string result;
    for(const CoopMemberInfo& member : GAMECLIENT.GetCoopMembers())
        result += (result.empty() ? "" : " ") + member.name + "@" + std::to_string(member.leader);
    return result.empty() ? "none" : result;
}

/// Ignore only measured ping; retain every serialized player/configuration field, including host identity.
std::vector<uint8_t> lobbyConfiguration(const GameLobby& lobby)
{
    Serializer ser;
    lobby.getSettings().Serialize(ser);
    for(auto player : lobby.getPlayers())
    {
        player.ping = 0;
        player.Serialize(ser);
    }
    return {ser.GetData(), ser.GetData() + ser.GetLength()};
}

void sendMemberHostProbe(const std::string& probe, const GameLobby& lobby)
{
    NetworkPlayer& connection = GAMECLIENT.GetMainPlayer();
    if(probe == "state")
    {
        connection.sendMsg(GameMessage_Player_State(1, PlayerState::Locked, AI::Info()));
    } else if(probe == "swap")
    {
        connection.sendMsg(GameMessage_Player_Swap(0, 1));
    } else
    {
        GlobalGameSettings settings = lobby.getSettings();
        settings.startWares = StartWares::Minimal;
        settings.lockedTeams = !settings.lockedTeams;
        connection.sendMsg(GameMessage_GGSChange(settings));
    }
}

/// What the map script recorded as campaign progress in this process (SETTINGS is per process, like on a real machine)
std::string describeCampaignProgress()
{
    std::string result;
    for(const auto& progress : SETTINGS.campaigns.createSaveData())
        result += (result.empty() ? "" : " ") + progress.first + "=" + progress.second;
    return result.empty() ? "none" : result;
}

/// Every process calls this between the same two GFs, so whatever the script changes in the world stays in sync
void callTestEnd(const Game& game, const unsigned gf)
{
    kaguya::LuaRef onTestEnd = game.world_.GetLua().getState()["onTestEnd"];
    if(onTestEnd.type() != LUA_TFUNCTION)
        throw std::runtime_error("Test script has no onTestEnd() function");
    onTestEnd.call<void>(gf);
}

std::string describeState(const Game& game)
{
    std::string result = "checksum " + std::to_string(AsyncChecksum::create(game).getHash()) + ", woodcutters";
    for(unsigned i = 0; i < game.world_.GetNumPlayers(); i++)
    {
        const BuildingCount count = game.world_.GetPlayer(i).GetBuildingRegister().GetBuildingNums();
        result +=
          " "
          + std::to_string(count.buildings[BuildingType::Woodcutter] + count.buildingSites[BuildingType::Woodcutter]);
    }
    return result;
}

/// Order a woodcutter on the first spot near our HQ where one fits
void orderWoodcutter(const Game& game)
{
    const unsigned playerId = GAMECLIENT.GetPlayerId();
    const GameWorld& world = game.world_;
    const MapPoint hq = world.GetPlayer(playerId).GetHQPos();
    const auto spots = world.GetMatchingPointsInRadius<1>(hq, 8, [&world, playerId](const MapPoint pt) {
        return canUseBq(world.GetBQ(pt, playerId), BuildingQuality::Hut);
    });
    if(spots.empty())
        throw std::runtime_error("No spot for a woodcutter near the HQ");
    GAMECLIENT.SetBuildingSite(spots.front(), BuildingType::Woodcutter);
    bnw::cout << "Ordered a woodcutter for player " << playerId << " at " << spots.front().x << "," << spots.front().y
              << std::endl;
}

void writeResult(const bfs::path& path, const std::string& text)
{
    bnw::cout << text << std::endl;
    if(!path.empty())
    {
        bnw::ofstream f(path);
        f << text << '\n';
    }
}

/// Host: configure the slots once everybody is there, start, run to maxGF; then stay until the other clients left
/// (they wait for our result file) so that the checksums of the last network frames are still compared
int run(Options& opt, Callbacks& cb)
{
    const auto startTime = std::chrono::steady_clock::now();
    std::unique_ptr<GameLobbyController> lobby;
    bool slotsSet = false, readySent = false, countdownSent = false, gameStarted = false, speedSet = false;
    std::optional<std::chrono::steady_clock::time_point> finishedAt;
    const auto numClients = static_cast<unsigned>(opt.players);
    auto nextProgress = startTime + 5s;
    unsigned maxNWFLength = 1;
    std::string stateAtMaxGF, startGoods;
    unsigned lastTracedGF = 0;
    bool switchRequested = false, switchDone = false, kicked = false, openSlotsClosed = false, swapped = false;
    bool announcedConnected = false;
    unsigned seenMemberChanges = 0;
    std::string membersAtStart, switchResult;
    std::vector<uint8_t> configurationBeforeProbe;
    bool probeSent = false, probeVerified = false;
    const bfs::path probeGate =
      opt.host ? bfs::path(opt.out.string() + ".probe-ready") : bfs::path(opt.waitFor.string() + ".probe-ready");
    const bfs::path probeReceipt =
      opt.host ? bfs::path(opt.out.string() + ".probe-sent") : bfs::path(opt.waitFor.string() + ".probe-sent");

    while(true)
    {
        if(std::chrono::steady_clock::now() - startTime > opt.timeout)
        {
            bnw::cerr << "Timeout in client state " << static_cast<int>(GAMECLIENT.GetState()) << std::endl;
            return Failed;
        }
        if(opt.host)
            GAMESERVER.Run();
        GAMECLIENT.Run();
        if(cb.error && !opt.host && !cb.connected && std::chrono::steady_clock::now() - startTime < 30s)
        {
            // The host may still be starting up
            cb.error.reset();
            GAMECLIENT.Stop();
            std::this_thread::sleep_for(200ms);
            GAMECLIENT.Connect("localhost", "", ServerType::Direct, opt.port, false, false,
                               opt.memberOf.value_or(0xFF));
            continue;
        }
        if(cb.error && opt.expectKick && cb.connected && GAMECLIENT.GetState() != ClientState::Game)
        {
            writeResult(opt.out, std::string("Kicked by the host in the lobby: ") + ClientErrorToStr(*cb.error));
            return Ok;
        }
        if(cb.error)
        {
            if(finishedAt)
                break; // Somebody leaving after the end is expected
            if(*cb.error == ClientError::LocalPlayerSetup)
                bnw::cerr << "LocalPlayerSetup: loading=" << static_cast<bool>(cb.game) << " started=" << cb.started
                          << std::endl;
            bnw::cerr << "Client error " << static_cast<int>(*cb.error) << ": " << ClientErrorToStr(*cb.error)
                      << std::endl;
            return cb.async ? Async : Failed;
        }
        if(cb.async)
        {
            bnw::cerr << "Async: " << *cb.async << std::endl;
            return Async;
        }
        const ClientState state = GAMECLIENT.GetState();
        // With --swap-early only once we are in the new slot, so a joiner asking for "the host's player" must find it
        // there
        const bool inPlace = !(opt.swapWith && opt.swapEarly) || (swapped && GAMECLIENT.GetPlayerId() == *opt.swapWith);
        if(opt.host && cb.connected && inPlace && !announcedConnected && !opt.out.empty())
        {
            // The server hands out slots in connection order and knows the host by its password, not its slot: a joiner
            // that got in before our own client would take slot 0. Joiners wait for this file (--after).
            bnw::ofstream(opt.out.string() + ".connected") << "connected\n";
            announcedConnected = true;
        }
        if(std::chrono::steady_clock::now() > nextProgress)
        {
            nextProgress += 5s;
            bnw::cout << "state " << static_cast<int>(state) << " started " << cb.started << " paused "
                      << GAMECLIENT.IsPaused() << " GF " << (state == ClientState::Game ? GAMECLIENT.GetGFNumber() : 0u)
                      << std::endl;
        }
        if(cb.membersChanged != seenMemberChanges)
        {
            seenMemberChanges = cb.membersChanged;
            bnw::cout << "Members: " << describeMembers()
                      << (GAMECLIENT.AreCoopMembersAllowed() ? "" : " (not allowed)") << std::endl;
        }
        if(state == ClientState::Config && cb.connected)
        {
            auto gameLobby = GAMECLIENT.GetGameLobby();
            if(!opt.host && !opt.memberHostProbe.empty() && !probeSent && GAMECLIENT.IsCoopMember()
               && bfs::exists(probeGate))
            {
                sendMemberHostProbe(opt.memberHostProbe, *gameLobby);
                bnw::cout << "Sent authenticated member host probe: " << opt.memberHostProbe << std::endl;
                writeResult(probeReceipt, opt.memberHostProbe);
                probeSent = true;
            }
            if(opt.switchToMember && !switchRequested && (opt.switchNow || GAMECLIENT.AreCoopMembersAllowed()))
            {
                GAMECLIENT.JoinCoopMember(*opt.switchToMember);
                switchRequested = true;
            } else if(switchRequested && !switchDone && !GAMECLIENT.IsCoopSwitchPending())
            {
                // Refused: we stay the player we were and play as one
                switchDone = true;
                switchResult = GAMECLIENT.IsCoopMember() ?
                                 "Switched to a member of player " + std::to_string(GAMECLIENT.GetPlayerId()) :
                                 "Switch refused, still player " + std::to_string(GAMECLIENT.GetPlayerId());
                bnw::cout << switchResult << std::endl;
            }
            if(opt.host && !slotsSet)
            {
                if(!opt.isSavegame && gameLobby->getNumPlayers() < numClients + opt.openSlots)
                {
                    bnw::cerr << "Map has only " << gameLobby->getNumPlayers() << " players" << std::endl;
                    return SetupError;
                }
                lobby = std::make_unique<GameLobbyController>(gameLobby, GAMECLIENT.GetMainPlayer());
                // Slot 0 is the host, the next ones wait for the joining clients (and the open slots for a joiner that
                // becomes a member), the rest get the given AIs
                // A savegame keeps the slots it was saved with
                for(unsigned i = numClients + opt.openSlots; !opt.isSavegame && i < gameLobby->getNumPlayers(); i++)
                {
                    const unsigned aiIdx = i - numClients - opt.openSlots;
                    if(aiIdx < opt.ais.size())
                        lobby->SetPlayerState(i, PlayerState::AI, parseAI(opt.ais[aiIdx]));
                    else
                        lobby->CloseSlot(i);
                }
                if(opt.membersViaLobby)
                    lobby->SetCoopMembersAllowed(true);
                if(opt.swapWith && opt.swapEarly)
                {
                    lobby->SwapPlayers(GAMECLIENT.GetPlayerId(), *opt.swapWith);
                    swapped = true;
                }
                if(opt.startWares)
                    GAMECLIENT.Command_SetStartWares(opt.startWares);
                GAMECLIENT.Command_SetReady(true);
                slotsSet = true;
            } else if(!opt.host && !readySent && !opt.memberOf
                      && (!opt.switchToMember || switchResult.find("refused") != std::string::npos))
            {
                // Sent before ready: the server handles both in order, so the host never starts without it
                if(opt.startWares)
                    GAMECLIENT.Command_SetStartWares(opt.startWares);
                GAMECLIENT.Command_SetReady(true);
                readySent = true;
            }
            if(opt.host && slotsSet && !countdownSent)
            {
                bool allThere = true;
                for(unsigned i = 0; i < numClients; i++)
                {
                    // After --swap-with the host is no longer in slot 0
                    const JoinPlayerInfo& player = gameLobby->getPlayer(i == 0 ? GAMECLIENT.GetPlayerId() : i);
                    allThere &= player.ps == PlayerState::Occupied && player.isReady;
                }
                // The members as the server counts them and as the lobby lists them (the broadcast arrived)
                const auto numListed = static_cast<unsigned>(GAMECLIENT.GetCoopMembers().size());
                if(!opt.memberHostProbe.empty())
                {
                    // Member presence alone does not prove that the host's queued slot setup was acknowledged.
                    const auto& spare = gameLobby->getPlayer(1);
                    const bool setupApplied =
                      gameLobby->getPlayer(0).isHost && spare.ps == PlayerState::AI
                      && spare.aiInfo.type == AI::Type::Dummy
                      && std::all_of(gameLobby->getPlayers().begin() + 2, gameLobby->getPlayers().end(),
                                     [](const JoinPlayerInfo& p) { return p.ps == PlayerState::Locked; });
                    if(configurationBeforeProbe.empty() && allThere && setupApplied
                       && GAMESERVER.GetNumCoopMembers() == 1 && numListed == 1)
                    {
                        configurationBeforeProbe = lobbyConfiguration(*gameLobby);
                        membersAtStart = describeMembers();
                        writeResult(probeGate, "Authenticated member and host configuration ready");
                    }
                    allThere &= !configurationBeforeProbe.empty() && bfs::exists(probeReceipt)
                                && GAMESERVER.GetNumCoopMembers() == 0 && numListed == 0;
                    if(allThere && !probeVerified)
                    {
                        if(lobbyConfiguration(*gameLobby) != configurationBeforeProbe || GAMECLIENT.GetPlayerId() != 0)
                        {
                            bnw::cerr << "Host configuration changed after member host probe" << std::endl;
                            return Failed;
                        }
                        bnw::cout << "Host configuration unchanged after member host probe: " << opt.memberHostProbe
                                  << std::endl;
                        probeVerified = true;
                    }
                } else
                {
                    allThere &= GAMESERVER.GetNumCoopMembers() >= opt.members && numListed >= opt.members;
                }
                if(allThere && opt.swapWith)
                {
                    if(!swapped)
                    {
                        lobby->SwapPlayers(GAMECLIENT.GetPlayerId(), *opt.swapWith);
                        swapped = true;
                    }
                    // Until we are in the new slot and the member list says the members followed
                    allThere = GAMECLIENT.GetPlayerId() == *opt.swapWith
                               && std::all_of(GAMECLIENT.GetCoopMembers().begin(), GAMECLIENT.GetCoopMembers().end(),
                                              [&opt](const CoopMemberInfo& m) { return m.leader == *opt.swapWith; });
                }
                if(allThere && membersAtStart.empty())
                    membersAtStart = describeMembers();
                if(allThere && opt.kickMembers)
                {
                    if(!kicked)
                    {
                        for(const CoopMemberInfo& member : GAMECLIENT.GetCoopMembers())
                            lobby->KickCoopMember(member.id);
                        kicked = true;
                    }
                    allThere = false;
                } else if(kicked)
                    allThere = numListed == 0 && GAMESERVER.GetNumCoopMembers() == 0;
                if(allThere && opt.openSlots)
                {
                    for(unsigned i = numClients; i < numClients + opt.openSlots; i++)
                    {
                        const PlayerState ps = gameLobby->getPlayer(i).ps;
                        if(ps == PlayerState::Free && !openSlotsClosed)
                            lobby->CloseSlot(i);
                        allThere &= ps == PlayerState::Locked;
                    }
                    openSlotsClosed = true;
                }
                if(allThere)
                {
                    lobby->StartCountdown(0);
                    countdownSent = true;
                }
            }
        } else if(state == ClientState::Loading && cb.loadingPending)
        {
            cb.loadingPending = false;
            GAMECLIENT.GameLoaded();
        } else if(state == ClientState::Game)
        {
            if(!gameStarted)
            {
                // What the game interface does when it becomes active: runs the map script's start and unpauses
                GAMECLIENT.OnGameStart();
                gameStarted = true;
                startGoods = describeStartGoods(*cb.game);
                bnw::cout << startGoods << std::endl;
                if(!opt.testScript.empty())
                {
                    if(!cb.game->world_.HasLua())
                    {
                        bnw::cerr << "--test-script needs a map script" << std::endl;
                        return SetupError;
                    }
                    bool loaded = false;
                    try
                    {
                        loaded = cb.game->world_.GetLua().getState().dofile(opt.testScript.string());
                    } catch(const LuaExecutionError& e)
                    {
                        bnw::cerr << "Lua error: " << e.what() << std::endl;
                    }
                    if(!loaded)
                    {
                        bnw::cerr << "Failed to load the test script " << opt.testScript << std::endl;
                        return LuaError;
                    }
                }
            }
            if(opt.host && !speedSet)
            {
                GAMECLIENT.SetNewSpeed(MAX_SPEED);
                // A map script's mission statement pauses the start until its window is closed; nobody reads it here
                GAMECLIENT.SetPause(false);
                speedSet = true;
            }
            const unsigned gf = GAMECLIENT.GetGFNumber();
            // One GF per Run(), so every GF is seen here
            if(opt.buildAtGF && gf >= *opt.buildAtGF)
            {
                orderWoodcutter(*cb.game);
                opt.buildAtGF.reset();
            }
            if(opt.stallAtGF && gf >= *opt.stallAtGF)
            {
                std::this_thread::sleep_for(2s);
                opt.stallAtGF.reset();
            }
            if(opt.traceEvery && gf % opt.traceEvery == 0 && gf != lastTracedGF)
            {
                bnw::cout << "Trace GF " << gf << ": " << AsyncChecksum::create(*cb.game) << std::endl;
                lastTracedGF = gf;
            }
            if(gf == opt.maxGF && stateAtMaxGF.empty())
            {
                if(!opt.testScript.empty())
                {
                    try
                    {
                        callTestEnd(*cb.game, gf);
                    } catch(const std::exception& e) // LuaExecutionError, or no onTestEnd at all
                    {
                        bnw::cerr << "Test script failed: " << e.what() << std::endl;
                        return LuaError;
                    }
                }
                stateAtMaxGF = "State at GF " + std::to_string(gf) + ": " + describeState(*cb.game);
                if(!opt.testScript.empty())
                    stateAtMaxGF += "\nCampaign progress: " + describeCampaignProgress();
                if(!opt.saveAt.empty() && !GAMECLIENT.SaveToFile(opt.saveAt))
                {
                    bnw::cerr << "Could not save the game to " << opt.saveAt << std::endl;
                    return Failed;
                }
            }
            maxNWFLength = std::max(maxNWFLength, GAMECLIENT.GetNWFLength());
            if(opt.desyncAtGF && gf >= *opt.desyncAtGF)
            {
                // Only in this process: its world diverges, which the server must notice
                RANDOM.Init(4711);
                opt.desyncAtGF.reset();
            }
            // The checksum a client takes at GF g travels with its commands for cmdDelay NWFs later, and the server
            // compares all of them before it releases that NWF. So once our GF is that far past maxGF, every checksum
            // up to maxGF was compared, and an async would have reached us before the release that let us get here.
            const unsigned checkedGF = gf - std::min(gf, (GAMECLIENT.GetNWFInfo()->getCmdDelay() + 1) * maxNWFLength);
            if(checkedGF >= opt.maxGF && !finishedAt)
            {
                std::string result = "Reached GF ";
                result += std::to_string(gf);
                result += " in sync (checksums compared through GF ";
                result += std::to_string(checkedGF);
                result += ")\n";
                result += stateAtMaxGF;
                result += "\n" + startGoods;
                if(opt.host)
                {
                    result += "\nMembers at start: ";
                    result += membersAtStart;
                    result += "\nMembers at the end: ";
                    result += describeMembers();
                }
                if(!switchResult.empty())
                {
                    result += '\n';
                    result += switchResult;
                }
                writeResult(opt.out, result);
                finishedAt = std::chrono::steady_clock::now();
            }
            if(finishedAt)
            {
                if(opt.host
                   && ((cb.playersLeft + 1 >= numClients && GAMESERVER.GetNumCoopMembers() == 0)
                       || std::chrono::steady_clock::now() - *finishedAt > 30s))
                    break;
                if(!opt.host && (opt.waitFor.empty() || bfs::exists(opt.waitFor)))
                    break;
            }
        } else if(state == ClientState::Stopped && finishedAt)
            break;
        std::this_thread::sleep_for(1ms);
    }
    return cb.async ? Async : Ok;
}
} // namespace

int main(int argc, char** argv)
{
    bnw::nowide_filesystem();
    bnw::args _(argc, argv);

    Options opt;
    std::string mode;
    unsigned timeoutSec = 300;
    po::options_description desc("Allowed options");
    // clang-format off
    desc.add_options()
        ("help,h", "Show help")
        ("mode", po::value(&mode)->required(), "host or join")
        ("port", po::value(&opt.port)->required(), "Port of the game server")
        ("maxGF", po::value(&opt.maxGF)->required(), "Game frame to play to")
        ("players", po::value(&opt.players), "Host: number of human players, host included (default 2)")
        ("map", po::value<std::string>(), "Host: map file")
        ("lua", po::value<std::string>(), "Host: map script (a campaign mission's .lua); default: the .lua beside the map, if any")
        ("ai", po::value(&opt.ais)->multitoken(), "Host: AI (aijh, dummy) for the slots after the human ones; others are closed")
        ("out", po::value<std::string>(), "Write the result line to this file as well")
        ("wait-for", po::value<std::string>(), "Join: after maxGF keep running until this file exists (the host's --out)")
        ("desync-at", po::value<unsigned>(), "Test the harness: diverge this process's world at this GF")
        ("member-of", po::value<unsigned>()->notifier(checkPlayerIndex), "Join: control this player together with its client instead of taking a slot")
        ("extra-local-slot", po::value<unsigned>()->notifier(checkPlayerIndex), "Join: request an additional local player slot (must be rejected in network games)")
        ("members", po::value(&opt.members), "Host: wait for this many members before starting")
        ("members-via-lobby", po::bool_switch(&opt.membersViaLobby), "Host: allow members through the lobby message, as the GUI does")
        ("open-slots", po::value(&opt.openSlots), "Host: leave this many slots after the human ones free, close them once the members are there")
        ("kick-members", po::bool_switch(&opt.kickMembers), "Host: kick every member once they are there, then play without them")
        ("swap-with", po::value<unsigned>()->notifier(checkPlayerIndex), "Host: once the members are there, swap our slot with this one")
        ("swap-early", po::bool_switch(&opt.swapEarly), "Host: do the --swap-with at once, before anybody joined")
        ("member-of-host", "Join: as a member of whichever player the host plays (what the Join Game window does)")
        ("switch-to-member", po::value<unsigned>()->notifier(checkPlayerIndex), "Join: take a slot, then become a member of this player from the lobby")
        ("switch-now", po::bool_switch(&opt.switchNow), "Join: ask to switch without waiting until members are allowed")
        ("member-host-probe", po::value(&opt.memberHostProbe), "Test: member sends state, swap or settings; host waits for its removal before starting")
        ("expect-kick", po::bool_switch(&opt.expectKick), "Join: being kicked in the lobby is the expected outcome (exit 0)")
        ("build-at", po::value<unsigned>(), "Order a woodcutter near our HQ at this GF")
        ("stall-at", po::value<unsigned>(), "Stop running for 2 s at this GF")
        ("trace", po::value(&opt.traceEvery), "Log the checksum every this many GFs")
        ("start-wares", po::value<std::string>(), "Choose these start goods for our own slot (vlow, low, normal, alot, minimal, minimalplus)")
        ("savegame", po::value<std::string>(), "Host: continue this savegame instead of starting --map")
        ("save", po::value<std::string>(), "Host: save the game to this file at maxGF")
        ("test-script", po::value<std::string>(), "Run this in the map script's Lua state; its onTestEnd(gf) is called at maxGF, then the campaign progress is reported")
        ("connect-delay", po::value<unsigned>(), "Join: wait this many seconds before connecting")
        ("after", po::value<std::string>(), "Join: wait until this file exists before connecting (the host's --out plus .connected)")
        ("timeout", po::value(&timeoutSec), "Give up after this many seconds (default 300)")
        ("log", po::value<std::string>(), "Write standard output (the game's log) to this file")
        ;
    // clang-format on
    po::positional_options_description positional;
    positional.add("mode", 1);
    po::variables_map options;
    try
    {
        po::store(po::command_line_parser(argc, argv).options(desc).positional(positional).run(), options);
        if(options.count("help"))
        {
            bnw::cout << desc << std::endl;
            return Ok;
        }
        po::notify(options);
    } catch(const std::exception& e)
    {
        bnw::cerr << "Error: " << e.what() << "\n\n" << desc << std::endl;
        return SetupError;
    }
    opt.host = mode == "host";
    if(!opt.host && mode != "join")
    {
        bnw::cerr << "Mode must be host or join" << std::endl;
        return SetupError;
    }
    if(opt.host && options.count("map") == options.count("savegame"))
    {
        bnw::cerr << "host needs either --map or --savegame" << std::endl;
        return SetupError;
    }
    if(!opt.memberHostProbe.empty()
       && ((opt.memberHostProbe != "state" && opt.memberHostProbe != "swap" && opt.memberHostProbe != "settings")
           || (opt.host && (opt.members != 1 || opt.players != 1 || opt.ais != std::vector<std::string>{"dummy"}))
           || (!opt.host && !options.count("member-of"))
           || (opt.host ? !options.count("out") : !options.count("wait-for"))))
    {
        bnw::cerr << "--member-host-probe needs state/swap/settings and host --players 1 --members 1 --ai dummy --out,"
                     " or join --member-of --wait-for"
                  << std::endl;
        return SetupError;
    }
    // Absolute now: RTTRCONFIG.Init changes the working directory
    if(options.count("out"))
        opt.out = bfs::absolute(options["out"].as<std::string>());
    if(options.count("wait-for"))
        opt.waitFor = bfs::absolute(options["wait-for"].as<std::string>());
    const bfs::path afterFile =
      options.count("after") ? bfs::absolute(options["after"].as<std::string>()) : bfs::path();
    if(options.count("log")
       && !std::freopen(bfs::absolute(options["log"].as<std::string>()).string().c_str(), "w", stdout))
    {
        bnw::cerr << "Cannot write the log file" << std::endl;
        return SetupError;
    }
    const bfs::path mapArg = options.count("map") ? bfs::absolute(options["map"].as<std::string>()) : bfs::path();
    const bfs::path luaArg = options.count("lua") ? bfs::absolute(options["lua"].as<std::string>()) : bfs::path();
    opt.isSavegame = options.count("savegame") > 0;
    const bfs::path savegameArg = opt.isSavegame ? bfs::absolute(options["savegame"].as<std::string>()) : bfs::path();
    if(options.count("save"))
        opt.saveAt = bfs::absolute(options["save"].as<std::string>());
    if(options.count("test-script"))
        opt.testScript = bfs::absolute(options["test-script"].as<std::string>());
    if(options.count("desync-at"))
        opt.desyncAtGF = options["desync-at"].as<unsigned>();
    if(options.count("member-of"))
        opt.memberOf = static_cast<uint8_t>(options["member-of"].as<unsigned>());
    if(options.count("extra-local-slot"))
    {
        if(opt.host)
        {
            bnw::cerr << "--extra-local-slot requires join mode" << std::endl;
            return SetupError;
        }
        opt.extraLocalSlot = static_cast<uint8_t>(options["extra-local-slot"].as<unsigned>());
    }
    if(options.count("member-of-host"))
        opt.memberOf = COOP_LEADER_HOST;
    if(options.count("switch-to-member"))
        opt.switchToMember = static_cast<uint8_t>(options["switch-to-member"].as<unsigned>());
    if(options.count("swap-with"))
        opt.swapWith = static_cast<uint8_t>(options["swap-with"].as<unsigned>());
    if(options.count("stall-at"))
        opt.stallAtGF = options["stall-at"].as<unsigned>();
    if(options.count("build-at"))
        opt.buildAtGF = options["build-at"].as<unsigned>();
    try
    {
        if(options.count("start-wares"))
            opt.startWares = parseStartWares(options["start-wares"].as<std::string>());
    } catch(const std::exception& e)
    {
        bnw::cerr << e.what() << std::endl;
        return SetupError;
    }
    opt.timeout = std::chrono::seconds(timeoutSec);
    try
    {
        for(const auto& ai : opt.ais)
            parseAI(ai);
    } catch(const std::exception& e)
    {
        bnw::cerr << e.what() << std::endl;
        return SetupError;
    }

    try
    {
        RTTRCONFIG.Init();
        const bfs::path logDir = RTTRCONFIG.ExpandPath(s25::folders::logs);
        bfs::create_directories(logDir);
        LOG.setLogFilepath(logDir);
        bfs::create_directories(RTTRCONFIG.ExpandPath(s25::folders::mapsPlayed));
        SETTINGS.lobby.name = opt.host ? "Host" : "Client";
        SETTINGS.interface.autosaveInterval = 0;
        // The client reports some things to the game manager (average GF/s, back to the menu on errors)
        GameManager gameManager(LOG, SETTINGS, VIDEODRIVER, AUDIODRIVER, WINDOWMANAGER);
        setGlobalGameManager(&gameManager);

        Callbacks cb;
        cb.extraLocalSlot = opt.extraLocalSlot;
        GAMECLIENT.SetInterface(&cb);
        if(opt.host)
        {
            const CreateServerInfo csi(ServerType::Direct, opt.port, "s25coop net test");
            const auto makeMap = [&]() {
                if(opt.isSavegame)
                    return MapDescription(savegameArg, MapType::Savegame);
                if(luaArg.empty())
                    return MapDescription(mapArg, MapType::OldMap);
                return MapDescription(mapArg, MapType::OldMap, luaArg);
            };
            const MapDescription map = makeMap();
            if(!GAMECLIENT.HostGame(csi, map))
            {
                bnw::cerr << "Could not host on port " << opt.port << std::endl;
                return SetupError;
            }
            if(!opt.membersViaLobby)
                GAMESERVER.SetAllowCoopMembers(opt.members > 0);
        } else
        {
            if(!afterFile.empty())
            {
                const auto start = std::chrono::steady_clock::now();
                while(!bfs::exists(afterFile))
                {
                    if(std::chrono::steady_clock::now() - start > 60s)
                    {
                        bnw::cerr << "The host did not connect: " << afterFile << std::endl;
                        return Failed;
                    }
                    std::this_thread::sleep_for(50ms);
                }
            }
            if(options.count("connect-delay"))
                std::this_thread::sleep_for(std::chrono::seconds(options["connect-delay"].as<unsigned>()));
            // The host may still be starting: retry for a while
            const auto start = std::chrono::steady_clock::now();
            while(!GAMECLIENT.Connect("localhost", "", ServerType::Direct, opt.port, false, false,
                                      opt.memberOf.value_or(0xFF)))
            {
                if(std::chrono::steady_clock::now() - start > 30s)
                {
                    bnw::cerr << "Could not connect to port " << opt.port << std::endl;
                    return Failed;
                }
                std::this_thread::sleep_for(200ms);
            }
        }
        const int result = run(opt, cb);
        GAMECLIENT.Stop();
        GAMESERVER.Stop();
        GAMECLIENT.RemoveInterface(&cb);
        setGlobalGameManager(nullptr);
        return result;
    } catch(const std::exception& e)
    {
        bnw::cerr << "Error: " << e.what() << std::endl;
        return Failed;
    }
}
