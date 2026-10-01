// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AsyncChecksum.h"
#include "LocalGameFixture.h"
#include "PadFixture.h"
#include "controls/ctrlImageButton.h"
#include "controls/ctrlMultiline.h"
#include "controls/ctrlText.h"
#include "controls/ctrlTextButton.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "helpers/EnumRange.h"
#include "ingameWindows/iwHelp.h"
#include "ingameWindows/iwMissionStatement.h"
#include "ingameWindows/iwPadSystemMenu.h"
#include "ingameWindows/iwPostWindow.h"
#include "postSystem/PostBox.h"
#include "postSystem/PostManager.h"
#include "postSystem/PostMsg.h"
#include <boost/test/unit_test.hpp>
#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr PadDeviceId pad = 112;
// iwPostWindow's IDs are private to its source; the expected texts and targets below
// independently verify that each physical button still performs the intended action.
constexpr unsigned all = 2, goal = 3, military = 4, geologist = 5, economy = 6, general = 7;
constexpr unsigned help = 8, oldest = 9, previous = 10, next = 11, newest = 12, goTo = 13, deleteLetter = 14;
constexpr unsigned messageText = 16, infoText = 19;
constexpr std::array<PostCategory, 9> categories = {
  PostCategory::General,   PostCategory::Military, PostCategory::Geologist,
  PostCategory::Economy,   PostCategory::General,  PostCategory::Military,
  PostCategory::Geologist, PostCategory::Economy,  PostCategory::Military};

[[noreturn]] void throwCleanupProbe()
{
    throw std::runtime_error("post office cleanup probe");
}

struct Letter
{
    // Snapshot expectations before input: a broken delete target must fail assertions
    // without dereferencing the message it accidentally destroyed.
    PostMsg snapshot;
    const PostMsg* identity;
};

struct PostPadFixture : uiHelper::Fixture, rttr::test::LocalGameFixture
{
    rttr::test::PadFeeder pads{*uiHelper::GetVideoDriver()};
    rttr::test::TestableGameInterface* dsk = nullptr;
    iwPostWindow* window = nullptr;
    const bool savedDebugMode = SETTINGS.global.debugMode;
    const decltype(SETTINGS.windows.persistentSettings) savedWindows = SETTINGS.windows.persistentSettings;

