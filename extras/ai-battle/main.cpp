// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GlobalGameSettings.h"
#include "HeadlessGame.h"
#include "QuickStartGame.h"
#include "RTTR_Version.h"
#include "Replay.h"
#include "RttrConfig.h"
#include "Settings.h"
#include "addons/Addon.h"
#include "addons/AddonBool.h"
#include "addons/AddonList.h"
#include "addons/const_addons.h"
#include "ai/random.h"
#include "files.h"
#include "lua/LuaInterfaceBase.h"
#include "random/Random.h"
#include "gameTypes/MapInfo.h"
#include "s25util/Log.h"
#include "s25util/StringConversion.h"
#include "s25util/System.h"

#include <boost/filesystem.hpp>
#include <boost/nowide/args.hpp>
#include <boost/nowide/filesystem.hpp>
#include <boost/nowide/iostream.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <iomanip>
#if BOOST_VERSION >= 109000
#    include <optional>
using std::optional;
#else
#    include <boost/optional.hpp>
using boost::optional;
#endif

namespace bnw = boost::nowide;
namespace bfs = boost::filesystem;
namespace po = boost::program_options;

static void loadAddonsFromIni(GlobalGameSettings& ggs, const bfs::path& iniPath)
{
    if(!bfs::exists(iniPath))
        throw std::runtime_error("Settings file not found: " + iniPath.string());

    boost::property_tree::ptree tree;
    boost::property_tree::read_ini(iniPath.string(), tree);

    const auto addons = tree.get_child_optional("addons");
    if(!addons)
    {
        bnw::cout << "Note: no [addons] section in " << iniPath << ", using defaults.\n";
        return;
    }

    unsigned loaded = 0;
    for(const auto& entry : *addons)
    {
        try
        {
            const auto id = static_cast<AddonId>(s25util::fromStringClassic<unsigned>(entry.first));
            const auto v = entry.second.get_value<unsigned>();
            ggs.setSelection(id, v);
            ++loaded;
        } catch(const std::exception&)
        {
            // Unknown or invalid entry - skip silently
        }
    }
    bnw::cout << "Loaded " << loaded << " addon settings from " << iniPath << '\n';
}

/// Replays a replay without AIs and compares the checksums recorded with its commands against the replayed game.
/// Coop and network games rely on every machine computing the same game from the same commands; a replay that does
/// not replay in sync means that is broken. Exit codes: 0 in sync, 3 async, 1 unusable replay, 2 Lua error.
static int checkReplay(const bfs::path& path, optional<unsigned> seedOverride)
{
    Replay replay;
    MapInfo mapInfo;
    if(!replay.LoadHeader(path) || !replay.LoadGameData(mapInfo))
    {
        bnw::cerr << "Invalid replay " << path << ": " << replay.GetLastErrorMsg() << std::endl;
        return 1;
    }
    if(mapInfo.type != MapType::OldMap)
    {
        bnw::cerr << "Only replays that start from a map can be checked, not from a savegame" << std::endl;
        return 1;
    }

    // The map and its script travel inside the replay, as they do for a network game
    struct TmpDir
    {
        bfs::path path = bfs::temp_directory_path() / bfs::unique_path("s25coop-replay-%%%%-%%%%-%%%%");
        TmpDir() { bfs::create_directories(path); }
        ~TmpDir()
        {
            boost::system::error_code ec;
            bfs::remove_all(path, ec);
        }
    } tmpDir;
    const bfs::path mapPath = tmpDir.path / mapInfo.filepath.filename();
    if(!mapInfo.mapData.DecompressToFile(mapPath))
    {
        bnw::cerr << "Could not unpack the map from the replay" << std::endl;
        return 1;
    }
    bfs::path luaPath;
    if(mapInfo.luaData.uncompressedLength)
    {
        luaPath = bfs::path(mapPath).replace_extension("lua");
        if(!mapInfo.luaData.DecompressToFile(luaPath))
        {
            bnw::cerr << "Could not unpack the Lua script from the replay" << std::endl;
            return 1;
        }
    }

    std::vector<PlayerInfo> players;
    for(unsigned i = 0; i < replay.GetNumPlayers(); ++i)
        players.emplace_back(replay.GetPlayer(i));

    const unsigned seed = seedOverride ? *seedOverride : replay.getSeed();
    RANDOM.Init(seed);
    bnw::cout << "Checking replay " << path << " (" << replay.GetNumPlayers() << " players, " << replay.GetLastGF()
              << " GF, seed " << seed << ")" << std::endl;

    HeadlessGame game(replay.ggs, mapPath, std::move(players), luaPath);
    const HeadlessGame::ReplayCheck result = game.PlayReplay(replay);
    bnw::cout << "Final state: " << game.GetChecksum() << std::endl;
    game.Close();

    if(result.numAsync > 0)
    {
        bnw::cout << "Replay ASYNC at GF " << result.firstAsyncGF << ": recorded " << result.expected << ", replayed "
                  << result.actual << " (" << result.numAsync << " of " << result.numChecked << " checksums differ)"
                  << std::endl;
        return 3;
    }
    // The recording stops where its game ended, so a replayed game that ends sooner went a different way
    if(result.endGF != replay.GetLastGF() || result.commandsLeft)
    {
        bnw::cout << "Replay ASYNC at GF " << result.endGF << ": the replayed game ended before the recording did (GF "
                  << replay.GetLastGF() << (result.commandsLeft ? ", commands left" : "") << ")" << std::endl;
        return 3;
    }
    if(result.numChecked == 0)
    {
        bnw::cerr << "The replay holds no checksums, so nothing was checked" << std::endl;
        return 1;
    }
    bnw::cout << "Replay in sync: " << result.numChecked << " checksums" << std::endl;
    return 0;
}

