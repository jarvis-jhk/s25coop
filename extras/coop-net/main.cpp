// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

// s25coop network test harness (ROADMAP M2 step 0): one process hosts a game over localhost, others join it, all
// without video or audio, through the real GameServer and GameClient. The server compares every player's checksum at
// every network frame, so a run that reaches the requested game frame without an async report proves the clients
// stayed in lockstep. Exit codes: 0 ok, 1 setup error, 3 async, 4 connection error or timeout.

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
#include "network/ClientInterface.h"
#include "network/CreateServerInfo.h"
#include "network/GameClient.h"
#include "network/GameServer.h"
#include "random/Random.h"
#include "world/GameWorld.h"
#include "gameTypes/BuildingQuality.h"
#include "gameTypes/MapDescription.h"
#include "gameData/GameConsts.h"
#include "s25util/Log.h"
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
    unsigned playersLeft = 0;

    void CI_NextConnectState(ConnectState cs) override { connected |= (cs == ConnectState::Finished); }
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
};

struct Options
{
    bool host = false;
    uint16_t port = 0;
    unsigned maxGF = 0;
    unsigned players = 2;
    bfs::path map;
    std::vector<std::string> ais;
    bfs::path out;
    bfs::path waitFor;
    std::optional<unsigned> desyncAtGF;
    /// Join as a member of this player instead of taking a slot
    std::optional<uint8_t> memberOf;
    /// Host: members to wait for before starting
    unsigned members = 0;
    /// Order a woodcutter near our HQ at this GF
    std::optional<unsigned> buildAtGF;
    /// Stop running for 2 s at this GF, as a slow machine or a hiccup on the line would
    std::optional<unsigned> stallAtGF;
    /// Log the checksum every this many GFs (0 = never), to find where two processes diverged
    unsigned traceEvery = 0;
    std::chrono::seconds timeout{300};
};

AI::Info parseAI(const std::string& name)
{
    if(name == "aijh")
        return AI::Info(AI::Type::Default, AI::Level::Hard);
    if(name == "dummy")
        return AI::Info(AI::Type::Dummy);
    throw std::runtime_error("Unknown AI: " + name);
}

/// What must be equal in every process at the end: the world checksum and the woodcutters (sites included) of each
/// player, which shows whether an order arrived
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
    std::string stateAtMaxGF;
    unsigned lastTracedGF = 0;

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
        if(cb.error)
        {
            if(finishedAt)
                break; // Somebody leaving after the end is expected
            bnw::cerr << "Client error " << static_cast<int>(*cb.error) << std::endl;
            return cb.async ? Async : Failed;
        }
        if(cb.async)
        {
            bnw::cerr << "Async: " << *cb.async << std::endl;
            return Async;
        }
        const ClientState state = GAMECLIENT.GetState();
        if(std::chrono::steady_clock::now() > nextProgress)
        {
            nextProgress += 5s;
            bnw::cout << "state " << static_cast<int>(state) << " started " << cb.started << " paused "
                      << GAMECLIENT.IsPaused() << " GF " << (state == ClientState::Game ? GAMECLIENT.GetGFNumber() : 0u)
                      << std::endl;
        }
        if(state == ClientState::Config && cb.connected)
        {
            auto gameLobby = GAMECLIENT.GetGameLobby();
            if(opt.host && !slotsSet)
            {
                if(gameLobby->getNumPlayers() < numClients)
                {
                    bnw::cerr << "Map has only " << gameLobby->getNumPlayers() << " players" << std::endl;
                    return SetupError;
                }
                lobby = std::make_unique<GameLobbyController>(gameLobby, GAMECLIENT.GetMainPlayer());
                // Slot 0 is the host, the next ones wait for the joining clients, the rest get the given AIs
                for(unsigned i = numClients; i < gameLobby->getNumPlayers(); i++)
                {
                    const unsigned aiIdx = i - numClients;
                    if(aiIdx < opt.ais.size())
                        lobby->SetPlayerState(i, PlayerState::AI, parseAI(opt.ais[aiIdx]));
                    else
                        lobby->CloseSlot(i);
                }
                GAMECLIENT.Command_SetReady(true);
                slotsSet = true;
            } else if(!opt.host && !readySent && !opt.memberOf)
            {
                GAMECLIENT.Command_SetReady(true);
                readySent = true;
            }
            if(opt.host && slotsSet && !countdownSent)
            {
                bool allThere = true;
                for(unsigned i = 0; i < numClients; i++)
                {
                    const JoinPlayerInfo& player = gameLobby->getPlayer(i);
                    allThere &= player.ps == PlayerState::Occupied && player.isReady;
                }
                allThere &= GAMESERVER.GetNumCoopMembers() >= opt.members;
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
                stateAtMaxGF = "State at GF " + std::to_string(gf) + ": " + describeState(*cb.game);
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
                writeResult(opt.out, "Reached GF " + std::to_string(gf) + " in sync (checksums compared through GF "
                                       + std::to_string(checkedGF) + ")\n" + stateAtMaxGF);
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
        ("ai", po::value(&opt.ais)->multitoken(), "Host: AI (aijh, dummy) for the slots after the human ones; others are closed")
        ("out", po::value<std::string>(), "Write the result line to this file as well")
        ("wait-for", po::value<std::string>(), "Join: after maxGF keep running until this file exists (the host's --out)")
        ("desync-at", po::value<unsigned>(), "Test the harness: diverge this process's world at this GF")
        ("member-of", po::value<unsigned>(), "Join: control this player together with its client instead of taking a slot")
        ("members", po::value(&opt.members), "Host: wait for this many members before starting")
        ("build-at", po::value<unsigned>(), "Order a woodcutter near our HQ at this GF")
        ("stall-at", po::value<unsigned>(), "Stop running for 2 s at this GF")
        ("trace", po::value(&opt.traceEvery), "Log the checksum every this many GFs")
        ("connect-delay", po::value<unsigned>(), "Join: wait this many seconds before connecting")
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
    if(opt.host && !options.count("map"))
    {
        bnw::cerr << "host needs --map" << std::endl;
        return SetupError;
    }
    // Absolute now: RTTRCONFIG.Init changes the working directory
    if(options.count("out"))
        opt.out = bfs::absolute(options["out"].as<std::string>());
    if(options.count("wait-for"))
        opt.waitFor = bfs::absolute(options["wait-for"].as<std::string>());
    if(options.count("log")
       && !std::freopen(bfs::absolute(options["log"].as<std::string>()).string().c_str(), "w", stdout))
    {
        bnw::cerr << "Cannot write the log file" << std::endl;
        return SetupError;
    }
    const bfs::path mapArg = options.count("map") ? bfs::absolute(options["map"].as<std::string>()) : bfs::path();
    if(options.count("desync-at"))
        opt.desyncAtGF = options["desync-at"].as<unsigned>();
    if(options.count("member-of"))
        opt.memberOf = static_cast<uint8_t>(options["member-of"].as<unsigned>());
    if(options.count("stall-at"))
        opt.stallAtGF = options["stall-at"].as<unsigned>();
    if(options.count("build-at"))
        opt.buildAtGF = options["build-at"].as<unsigned>();
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
        GAMECLIENT.SetInterface(&cb);
        if(opt.host)
        {
            const CreateServerInfo csi(ServerType::Direct, opt.port, "s25coop net test");
            if(!GAMECLIENT.HostGame(csi, MapDescription(mapArg, MapType::OldMap)))
            {
                bnw::cerr << "Could not host on port " << opt.port << std::endl;
                return SetupError;
            }
            GAMESERVER.SetAllowCoopMembers(opt.members > 0);
        } else
        {
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