    template<class F>
    void run(F&& body)
    {
        try
        {
            beginGame();
            body();
        } catch(...)
        {
            cleanup();
            throw;
        }
        cleanup();
        expectRestored();
    }
    void expectRestored() const
    {
        BOOST_TEST(SETTINGS.global.debugMode == savedDebugMode);
        BOOST_TEST_REQUIRE(SETTINGS.windows.persistentSettings.size() == savedWindows.size());
        for(const auto& entry : savedWindows)
        {
            const auto it = SETTINGS.windows.persistentSettings.find(entry.first);
            BOOST_TEST_REQUIRE((it != SETTINGS.windows.persistentSettings.end()));
            const auto& actual = it->second;
            BOOST_TEST(actual.isOpen == entry.second.isOpen);
            BOOST_TEST(actual.isMinimized == entry.second.isMinimized);
            BOOST_TEST(actual.isPinned == entry.second.isPinned);
            BOOST_TEST((actual.lastPos == entry.second.lastPos));
            BOOST_TEST((actual.restorePos == entry.second.restorePos));
        }
    }
    void cleanup()
    {
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        WINDOWMANAGER.Draw();
        if(ci().game)
            world().SetGameInterface(nullptr);
        dsk = nullptr;
        window = nullptr;
        // Windows retain pointers into persistent settings until they are destroyed.
        SETTINGS.windows.persistentSettings = savedWindows;
        SETTINGS.global.debugMode = savedDebugMode;
    }
    void beginGame()
    {
        SETTINGS.global.debugMode = false;
        hostAndEnterLobby();
        ci().numErrors = 0;
        lobby().SetPlayerState(1, PlayerState::AI, AI::Info(AI::Type::Dummy));
        lobby().SetPlayerState(2, PlayerState::AI, AI::Info(AI::Type::Dummy));
        pumpUntil(
          [] {
              const auto lobby = GAMECLIENT.GetGameLobby();
              return lobby->getPlayer(1).ps == PlayerState::AI && lobby->getPlayer(2).ps == PlayerState::AI;
          },
          "dummy AI slots");
        startGame();
        for(auto& entry : SETTINGS.windows.persistentSettings)
            entry.second.isOpen = false;
        LOADER.LoadDummyMapFiles();
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
        box().Clear();
    }
    PostBox& box()
    {
        auto* result = world().GetPostMgr().GetPostBox(0);
        BOOST_TEST_REQUIRE(result != nullptr);
        return *result;
    }
    void frame()
    {
        dsk->UpdateInput(16, Position(-10000, -10000));
        WINDOWMANAGER.Draw();
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
            for(unsigned i = 0; i < 8 && focused() != target; ++i)
                press(PadButton::DpadRight);
        } else
        {
            for(const auto direction : {PadButton::RightShoulder, PadButton::LeftShoulder})
                for(unsigned i = 0; i < 32 && focused() != target; ++i)
                    press(direction);
        }
        BOOST_TEST_REQUIRE(focused() == target);
    }
    void enter()
    {
        press(PadButton::Back);
        auto* system = dynamic_cast<iwPadSystemMenu*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(system != nullptr);
        focusUntil(system->GetCtrl<ctrlTextButton>(iwPadSystemMenu::ID_POST));
        press(PadButton::A);
        window = dynamic_cast<iwPostWindow*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(window != nullptr);
        BOOST_TEST_REQUIRE(dsk->GetPlayerView(0).GetFocus().GetRoot() == window);
    }
    void activate(const unsigned id)
    {
        auto* control = window->GetCtrl<ctrlImageButton>(id);
        BOOST_TEST_REQUIRE(control != nullptr);
        BOOST_TEST_REQUIRE(control->IsVisible());
        focusUntil(control);
        press(PadButton::A);
    }
    void mouseClick(const unsigned id)
    {
        const auto* control = window->GetCtrl<ctrlImageButton>(id);
        BOOST_TEST_REQUIRE(control != nullptr);
        BOOST_TEST_REQUIRE(control->IsVisible());
        const MouseCoords mc(control->GetDrawPos() + control->GetSize() / 2u);
        WINDOWMANAGER.Msg_LeftDown(mc);
        WINDOWMANAGER.Msg_LeftUp(mc);
        frame();
    }
    void key(const KeyEvent& event)
    {
        WINDOWMANAGER.Msg_KeyDown(event);
        frame();
    }
    Letter add(const unsigned number, const PostCategory category, const MapPoint pos = MapPoint::Invalid())
    {
        world().GetPostMgr().SendMsg(
          0, std::make_unique<PostMsg>(number * 10u, "Letter " + std::to_string(number), category, pos));
        const auto* message = box().GetMsg(box().GetNumMsgs() - 1u);
        return {*message, message};
    }
    std::vector<Letter> seed()
    {
        std::vector<Letter> result;
        for(unsigned i = 0; i < categories.size(); ++i)
            result.push_back(add(i, categories[i]));
        return result;
    }
    void expectMessage(const Letter& expected, const unsigned ordinal, const unsigned count) const
    {
        const auto* text = window->GetCtrl<ctrlMultiline>(messageText);
        BOOST_TEST_REQUIRE(text->GetNumLines() == 1u);
        BOOST_TEST(text->GetLine(0) == expected.snapshot.GetText());
        const auto* info = window->GetCtrl<ctrlText>(infoText);
        BOOST_TEST(info->IsVisible());
        BOOST_TEST(info->GetText()
                   == "Message " + std::to_string(ordinal) + "/" + std::to_string(count)
                        + " - Time: " + GAMECLIENT.FormatGFTime(expected.snapshot.GetSendFrame()));
        BOOST_TEST(window->GetCtrl<Window>(deleteLetter)->IsVisible());
        BOOST_TEST(window->GetCtrl<Window>(goTo)->IsVisible() == expected.snapshot.GetPos().isValid());
        BOOST_TEST(!window->GetCtrl<Window>(17)->IsVisible());
        BOOST_TEST(!window->GetCtrl<Window>(18)->IsVisible());
    }
    void expectEmpty() const
    {
        const auto* text = window->GetCtrl<ctrlMultiline>(messageText);
        BOOST_TEST_REQUIRE(text->GetNumLines() == 1u);
        BOOST_TEST(text->GetLine(0) == "No letters!");
        for(const unsigned id : {deleteLetter, goTo, infoText, 17u, 18u})
            BOOST_TEST(!window->GetCtrl<Window>(id)->IsVisible());
    }
    void expectWorld(const Inventory& inventory, const AsyncChecksum& checksum)
    {
        for(const auto good : helpers::EnumRange<GoodType>{})
            BOOST_TEST(world().GetPlayer(0).GetInventory()[good] == inventory[good]);
        for(const auto job : helpers::EnumRange<Job>{})
            BOOST_TEST(world().GetPlayer(0).GetInventory()[job] == inventory[job]);
        BOOST_TEST((AsyncChecksum::create(*ci().game) == checksum));
        BOOST_TEST(ci().numErrors == 0u);
        BOOST_TEST(ci().numAsync == 0u);
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(PadPostOfficeTests)

BOOST_FIXTURE_TEST_CASE(AllCategoriesAndTimelineBoundariesFollowPhysicalControllerAndMouseInput, PostPadFixture)
{
    run([this] {
        const auto letters = seed();
        const auto inventory = world().GetPlayer(0).GetInventory();
        const auto checksum = AsyncChecksum::create(*ci().game);
        enter();
        expectMessage(letters[8], 9, 9);
        for(const auto& filter : {std::make_pair(general, std::vector<unsigned>{0, 4}),
                                  std::make_pair(military, std::vector<unsigned>{1, 5, 8}),
                                  std::make_pair(geologist, std::vector<unsigned>{2, 6}),
                                  std::make_pair(economy, std::vector<unsigned>{3, 7})})
        {
            activate(filter.first);
            const auto& ids = filter.second;
            expectMessage(letters[ids.back()], ids.size(), ids.size());
            activate(oldest);
            expectMessage(letters[ids.front()], 1, ids.size());
            activate(previous);
            expectMessage(letters[ids.front()], 1, ids.size());
            for(unsigned i = 1; i < ids.size(); ++i)
            {
                activate(next);
                expectMessage(letters[ids[i]], i + 1, ids.size());
            }
            activate(next);
            expectMessage(letters[ids.back()], ids.size(), ids.size());
            mouseClick(oldest);
            mouseClick(newest);
            expectMessage(letters[ids.back()], ids.size(), ids.size());
            key(KeyEvent('-'));
            expectMessage(letters[ids[ids.size() - 2]], ids.size() - 1, ids.size());
            key(KeyEvent('+'));
            expectMessage(letters[ids.back()], ids.size(), ids.size());
        }
        activate(all);
        expectMessage(letters[8], 9, 9);
        mouseClick(oldest);
        expectMessage(letters[0], 1, 9);
        BOOST_TEST(box().GetNumMsgs() == 9u);
        expectWorld(inventory, checksum);
    });
}

BOOST_FIXTURE_TEST_CASE(DeletingTheSelectedFilteredLetterKeepsOtherCategoriesAndChoosesTheNextLetter, PostPadFixture)
{
    run([this] {
        const auto letters = seed();
        const auto inventory = world().GetPlayer(0).GetInventory();
        const auto checksum = AsyncChecksum::create(*ci().game);
        enter();
        activate(military);
        activate(oldest);
        activate(next);
        expectMessage(letters[5], 2, 3);
        activate(deleteLetter);
        BOOST_TEST(box().GetNumMsgs() == 8u);
        BOOST_TEST(box().GetMsg(5) == letters[6].identity);
        expectMessage(letters[8], 2, 2);
        mouseClick(deleteLetter);
        expectMessage(letters[1], 1, 1);
        key(KeyEvent(KeyType::Delete));
        expectEmpty();
        BOOST_TEST(box().GetNumMsgs() == 6u);
        const std::array<unsigned, 6> survivors = {0, 2, 3, 4, 6, 7};
        for(unsigned i = 0; i < survivors.size(); ++i)
            BOOST_TEST(box().GetMsg(i) == letters[survivors[i]].identity);
        activate(all);
        expectMessage(letters[7], 6, 6);
        expectWorld(inventory, checksum);
    });
}

BOOST_FIXTURE_TEST_CASE(DeletingEarlierInterleavedLettersPreservesTheSelectedFilteredLetter, PostPadFixture)
{
    run([this] {
        const auto letters = seed();
        enter();
        activate(military);
        activate(oldest);
        activate(next);
        expectMessage(letters[5], 2, 3);
        BOOST_TEST_REQUIRE(box().DeleteMsg(letters[1].identity));
        frame();
        expectMessage(letters[5], 1, 2);
        BOOST_TEST_REQUIRE(box().DeleteMsg(letters[0].identity));
        frame();
        expectMessage(letters[5], 1, 2);
        activate(deleteLetter);
        expectMessage(letters[8], 1, 1);
        BOOST_TEST(box().GetNumMsgs() == 6u);
    });
}

BOOST_FIXTURE_TEST_CASE(FullInboxEvictionPreservesTheSelectedFilteredLetterAndItsDeleteTarget, PostPadFixture)
{
    run([this] {
        const auto first = add(0, PostCategory::Military);
        add(1, PostCategory::Economy);
        const auto selected = add(2, PostCategory::Military);
        add(3, PostCategory::Economy);
        const auto last = add(4, PostCategory::Military);
        for(unsigned i = 5; i < PostBox::GetMaxMsgs(); ++i)
            add(i, PostCategory::General);
        enter();
        activate(military);
        activate(oldest);
        expectMessage(first, 1, 3);
        activate(next);
        expectMessage(selected, 2, 3);
        add(20, PostCategory::Geologist);
        BOOST_TEST(box().GetNumMsgs() == PostBox::GetMaxMsgs());
        frame();
        expectMessage(selected, 1, 2);
        activate(deleteLetter);
        expectMessage(last, 1, 1);
        BOOST_TEST(box().GetNumMsgs() == PostBox::GetMaxMsgs() - 1);
        BOOST_TEST(box().GetMsg(2) == last.identity);
    });
}

BOOST_FIXTURE_TEST_CASE(SameCountReplacementRefreshesFilteredNavigationWhileRetainingSelection, PostPadFixture)
{
    run([this] {
        const auto letters = seed();
        enter();
        activate(military);
        activate(oldest);
        expectMessage(letters[1], 1, 3);
        BOOST_TEST_REQUIRE(box().DeleteMsg(letters[8].identity));
        const auto incoming = add(99, PostCategory::Economy);
        BOOST_TEST(box().GetNumMsgs() == 9u);
        frame();
        expectMessage(letters[1], 1, 2);
        activate(newest);
        expectMessage(letters[5], 2, 2);
        activate(economy);
        expectMessage(incoming, 3, 3);
    });
}

BOOST_FIXTURE_TEST_CASE(EmptyCategoriesReceiveNewLettersAndRecoverAfterDeletingTheLastOne, PostPadFixture)
{
    run([this] {
        enter();
        expectEmpty();
        for(const unsigned id : {oldest, previous, next, newest})
        {
            activate(id);
            expectEmpty();
        }
        activate(military);
        const auto generalLetter = add(0, PostCategory::General);
        frame();
        expectEmpty();
        const auto incoming = add(1, PostCategory::Military);
        frame();
        expectMessage(incoming, 1, 1);
        activate(deleteLetter);
        expectEmpty();
        BOOST_TEST(box().GetNumMsgs() == 1u);
        activate(general);
        expectMessage(generalLetter, 1, 1);
    });
}

BOOST_FIXTURE_TEST_CASE(HelpDiaryAndPositionLinksLeaveTheRunningWorldAndInboxUnchanged, PostPadFixture)
{
    run([this] {
        const MapPoint position(5, 7);
        const auto letter = add(0, PostCategory::General, position);
        const auto inventory = world().GetPlayer(0).GetInventory();
        const auto checksum = AsyncChecksum::create(*ci().game);
        enter();
        expectMessage(letter, 1, 1);
        BOOST_TEST(!window->GetCtrl<Window>(goal)->IsVisible());
        const auto expectedOffset = [this](const MapPoint point) {
            const auto& view = dsk->GetPlayerView(0).GetView();
            auto result = world().GetNodePos(point) - view.GetSize() / 2u;
            const DrawPoint mapSize(world().GetWidth() * TR_W, world().GetHeight() * TR_H);
            result.x = (result.x % mapSize.x + mapSize.x) % mapSize.x;
            result.y = (result.y % mapSize.y + mapSize.y) % mapSize.y;
            return result;
        };
        BOOST_TEST_REQUIRE((dsk->GetPlayerView(0).GetView().GetOffset() != expectedOffset(position)));
        activate(goTo);
        BOOST_TEST((dsk->GetPlayerView(0).GetView().GetOffset() == expectedOffset(position)));
        const MapPoint otherPosition(20, 15);
        const auto otherLetter = add(1, PostCategory::General, otherPosition);
        frame();
        activate(newest);
        expectMessage(otherLetter, 2, 2);
        mouseClick(goTo);
        BOOST_TEST((dsk->GetPlayerView(0).GetView().GetOffset() == expectedOffset(otherPosition)));
        activate(oldest);
        activate(help);
        BOOST_TEST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(dynamic_cast<iwHelp*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        press(PadButton::Y);
        box().SetCurrentMissionGoal("Connect a sawmill to the castle.");
        frame();
        BOOST_TEST(window->GetCtrl<Window>(goal)->IsVisible());
        activate(goal);
        auto* diary = dynamic_cast<iwMissionStatement*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(diary != nullptr);
        BOOST_TEST(diary->GetCtrl<ctrlMultiline>(0)->GetLine(0) == box().GetCurrentMissionGoal());
        BOOST_TEST(!GAMECLIENT.IsPaused());
        press(PadButton::Y);
        focusUntil(diary->GetCtrl<ctrlTextButton>(1));
        press(PadButton::A);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        box().SetCurrentMissionGoal("");
        frame();
        BOOST_TEST(!window->GetCtrl<Window>(goal)->IsVisible());
        expectMessage(letter, 1, 2);
        BOOST_TEST(box().GetNumMsgs() == 2u);
        expectWorld(inventory, checksum);
        press(PadButton::Y);
        press(PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        press(PadButton::B);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        window = nullptr;
    });
}

BOOST_AUTO_TEST_CASE(ExceptionalCleanupRestoresSettingsBeforeFixtureDestruction)
{
    {
        PostPadFixture fixture;
        bool reachedProbe = false;
        BOOST_CHECK_THROW(fixture.run([&fixture, &reachedProbe] {
            fixture.enter();
            reachedProbe = true;
            throwCleanupProbe();
        }),
                          std::runtime_error);
        BOOST_TEST(reachedProbe);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        fixture.expectRestored();
    }
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
}

BOOST_AUTO_TEST_SUITE_END()