int main(int argc, char** argv)
{
    bnw::nowide_filesystem();
    bnw::args _(argc, argv);

    optional<std::string> replay_path;
    optional<std::string> savegame_path;
    optional<std::string> lua_path;
    optional<std::string> settings_path;
    optional<std::string> test_script_path;
    optional<std::string> check_replay_path;
    unsigned random_init = static_cast<unsigned>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    unsigned random_ai_init = random_init;

    po::options_description desc("Allowed options");
    // clang-format off
    desc.add_options()
        ("help,h", "Show help")
        ("map,m", po::value<std::string>(),"Map to load (required unless --check-replay)")
        ("ai", po::value<std::vector<std::string>>(),"AI player(s) to add (aijh | dummy) (required unless --check-replay)")
        ("objective", po::value<std::string>()->default_value("domination"),"domination(default) | conquer | none (campaign missions: the script decides)")
        ("wares", po::value<std::string>()->default_value("normal"),"Starting wares: vlow | low | normal (default) | alot")
        ("settings", po::value(&settings_path),"INI file with an [addons] section to configure addon settings (optional)")
        ("replay", po::value(&replay_path),"Filename to write replay to (optional)")
        ("save", po::value(&savegame_path),"Filename to write savegame to (optional)")
        ("lua", po::value(&lua_path),"Lua script to execute during the game (optional)")
        ("random_init", po::value(&random_init),"Seed value for the random number generator (optional)")
        ("random_ai_init", po::value(&random_ai_init),"Seed value for the AI random number generator (optional)")
        ("maxGF", po::value<unsigned>()->default_value(std::numeric_limits<unsigned>::max()),"Maximum number of game frames to run (optional)")
        ("test", "Test mode: needs --lua; the script's onTestEnd(gf) asserts on the final state, onTestFrame(gf) may issue commands through the global test. Exit code 2 on any Lua error or failed assertion")
        ("test-script", po::value(&test_script_path),"Test mode: a second script run in the map script's Lua state, e.g. checks around a campaign mission (optional)")
        ("check-replay", po::value(&check_replay_path),"Test mode: replay this replay headless and check that it stays in sync with the recorded game. Exit code 3 if not. --random_init overrides the recorded seed (to prove the check can fail)")
        ("version", "Show version information and exit")
        ;
    // clang-format on

    const auto printHelp = [&](std::ostream& os) {
        os << desc
           << "\nNote: path arguments support the <RTTR_USERDATA> placeholder "
              "(game data folder: SAVES, REPLAYS, MAPS, PRESETS)."
           << std::endl;
    };

    if(argc == 1)
    {
        printHelp(bnw::cerr);
        return 1;
    }

    po::variables_map options;
    try
    {
        po::store(po::command_line_parser(argc, argv).options(desc).run(), options);

        if(options.count("help"))
        {
            printHelp(bnw::cout);
            return 0;
        }
        if(options.count("version"))
        {
            bnw::cout << rttr::version::GetTitle() << " v" << rttr::version::GetVersion() << "-"
                      << rttr::version::GetRevision() << std::endl
                      << "Compiled with " << System::getCompilerName() << " for " << System::getOSName() << std::endl;
            return 0;
        }

        po::notify(options);
        if(!check_replay_path && (!options.count("map") || !options.count("ai")))
            throw std::runtime_error("--map and --ai are required");
    } catch(const std::exception& e)
    {
        bnw::cerr << "Error: " << e.what() << std::endl;
        printHelp(bnw::cerr);
        return 1;
    }

    try
    {
        // We print arguments and seed in order to be able to reproduce crashes.
        for(int i = 0; i < argc; ++i)
            bnw::cout << argv[i] << " ";
        bnw::cout << std::endl;
        bnw::cout << "random_init: " << random_init << std::endl;
        bnw::cout << "random_ai_init: " << random_ai_init << std::endl;
        bnw::cout << std::endl;

        RTTRCONFIG.Init();
        // Lua errors and the AI log to file; without this that is ./logs, and a missing folder there turned
        // every Lua error into "Could not open logs/... for writing"
        const bfs::path logDir = RTTRCONFIG.ExpandPath(s25::folders::logs);
        bfs::create_directories(logDir);
        LOG.setLogFilepath(logDir);
        RANDOM.Init(random_init);
        AI::getRandomGenerator().seed(random_ai_init);

        if(check_replay_path)
            return checkReplay(RTTRCONFIG.ExpandPath(*check_replay_path),
                               options.count("random_init") ? optional<unsigned>(random_init) : optional<unsigned>());

        const bfs::path mapPath = RTTRCONFIG.ExpandPath(options["map"].as<std::string>());
        const std::vector<AI::Info> ais = ParseAIOptions(options["ai"].as<std::vector<std::string>>());

        GlobalGameSettings ggs;
        const auto objective = options["objective"].as<std::string>();
        if(objective == "domination")
            ggs.objective = GameObjective::TotalDomination;
        else if(objective == "conquer")
            ggs.objective = GameObjective::Conquer3_4;
        else if(objective == "none")
            ggs.objective = GameObjective::None;
        else
        {
            bnw::cerr << "unknown objective: " << objective << std::endl;
            return 1;
        }

        const auto wares = options["wares"].as<std::string>();
        if(wares == "vlow")
            ggs.startWares = StartWares::VLow;
        else if(wares == "low")
            ggs.startWares = StartWares::Low;
        else if(wares == "normal")
            ggs.startWares = StartWares::Normal;
        else if(wares == "alot")
            ggs.startWares = StartWares::ALot;
        else
        {
            bnw::cerr << "Unknown wares value: " << wares << std::endl;
            return 1;
        }

        if(settings_path)
        {
            loadAddonsFromIni(ggs, RTTRCONFIG.ExpandPath(*settings_path));

            bnw::cout << "settings: " << RTTRCONFIG.ExpandPath(*settings_path) << std::endl;
            bnw::cout << "addon selections (non-default only):" << std::endl;
            for(unsigned i = 0; i < ggs.getNumAddons(); ++i)
            {
                unsigned status = 0;
                const Addon* addon = ggs.getAddon(i, status);
                if(addon && status != addon->getDefaultStatus())
                {
                    bnw::cout << "  [0x" << std::hex << std::setw(8) << std::setfill('0')
                              << static_cast<unsigned>(addon->getId()) << std::dec << "] " << addon->getName() << " = ";
                    if(const auto* listAddon = dynamic_cast<const AddonList*>(addon))
                        bnw::cout << listAddon->getOptionName(status);
                    else if(dynamic_cast<const AddonBool*>(addon))
                        bnw::cout << (status ? "True" : "False");
                    else
                        bnw::cout << status;
                    bnw::cout << std::endl;
                }
            }
        }

        HeadlessGame game(ggs, mapPath, ais, lua_path ? RTTRCONFIG.ExpandPath(*lua_path) : bfs::path{});
        if(replay_path)
            game.RecordReplay(RTTRCONFIG.ExpandPath(*replay_path), random_init);

        const bool testMode = options.count("test") > 0;
        if(testMode)
        {
            if(!lua_path)
            {
                bnw::cerr << "--test needs --lua" << std::endl;
                return 1;
            }
            game.ShowLuaOutput();
            game.EnableTestInput();
            if(test_script_path)
                game.LoadTestScript(RTTRCONFIG.ExpandPath(*test_script_path));
        }

        game.Run(options["maxGF"].as<unsigned>());
        if(testMode)
        {
            game.CheckTestEnd();
            // Same map, seeds and GF must give the same line: tests/coop/checkDeterminism.cmake compares two runs
            bnw::cout << "Final state: " << game.GetChecksum() << std::endl;
            // What the mission scripts recorded as campaign progress (tests/coop: CoopCampaign_*)
            for(const auto& progress : SETTINGS.campaigns.createSaveData())
                bnw::cout << "Campaign progress: " << progress.first << "=" << progress.second << std::endl;
            bnw::cout << "TEST PASSED" << std::endl;
        }
        game.Close();
        if(savegame_path)
            game.SaveGame(RTTRCONFIG.ExpandPath(*savegame_path));
    } catch(const LuaExecutionError& e)
    {
        // Also while the script starts (onStart, onSettingsReady ...): a test must never pass on that
        bnw::cerr << "Lua error: " << e.what() << std::endl;
        return 2;
    } catch(const std::exception& e)
    {
        bnw::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
