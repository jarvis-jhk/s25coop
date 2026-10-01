// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "LocalGameFixture.h"
#include "PadFixture.h"
#include "controls/ctrlComboBox.h"
#include "controls/ctrlImage.h"
#include "controls/ctrlImageButton.h"
#include "controls/ctrlList.h"
#include "controls/ctrlTextButton.h"
#include "driver/MouseCoords.h"
#include "helpers/optional_io.h"
#include "ingameWindows/iwBuildOrder.h"
#include "ingameWindows/iwMainMenu.h"
#include "ingameWindows/iwPadSystemMenu.h"
#include "gameData/BuildingConsts.h"
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
constexpr PadDeviceId pad = 103;

[[noreturn]] void throwCleanupProbe()
{
    throw std::runtime_error("build order cleanup probe");
}

struct BuildOrderPadFixture : uiHelper::Fixture, rttr::test::LocalGameFixture
{
    rttr::test::PadFeeder pads{*uiHelper::GetVideoDriver()};
    rttr::test::TestableGameInterface* dsk = nullptr;
    iwBuildOrder* window = nullptr;
    const bool savedDebugMode = SETTINGS.global.debugMode;

    ~BuildOrderPadFixture() { SETTINGS.global.debugMode = savedDebugMode; }

    template<class F>
    void run(const unsigned mask, F&& body)
    {
        // Destroy the real desktop and its windows while the game and its interfaces still exist.
        try
        {
            beginGame(mask);
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
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        WINDOWMANAGER.Draw();
        if(ci().game)
            world().SetGameInterface(nullptr);
        dsk = nullptr;
        window = nullptr;
    }

    void beginGame(const unsigned mask)
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
        settings.setSelection(AddonId::CUSTOM_BUILD_SEQUENCE, 1);
        settings.setSelection(AddonId::WINE, (mask & 1u) ? 1 : 0);
        settings.setSelection(AddonId::LEATHER, (mask & 2u) ? 1 : 0);
        settings.setSelection(AddonId::CHARBURNER, (mask & 4u) ? 1 : 0);
        lobby().ChangeGlobalGameSettings(settings);
        GAMECLIENT.GetGameLobby()->getSettings() = before;
        pumpUntil(
          [&settings] {
              const auto& received = GAMECLIENT.GetGameLobby()->getSettings();
              return received.isEnabled(AddonId::CUSTOM_BUILD_SEQUENCE)
                     && received.getSelection(AddonId::WINE) == settings.getSelection(AddonId::WINE)
                     && received.getSelection(AddonId::LEATHER) == settings.getSelection(AddonId::LEATHER)
                     && received.getSelection(AddonId::CHARBURNER) == settings.getSelection(AddonId::CHARBURNER);
          },
          "addon settings");
        startGame();
        // The controller updates its lobby copy eagerly; the running world proves the server received it.
        BOOST_TEST(world().GetGGS().isEnabled(AddonId::CUSTOM_BUILD_SEQUENCE));
        BOOST_TEST(world().GetGGS().isEnabled(AddonId::WINE) == ((mask & 1u) != 0u));
        BOOST_TEST(world().GetGGS().isEnabled(AddonId::LEATHER) == ((mask & 2u) != 0u));
        BOOST_TEST(world().GetGGS().isEnabled(AddonId::CHARBURNER) == ((mask & 4u) != 0u));
        makeDesktop();
    }

    void makeDesktop()
    {
        LOADER.LoadDummyMapFiles();
        LOADER.LoadDummyBuildingFiles();
        auto desktop = std::make_unique<rttr::test::TestableGameInterface>(ci().game, GAMECLIENT.GetNWFInfo(),
                                                                           GAMECLIENT.GetPlayerId(), false);
        dsk = desktop.get();
        WINDOWMANAGER.Switch(std::move(desktop));
        WINDOWMANAGER.Draw();
        // Desktop activation installs its own interface; observe real backend errors and replay completion here.
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
        // Closing the last window can reactivate the desktop and replace the observer again.
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
        focusUntil(main->GetCtrl<ctrlImageButton>(10));
        press(PadButton::A);
        window = dynamic_cast<iwBuildOrder*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(window != nullptr);
        press(PadButton::Y);
        BOOST_TEST_REQUIRE(dsk->GetPlayerView(0).GetFocus().GetRoot() == window);
    }

