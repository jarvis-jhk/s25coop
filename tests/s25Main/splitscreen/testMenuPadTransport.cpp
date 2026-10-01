// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "LeatherLoader.h"
#include "LocalGameFixture.h"
#include "PadFixture.h"
#include "controls/ctrlImageButton.h"
#include "controls/ctrlOptionGroup.h"
#include "controls/ctrlTextButton.h"
#include "driver/MouseCoords.h"
#include "helpers/EnumRange.h"
#include "ingameWindows/iwHelp.h"
#include "ingameWindows/iwMainMenu.h"
#include "ingameWindows/iwPadSystemMenu.h"
#include "ingameWindows/iwTransport.h"
#include "gameData/GoodConsts.h"
#include "gameData/SettingTypeConv.h"
#include "rttr/test/TmpFolder.hpp"
#include <libsiedler2/Archiv.h>
#include <libsiedler2/ArchivItem_Bitmap_Raw.h>
#include <libsiedler2/PixelBufferBGRA.h>
#include <libsiedler2/libsiedler2.h>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr PadDeviceId pad = 104;

[[noreturn]] void throwCleanupProbe()
{
    throw std::runtime_error("transport cleanup probe");
}

struct TransportPadFixture : uiHelper::Fixture, rttr::test::LocalGameFixture
{
    rttr::test::PadFeeder pads{*uiHelper::GetVideoDriver()};
    rttr::test::TestableGameInterface* dsk = nullptr;
    iwTransport* window = nullptr;
    const bool savedDebugMode = SETTINGS.global.debugMode;
    rttr::test::TmpFolder graphics;

    template<class F>
    void run(const bool leather, F&& body)
    {
        try
        {
            beginGame(leather);
            body();
        } catch(...)
        {
            cleanup();
            throw;
        }
        cleanup();
    }

    void cleanup()
    {
        cleanupDesktop();
        SETTINGS.global.debugMode = savedDebugMode;
    }

    void cleanupDesktop()
    {
        // The windows retain their viewer/factory: destroy them before stopping the backend.
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        WINDOWMANAGER.Draw();
        if(ci().game)
            world().SetGameInterface(nullptr);
        dsk = nullptr;
        window = nullptr;
    }

    void beginGame(const bool leather)
    {
        SETTINGS.global.debugMode = false;
        hostAndEnterLobby();
        lobby().SetPlayerState(1, PlayerState::AI, AI::Info(AI::Type::Dummy));
        lobby().SetPlayerState(2, PlayerState::AI, AI::Info(AI::Type::Dummy));
        pumpUntil(
          [] {
              const auto lobby = GAMECLIENT.GetGameLobby();
              return lobby->getPlayer(1).ps == PlayerState::AI && lobby->getPlayer(2).ps == PlayerState::AI;
          },
          "dummy AI configuration");
        const auto before = GAMECLIENT.GetGameLobby()->getSettings();
        auto settings = before;
        settings.setSelection(AddonId::LEATHER, leather ? 1 : 0);
        lobby().ChangeGlobalGameSettings(settings);
        // ChangeGlobalGameSettings edits this copy eagerly. Require the server broadcast instead.
        GAMECLIENT.GetGameLobby()->getSettings() = before;
        GAMECLIENT.GetGameLobby()->getSettings().setSelection(AddonId::LEATHER, leather ? 0 : 1);
        pumpUntil([leather] { return GAMECLIENT.GetGameLobby()->getSettings().isEnabled(AddonId::LEATHER) == leather; },
                  "leather settings");
        startGame();
        BOOST_TEST(world().GetGGS().isEnabled(AddonId::LEATHER) == leather);
        makeDesktop();
    }

    void makeDesktop()
    {
        LOADER.LoadDummyMapFiles();
        // The normal GUI/map placeholders contain no addon sprites. Generate this window's icon.
        const auto path = graphics / "leather_bobs.lst";
        boost::filesystem::create_directories(path);
        const auto icon = leatheraddon::bobIndex[leatheraddon::BobType::LeatherWareIcon];
        libsiedler2::Archiv sprite;
        auto bitmap = std::make_unique<libsiedler2::ArchivItem_Bitmap_Raw>();
        bitmap->create(libsiedler2::PixelBufferBGRA(1, 1));
        sprite.push(std::move(bitmap));
        BOOST_TEST_REQUIRE(libsiedler2::Write(path / (std::to_string(icon) + ".bmp"), sprite) == 0);
        BOOST_TEST_REQUIRE(LOADER.LoadFiles({path.string()}));
        BOOST_TEST_REQUIRE(LOADER.GetWareTex(GoodType::Leather) != nullptr);
        auto desktop = std::make_unique<rttr::test::TestableGameInterface>(ci().game, GAMECLIENT.GetNWFInfo(),
                                                                           GAMECLIENT.GetPlayerId(), false);
        dsk = desktop.get();
        WINDOWMANAGER.Switch(std::move(desktop));
        WINDOWMANAGER.Draw();
        GAMECLIENT.SetInterface(&ci());
        BOOST_TEST_REQUIRE(dsk->GetNumViews() == 1u);
        pads.pickUp(pad);
        frame();
        BOOST_TEST_REQUIRE(dsk->GetPlayerView(0).HasPadCursor());
    }

    void frame()
    {
        dsk->UpdateInput(16, Position(-10000, -10000));
        WINDOWMANAGER.Draw();
        // Closing a window may reactivate the desktop and replace the backend observer.
        GAMECLIENT.SetInterface(&ci());
    }
    void press(const PadButton button)
    {
        pads.tap(pad, button);
        frame();
    }
    Window* focused() const { return dsk->GetPlayerView(0).GetFocus().GetFocused(); }

    void focusUntil(const Window* target)
    {
        if(dsk->GetPlayerView(0).GetRing().IsOpen())
        {
            for(unsigned sector = 0; sector < 8 && focused() != target; ++sector)
                press(PadButton::DpadRight);
        } else
        {
            // Shoulder navigation does not wrap; returning to an earlier control needs Left.
            for(const auto direction : {PadButton::RightShoulder, PadButton::LeftShoulder})
            {
                for(unsigned i = 0; i < 40 && focused() != target; ++i)
                    press(direction);
            }
        }
        BOOST_TEST_REQUIRE(focused() == target);
    }

    void enter()
    {
        press(PadButton::Back);
        auto* system = dynamic_cast<iwPadSystemMenu*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(system != nullptr);
        focusUntil(system->GetCtrl<ctrlTextButton>(iwPadSystemMenu::ID_MAIN_SELECTION));
        press(PadButton::A);
        openFromMain();
    }

    void openFromMain()
    {
        auto* main = dynamic_cast<iwMainMenu*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(main != nullptr);
        press(PadButton::Y);
        focusUntil(main->GetCtrl<ctrlImageButton>(1));
        press(PadButton::A);
        window = dynamic_cast<iwTransport*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(window != nullptr);
        press(PadButton::Y);
        BOOST_TEST_REQUIRE(dsk->GetPlayerView(0).GetFocus().GetRoot() == window);
    }