    ctrlList& list() const { return *window->GetCtrl<ctrlList>(0); }
    ctrlComboBox& mode() const { return *window->GetCtrl<ctrlComboBox>(6); }
    void click(const unsigned id)
    {
        focusUntil(window->GetCtrl<ctrlImageButton>(id));
        press(PadButton::A);
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

    static bool inactive(const BuildingType bld, const unsigned mask)
    {
        if(!(mask & 1u)
           && (bld == BuildingType::Vineyard || bld == BuildingType::Winery || bld == BuildingType::Temple))
            return true;
        if(!(mask & 2u)
           && (bld == BuildingType::Skinner || bld == BuildingType::Tannery || bld == BuildingType::LeatherWorks))
            return true;
        return !(mask & 4u) && bld == BuildingType::Charburner;
    }

    static std::vector<BuildingType> visibleDefaults(const unsigned mask)
    {
        const auto defaults = GamePlayer::GetStandardBuildOrder();
        std::vector<BuildingType> visible;
        std::copy_if(defaults.begin(), defaults.end(), std::back_inserter(visible),
                     [mask](const auto bld) { return !inactive(bld, mask); });
        return visible;
    }

    static BuildOrders completeOrder(std::vector<BuildingType> visible, const unsigned mask)
    {
        if(!(mask & 1u))
            visible.insert(visible.end(), {BuildingType::Vineyard, BuildingType::Winery, BuildingType::Temple});
        if(!(mask & 4u))
            visible.push_back(BuildingType::Charburner);
        if(!(mask & 2u))
            visible.insert(visible.end(), {BuildingType::Skinner, BuildingType::Tannery, BuildingType::LeatherWorks});
        BuildOrders result;
        BOOST_TEST_REQUIRE(visible.size() == result.size());
        std::copy(visible.begin(), visible.end(), result.begin());
        return result;
    }

    void expectList(const std::vector<BuildingType>& expected)
    {
        BOOST_TEST_REQUIRE(list().GetNumLines() == expected.size());
        for(unsigned i = 0; i < expected.size(); ++i)
            BOOST_TEST(list().GetItemText(i) == _(BUILDING_NAMES[expected[i]]));
        BOOST_TEST_REQUIRE(list().GetSelection().has_value());
        BOOST_TEST(window->GetCtrl<ctrlImage>(5)->GetImage()
                   == LOADER.GetBuildingTex(world().GetPlayer(0).nation, expected[*list().GetSelection()]));
    }

    VisualSettings actual()
    {
        VisualSettings result;
        world().GetPlayer(0).FillVisualSettings(result);
        return result;
    }

    void expectWorld(const BuildOrders& expected, const bool custom)
    {
        const auto settings = actual();
        BOOST_TEST(settings.useCustomBuildOrder == custom);
        for(unsigned i = 0; i < expected.size(); ++i)
            BOOST_TEST(rttr::enum_cast(settings.build_order[i]) == rttr::enum_cast(expected[i]));
        BOOST_TEST(ci().numErrors == 0u);
        BOOST_TEST(ci().numAsync == 0u);
    }

    void closeAndRoundtrip(const BuildOrders& expected, const bool custom)
    {
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(dynamic_cast<iwMainMenu*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        window = nullptr;
        if(!GAMECLIENT.IsReplayModeOn())
            pumpUntilGF(GAMECLIENT.GetGFNumber() + 2u * GAMECLIENT.GetNWFLength());
        pumpUntil(
          [this, &expected, custom] {
              const auto settings = actual();
              return settings.build_order == expected && settings.useCustomBuildOrder == custom;
          },
          "build order to reach the actual world");
        expectWorld(expected, custom);
    }

    void chooseCustom(const bool custom)
    {
        focusUntil(&mode());
        press(PadButton::A);
        BOOST_TEST_REQUIRE(mode().IsListOpen());
        press(custom ? PadButton::DpadDown : PadButton::DpadUp);
        press(PadButton::A);
        BOOST_TEST(!mode().IsListOpen());
        BOOST_TEST((mode().GetSelection() == (custom ? 1u : 0u)));
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(PadBuildOrderTests)

BOOST_DATA_TEST_CASE_F(BuildOrderPadFixture, DefaultRestoresEveryVisibleBuildingForEachAddonCombination,
                       boost::unit_test::data::xrange(8u), mask)
{
    run(mask, [this, mask] {
        enter();
        const auto defaults = visibleDefaults(mask);
        auto edited = defaults;
        expectList(defaults);
        focusUntil(&list());
        for(unsigned i = 1; i < defaults.size(); ++i)
            press(PadButton::DpadDown);
        click(1);
        std::rotate(edited.begin(), edited.end() - 1, edited.end());
        expectList(edited);
        chooseCustom(true);
        closeAndRoundtrip(completeOrder(edited, mask), true);
        openFromMain();
        expectList(edited);
        click(10);
        BOOST_TEST((list().GetSelection() == 0u));
        expectList(defaults);
        closeAndRoundtrip(completeOrder(defaults, mask), true);
        openFromMain();
        expectList(defaults);
    });
}

BOOST_FIXTURE_TEST_CASE(UpDownTopBottomAndBoundaryMovesRoundtripTheEntireOrder, BuildOrderPadFixture)
{
    run(7u, [this] {
        enter();
        auto expected = visibleDefaults(7u);
        focusUntil(&list());
        press(PadButton::DpadDown);
        press(PadButton::DpadDown);
        BOOST_TEST((list().GetSelection() == 2u));
        click(2);
        std::swap(expected[1], expected[2]);
        BOOST_TEST((list().GetSelection() == 1u));
        expectList(expected);
        click(3);
        std::swap(expected[1], expected[2]);
        BOOST_TEST((list().GetSelection() == 2u));
        expectList(expected);
        click(1);
        std::rotate(expected.begin(), expected.begin() + 2, expected.begin() + 3);
        BOOST_TEST((list().GetSelection() == 0u));
        expectList(expected);
        click(2);
        expectList(expected);
        click(4);
        std::rotate(expected.begin(), expected.begin() + 1, expected.end());
        BOOST_TEST((list().GetSelection() == expected.size() - 1u));
        expectList(expected);
        click(3);
        expectList(expected);
        click(2);
        std::iter_swap(expected.end() - 2, expected.end() - 1);
        BOOST_TEST((list().GetSelection() == expected.size() - 2u));
        expectList(expected);
        closeAndRoundtrip(completeOrder(expected, 7u), false);
        openFromMain();
        expectList(expected);
    });
}

BOOST_FIXTURE_TEST_CASE(BrowsingTheListOrCancellingTheModeDoesNotChangeTheWorld, BuildOrderPadFixture)
{
    run(0u, [this] {
        enter();
        const auto original = GamePlayer::GetStandardBuildOrder();
        focusUntil(&list());
        press(PadButton::DpadDown);
        expectList(visibleDefaults(0u));
        focusUntil(&mode());
        press(PadButton::A);
        press(PadButton::DpadDown);
        BOOST_TEST((mode().GetSelection() == 1u));
        press(PadButton::B);
        BOOST_TEST(!mode().IsListOpen());
        BOOST_TEST((mode().GetSelection() == 0u));
        expectWorld(original, false);
        closeAndRoundtrip(original, false);
        openFromMain();
        chooseCustom(true);
        closeAndRoundtrip(completeOrder(visibleDefaults(0u), 0u), true);
        openFromMain();
        chooseCustom(false);
        closeAndRoundtrip(completeOrder(visibleDefaults(0u), 0u), false);
    });
}

BOOST_FIXTURE_TEST_CASE(ReplayShowsRecordedChangesAndRejectsControllerAndMouseEdits, BuildOrderPadFixture)
{
    run(7u, [this] {
        enter();
        auto recorded = visibleDefaults(7u);
        click(4);
        std::rotate(recorded.begin(), recorded.begin() + 1, recorded.end());
        chooseCustom(true);
        closeAndRoundtrip(completeOrder(recorded, 7u), true);
        pumpUntilGF(GAMECLIENT.GetGFNumber() + 40u);
        const auto replay = RTTRCONFIG.ExpandPath(s25::folders::replays) / GAMECLIENT.GetReplayFilename();
        cleanup();
        GAMECLIENT.Stop();
        GAMESERVER.Stop();
        BOOST_TEST_REQUIRE(boost::filesystem::is_regular_file(replay));
        GAMECLIENT.SetInterface(&ci());
        BOOST_TEST_REQUIRE(GAMECLIENT.StartReplay(replay));
        GAMECLIENT.GameLoaded();
        makeDesktop();
        GAMECLIENT.SetPause(false);
        enter();
        BOOST_TEST(mode().isReadOnly());
        focusUntil(&list());
        press(PadButton::DpadDown);
        const auto before = actual();
        for(const unsigned id : {1u, 2u, 3u, 4u, 10u})
        {
            click(id);
            expectList(visibleDefaults(7u));
        }
        for(const unsigned id : {1u, 2u, 3u, 4u, 10u})
        {
            mouseClick(*window->GetCtrl<ctrlImageButton>(id));
            expectList(visibleDefaults(7u));
        }
        mouseClick(mode());
        BOOST_TEST(!mode().CanFocus());
        for(const auto direction : {PadButton::RightShoulder, PadButton::LeftShoulder})
        {
            for(unsigned i = 0; i < 8; ++i)
            {
                press(direction);
                BOOST_TEST(focused() != &mode());
            }
        }
        focusUntil(&list());
        press(PadButton::A);
        press(PadButton::DpadDown);
        press(PadButton::A);
        BOOST_TEST(!mode().IsListOpen());
        expectWorld(before.build_order, before.useCustomBuildOrder);
        GAMECLIENT.skiptogf = GAMECLIENT.GetLastReplayGF();
        pumpUntil([this] { return ci().replayEnded || ci().numErrors || ci().numReplayAsync; },
                  "recorded replay to finish");
        std::this_thread::sleep_for(std::chrono::milliseconds(2100));
        frame();
        expectList(recorded);
        BOOST_TEST((mode().GetSelection() == 1u));
        BOOST_TEST(ci().replayEnded);
        BOOST_TEST(ci().numReplayAsync == 0u);
        closeAndRoundtrip(completeOrder(recorded, 7u), true);
    });
}

BOOST_AUTO_TEST_CASE(ExceptionalCleanupDestroysWindowsBeforeStoppingTheGame)
{
    const auto autosaveBefore = SETTINGS.interface.autosaveInterval;
    const auto debugBefore = SETTINGS.global.debugMode;
    {
        BuildOrderPadFixture fixture;
        BOOST_CHECK_THROW(fixture.run(0u,
                                      [&fixture] {
                                          fixture.enter();
                                          throwCleanupProbe();
                                      }),
                          std::runtime_error);
        BOOST_TEST(dynamic_cast<dskGameInterface*>(WINDOWMANAGER.GetCurrentDesktop()) == nullptr);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    }
    BOOST_TEST(SETTINGS.interface.autosaveInterval == autosaveBefore);
    BOOST_TEST(SETTINGS.global.debugMode == debugBefore);
}

BOOST_AUTO_TEST_SUITE_END()