    ctrlOptionGroup& group() const { return *window->GetCtrl<ctrlOptionGroup>(6); }
    void click(const unsigned id)
    {
        focusUntil(window->GetCtrl<ctrlImageButton>(id));
        press(PadButton::A);
    }
    void select(const unsigned index)
    {
        focusUntil(group().GetCtrl<ctrlImageButton>(index));
        press(PadButton::A);
        BOOST_TEST(group().GetSelection() == index);
    }
    void mouseClick(const Window& control)
    {
        MouseCoords mc(control.GetDrawPos() + DrawPoint(10, 5));
        mc.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mc);
        mc.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mc);
        frame();
    }

    static TransportOrders defaults()
    {
        TransportOrders order;
        std::iota(order.begin(), order.end(), 0);
        return order;
    }
    static std::vector<TransportOrders::value_type> visibleDefaults(const bool leather)
    {
        const auto order = defaults();
        std::vector<TransportOrders::value_type> visible(order.begin(), order.end());
        if(!leather)
            visible.erase(visible.begin() + 7);
        return visible;
    }
    static TransportOrders completeOrder(std::vector<TransportOrders::value_type> visible, const bool leather)
    {
        if(!leather)
            visible.push_back(7);
        TransportOrders result;
        BOOST_TEST_REQUIRE(visible.size() == result.size());
        std::copy(visible.begin(), visible.end(), result.begin());
        return result;
    }

    void expectButtons(const std::vector<TransportOrders::value_type>& expected) const
    {
        const std::array<std::string, 15> names = {
          WARE_NAMES[GoodType::Coins],  gettext_noop("Weapons"),        WARE_NAMES[GoodType::Beer],
          WARE_NAMES[GoodType::Iron],   WARE_NAMES[GoodType::Gold],     WARE_NAMES[GoodType::IronOre],
          WARE_NAMES[GoodType::Coal],   gettext_noop("Leatherworking"), WARE_NAMES[GoodType::Boards],
          WARE_NAMES[GoodType::Stones], WARE_NAMES[GoodType::Wood],     WARE_NAMES[GoodType::Water],
          gettext_noop("Food"),         gettext_noop("Tools"),          WARE_NAMES[GoodType::Boat]};
        const std::array<ITexture*, 15> sprites = {
          LOADER.GetWareTex(GoodType::Coins),  LOADER.GetTextureN("io", 111),
          LOADER.GetWareTex(GoodType::Beer),   LOADER.GetWareTex(GoodType::Iron),
          LOADER.GetWareTex(GoodType::Gold),   LOADER.GetWareTex(GoodType::IronOre),
          LOADER.GetWareTex(GoodType::Coal),   LOADER.GetWareTex(GoodType::Leather),
          LOADER.GetWareTex(GoodType::Boards), LOADER.GetWareTex(GoodType::Stones),
          LOADER.GetWareTex(GoodType::Wood),   LOADER.GetWareTex(GoodType::Water),
          LOADER.GetTextureN("io", 80),        LOADER.GetWareTex(GoodType::Hammer),
          LOADER.GetWareTex(GoodType::Boat)};
        for(unsigned i = 0; i < expected.size(); ++i)
        {
            const auto* button = group().GetCtrl<ctrlImageButton>(i);
            BOOST_TEST_REQUIRE(button != nullptr);
            BOOST_TEST(button->GetTooltip() == _(names[expected[i]]));
            BOOST_TEST(button->GetImage() == sprites[expected[i]]);
            BOOST_TEST(button->GetIlluminated() == (i == group().GetSelection()));
        }
        BOOST_TEST(group().GetCtrl<ctrlImageButton>(expected.size()) == nullptr);
    }

    TransportOrders actual()
    {
        VisualSettings settings;
        world().GetPlayer(0).FillVisualSettings(settings);
        return settings.transport_order;
    }
    void expectWorld(const TransportOrders& expected)
    {
        const auto order = actual();
        for(unsigned i = 0; i < expected.size(); ++i)
            BOOST_TEST(order[i] == expected[i]);
        for(const auto good : helpers::EnumRange<GoodType>{})
        {
            const auto priority =
              std::find(expected.begin(), expected.end(), STD_TRANSPORT_PRIO[good]) - expected.begin();
            BOOST_TEST(world().GetPlayer(0).GetTransportPriority(good) == priority);
        }
        BOOST_TEST(ci().numErrors == 0u);
        BOOST_TEST(ci().numAsync == 0u);
    }
    void closeAndRoundtrip(const TransportOrders& expected)
    {
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(dynamic_cast<iwMainMenu*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        window = nullptr;
        if(!GAMECLIENT.IsReplayModeOn())
            pumpUntilGF(GAMECLIENT.GetGFNumber() + 2u * GAMECLIENT.GetNWFLength());
        // Commands can still be buffered beyond two NWFs; wait for the actual simulation, not its eager UI copy.
        pumpUntil([this, &expected] { return actual() == expected; }, "transport order to reach the actual world",
                  std::chrono::seconds(10));
        expectWorld(expected);
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(PadTransportTests)

BOOST_DATA_TEST_CASE_F(TransportPadFixture, AllMovesBoundariesDefaultAndReopenReachTheWorld,
                       boost::unit_test::data::make({false, true}), leather)
{
    run(leather, [this, leather] {
        enter();
        auto expected = visibleDefaults(leather);
        expectButtons(expected);
        expectWorld(defaults());
        select(2);
        click(3);
        std::swap(expected[1], expected[2]);
        BOOST_TEST(group().GetSelection() == 1u);
        expectButtons(expected);
        click(4);
        std::swap(expected[1], expected[2]);
        BOOST_TEST(group().GetSelection() == 2u);
        expectButtons(expected);
        click(2);
        std::rotate(expected.begin(), expected.begin() + 2, expected.begin() + 3);
        BOOST_TEST(group().GetSelection() == 0u);
        expectButtons(expected);
        click(3);
        click(2);
        expectButtons(expected);
        click(5);
        std::rotate(expected.begin(), expected.begin() + 1, expected.end());
        BOOST_TEST(group().GetSelection() == expected.size() - 1u);
        expectButtons(expected);
        click(4);
        click(5);
        expectButtons(expected);
        click(3);
        std::iter_swap(expected.end() - 2, expected.end() - 1);
        BOOST_TEST(group().GetSelection() == expected.size() - 2u);
        expectButtons(expected);
        closeAndRoundtrip(completeOrder(expected, leather));
        openFromMain();
        expectButtons(expected);
        // Default keeps the selected row, but resets every visible category and the backend.
        select(expected.size() - 1u);
        click(1);
        BOOST_TEST(group().GetSelection() == expected.size() - 1u);
        expectButtons(visibleDefaults(leather));
        closeAndRoundtrip(completeOrder(visibleDefaults(leather), leather));
        openFromMain();
        expectButtons(visibleDefaults(leather));
    });
    BOOST_TEST(SETTINGS.global.debugMode == savedDebugMode);
}

BOOST_DATA_TEST_CASE_F(TransportPadFixture, SelectionMouseAndHelpDoNotChangePriorities,
                       boost::unit_test::data::make({false, true}), leather)
{
    run(leather, [this, leather] {
        enter();
        const auto expected = visibleDefaults(leather);
        for(unsigned i = 0; i < expected.size(); ++i)
            select(i);
        mouseClick(*group().GetCtrl<ctrlImageButton>(0));
        BOOST_TEST(group().GetSelection() == 0u);
        expectButtons(expected);
        closeAndRoundtrip(defaults());
        openFromMain();
        click(0);
        BOOST_TEST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        expectButtons(expected);
        press(PadButton::Y);
        closeAndRoundtrip(defaults());
    });
}

BOOST_DATA_TEST_CASE_F(TransportPadFixture, ReplayReflectsRecordedPrioritiesAndRejectsBothInputPaths,
                       boost::unit_test::data::make({false, true}), leather)
{
    run(leather, [this, leather] {
        enter();
        auto recorded = visibleDefaults(leather);
        click(5);
        std::rotate(recorded.begin(), recorded.begin() + 1, recorded.end());
        closeAndRoundtrip(completeOrder(recorded, leather));
        pumpUntilGF(GAMECLIENT.GetGFNumber() + 40u);
        const auto replay = RTTRCONFIG.ExpandPath(s25::folders::replays) / GAMECLIENT.GetReplayFilename();
        cleanupDesktop();
        GAMECLIENT.Stop();
        GAMESERVER.Stop();
        BOOST_TEST_REQUIRE(boost::filesystem::is_regular_file(replay));
        GAMECLIENT.SetInterface(&ci());
        BOOST_TEST_REQUIRE(GAMECLIENT.StartReplay(replay));
        GAMECLIENT.GameLoaded();
        makeDesktop();
        GAMECLIENT.SetPause(false);
        enter();
        select(2);
        const auto before = actual();
        for(const unsigned id : {1u, 2u, 3u, 4u, 5u})
        {
            click(id);
            expectButtons(visibleDefaults(leather));
        }
        for(const unsigned id : {1u, 2u, 3u, 4u, 5u})
        {
            mouseClick(*window->GetCtrl<ctrlImageButton>(id));
            expectButtons(visibleDefaults(leather));
        }
        expectWorld(before);
        GAMECLIENT.skiptogf = GAMECLIENT.GetLastReplayGF();
        pumpUntil([this] { return ci().replayEnded || ci().numErrors || ci().numReplayAsync; }, "replay to finish");
        // The settings adapter's timer measures wall time, independently of mock video ticks.
        std::this_thread::sleep_for(std::chrono::milliseconds(2100));
        frame();
        expectButtons(recorded);
        BOOST_TEST(ci().replayEnded);
        BOOST_TEST(ci().numReplayAsync == 0u);
        closeAndRoundtrip(completeOrder(recorded, leather));
    });
}

BOOST_AUTO_TEST_CASE(ExceptionalCleanupRestoresSettingsBeforeFixtureDestruction)
{
    const auto autosaveBefore = SETTINGS.interface.autosaveInterval;
    const auto debugBefore = SETTINGS.global.debugMode;
    {
        TransportPadFixture fixture;
        bool reachedProbe = false;
        BOOST_CHECK_THROW(fixture.run(false,
                                      [&fixture, &reachedProbe] {
                                          fixture.enter();
                                          reachedProbe = true;
                                          throwCleanupProbe();
                                      }),
                          std::runtime_error);
        BOOST_TEST(reachedProbe);
        BOOST_TEST(dynamic_cast<dskGameInterface*>(WINDOWMANAGER.GetCurrentDesktop()) == nullptr);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(SETTINGS.global.debugMode == debugBefore);
    }
    BOOST_TEST(SETTINGS.interface.autosaveInterval == autosaveBefore);
    BOOST_TEST(SETTINGS.global.debugMode == debugBefore);
}

BOOST_AUTO_TEST_SUITE_END()
