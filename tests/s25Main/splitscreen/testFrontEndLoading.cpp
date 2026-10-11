// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "LocalGameFixture.h"
#include "MenuPadFixture.h"
#include "PadFixture.h"
#include "controls/ctrlMultiline.h"
#include "controls/ctrlText.h"
#include "controls/ctrlTimer.h"
#include "desktops/dskDirectIP.h"
#include "desktops/dskGameLoader.h"
#include "desktops/dskHome.h"
#include "desktops/dskSinglePlayer.h"
#include "desktops/dskTitle.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "ingameWindows/iwMsgbox.h"
#include "postSystem/PostBox.h"
#include "postSystem/PostManager.h"
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace {
using Page = dskGameLoader;
constexpr PadDeviceId pad = 97;

[[noreturn]] void cleanupProbe()
{
    throw std::runtime_error("deliberate loading-fixture cleanup probe");
}

// Original S2 graphics are the sole bypass. The timed loader, real views, postboxes,
// client/server GameLoaded handshake and first simulation frames stay in production code.
class GraphicsBypass : public Page
{
public:
    explicit GraphicsBypass(std::shared_ptr<Game> game, unsigned failureMode = 0)
        : Page(game), game_(std::move(game)), failureMode_(failureMode)
    {}

protected:
    bool LoadTextures() override
    {
        LOADER.LoadDummyMapFiles();
        if(failureMode_ == 2)
            CI_Error(ClientError::ConnectionLost);
        return true;
    }
    std::unique_ptr<dskGameInterface> CreateGameInterface() override
    {
        if(failureMode_ == 1)
            throw std::runtime_error("deliberate game-view initialization failure");
        if(failureMode_ == 3)
            CI_Error(ClientError::ConnectionLost);
        return std::make_unique<rttr::test::TestableGameInterface>(game_, GAMECLIENT.GetNWFInfo(),
                                                                   GAMECLIENT.GetPlayerId(), false);
    }

private:
    std::shared_ptr<Game> game_;
    unsigned failureMode_;
};

struct LoadingFixture : rttr::test::LocalGameFixture, rttr::test::MenuPadFixture
{
    const decltype(SETTINGS.video) savedVideo = SETTINGS.video;
    const VideoMode savedSize = VIDEODRIVER.GetWindowSize();
    const DisplayMode savedMode = VIDEODRIVER.GetDisplayMode();
    const decltype(SETTINGS.windows.persistentSettings) savedWindows = SETTINGS.windows.persistentSettings;
    const bool savedSmartCursor = SETTINGS.global.smartCursor;
    const unsigned savedScale = VIDEODRIVER.getGuiScale().percent();

    void frame() override
    {
        video.tickCount_ += frameMs;
        pump();
        WINDOWMANAGER.Draw();
    }
    void tick()
    {
        // ctrlTimer uses real elapsed time, not the mock driver's tick count.
        std::this_thread::sleep_for(std::chrono::milliseconds(55));
        frame();
    }
    template<class F>
    bool until(const F& done)
    {
        for(unsigned i = 0; i < 10000 && !done(); ++i)
        {
            frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return done();
    }
    void cleanup()
    {
        GAMECLIENT.Stop();
        GAMESERVER.Stop();
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        frame();
        GAMECLIENT.SetInterface(&ci());
        VIDEODRIVER.setGuiScalePercent(savedScale);
        VIDEODRIVER.ResizeScreen(savedSize, savedMode);
        SETTINGS.video = savedVideo;
        SETTINGS.windows.persistentSettings = savedWindows;
        SETTINGS.global.smartCursor = savedSmartCursor;
        padInput().Reset();
        video.padEvents_.clear();
        dskHome::ForgetLastChoice();
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
        BOOST_TEST((SETTINGS.video.windowedSize == savedVideo.windowedSize));
        BOOST_TEST((VIDEODRIVER.GetWindowSize() == savedSize));
        BOOST_TEST(SETTINGS.global.smartCursor == savedSmartCursor);
    }
    template<class F>
    void checked(const F& body)
    {
        try
        {
            body();
        } catch(...)
        {
            cleanup();
            throw;
        }
        cleanup();
    }
    void prepare(unsigned controllers = 2)
    {
        for(auto& entry : SETTINGS.windows.persistentSettings)
            entry.second.isOpen = false;
        // A new game's actual mission onStart checks that loading created postboxes first.
        boost::nowide::ofstream(boost::filesystem::path(mapPath()).replace_extension("lua").string())
          << "function getRequiredLuaVersion() return 1 end\n"
             "function onStart(first) rttr:SetMissionGoal(0,'Loading mission goal') end\n";
        WINDOWMANAGER.Switch(std::make_unique<dskTitle>());
        frame();
        for(unsigned i = 0; i < controllers; ++i)
        {
            connect(pad + i);
            press(pad + i, PadButton::A);
        }
        BOOST_TEST_REQUIRE(padInput().GetParty().Members().size() == controllers);
        frontend::SetMenuStyle(frontend::MenuStyle::FrontEnd);
        hostAndEnterLobby();
        for(unsigned i = 1; i < GAMECLIENT.GetGameLobby()->getNumPlayers(); ++i)
            lobby().CloseSlot(i);
        pumpUntil(
          [] {
              const auto current = GAMECLIENT.GetGameLobby();
              for(unsigned i = 1; i < current->getNumPlayers(); ++i)
              {
                  if(current->getPlayer(i).ps != PlayerState::Locked)
                      return false;
              }
              return true;
          },
          "scripted solo lobby slots closed");
        GAMECLIENT.SetSharedLocalViews(controllers - 1);
        GAMECLIENT.Command_SetReady(true);
        lobby().StartCountdown(0);
        pumpUntil([] { return GAMECLIENT.GetState() == ClientState::Loading; }, "real loading state");
        BOOST_TEST_REQUIRE(ci().game);
    }
    Page& show(bool bypass = true, unsigned failureMode = 0)
    {
        if(bypass)
            WINDOWMANAGER.Switch(std::make_unique<GraphicsBypass>(ci().game, failureMode));
        else
            WINDOWMANAGER.Switch(std::make_unique<Page>(ci().game));
        frame();
        auto* page = desktopAs<Page>();
        BOOST_TEST_REQUIRE(page);
        return *page;
    }
    void click(const Window& control)
    {
        MouseCoords mouse(control.GetDrawPos() + Position(control.GetSize() / 2u));
        WINDOWMANAGER.Msg_MouseMove(mouse);
        mouse.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mouse);
        mouse.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mouse);
        frame();
    }
    void acknowledge(bool controller)
    {
        auto* error = dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(error);
        if(controller)
            press(pad, PadButton::A);
        else
            click(*error->GetCtrl<Window>(2));
        frame();
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(FrontEndLoadingTests)

BOOST_FIXTURE_TEST_CASE(RealTimedLoadCreatesViewsBeforeMissionGoalAndStartsTwoOrFourControllers, LoadingFixture)
{
    checked([&] {
        for(unsigned count : {2u, 4u})
        {
            prepare(count);
            auto& page = show();
            BOOST_TEST(page.GetCtrl<ctrlText>(Page::ID_MapTitle)->GetText() == GAMECLIENT.GetMapTitle());
            BOOST_TEST(!page.WantsPadInput());
            BOOST_TEST(!page.HasBackAction());
            BOOST_TEST(page.GetPlayerStrip().size() == count);
            BOOST_TEST(page.GetFooterKeys().empty());
            BOOST_TEST(page.GetCtrl<ctrlMultiline>(Page::ID_Tip)->GetNumLines() == 1u);
            BOOST_TEST(page.GetCtrl<ctrlMultiline>(Page::ID_Controls)->GetNumLines() == 1u);
            BOOST_TEST_REQUIRE(until([] { return desktopAs<rttr::test::TestableGameInterface>(); }));
            auto* game = desktopAs<rttr::test::TestableGameInterface>();
            BOOST_TEST_REQUIRE(game);
            BOOST_TEST((GAMECLIENT.GetState() == ClientState::Game));
            BOOST_TEST(ci().game->IsStarted());
            BOOST_TEST(game->GetNumViews() == count);
            for(unsigned i = 0; i < count; ++i)
            {
                BOOST_TEST(game->GetPlayerView(i).GetPlayerId() == GAMECLIENT.GetPlayerId());
                BOOST_TEST((game->GetPadRouter().GetSlot(pad + i) == i));
            }
            const auto* box = ci().game->world_.GetPostMgr().GetPostBox(GAMECLIENT.GetPlayerId());
            BOOST_TEST_REQUIRE(box);
            BOOST_TEST(box->GetCurrentMissionGoal() == "Loading mission goal");
            GAMECLIENT.SetInterface(&ci());
            const unsigned first = GAMECLIENT.GetGFNumber();
            BOOST_TEST_REQUIRE(until([first] { return GAMECLIENT.GetGFNumber() >= first + 12; }));
            BOOST_TEST(ci().numErrors == 0u);
            BOOST_TEST(ci().numAsync == 0u);
            cleanup();
        }
    });
}

BOOST_FIXTURE_TEST_CASE(StagesDescribeCompletedWorkAndTheActualClientWait, LoadingFixture)
{
    checked([&] {
        prepare();
        auto& page = show();
        for(const auto* status : {_("Map loaded"), _("Preparing tribes..."), _("Loading game resources..."),
                                  _("Creating game views..."), _("Waiting for the other players...")})
        {
            tick();
            BOOST_TEST(page.GetCtrl<ctrlText>(Page::ID_Status)->GetText() == status);
        }
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Loaded));
        BOOST_TEST(!ci().game->IsStarted());
        BOOST_TEST_REQUIRE(ci().game->world_.GetPostMgr().GetPostBox(GAMECLIENT.GetPlayerId()));
        BOOST_TEST_REQUIRE(until([] { return desktopAs<rttr::test::TestableGameInterface>(); }));
        BOOST_TEST(ci().game->IsStarted());
    });
}

BOOST_FIXTURE_TEST_CASE(ReentrantResourceOrViewErrorsCannotOverwriteFailureOrRestartTheTimer, LoadingFixture)
{
    checked([&] {
        for(unsigned mode : {2u, 3u})
        {
            prepare();
            auto& page = show(true, mode);
            BOOST_TEST_REQUIRE(until([] { return WINDOWMANAGER.GetTopMostWindow(); }));
            BOOST_TEST(page.GetCtrl<ctrlText>(Page::ID_Status)->GetText() == _("Loading failed"));
            BOOST_TEST(!page.GetCtrl<ctrlTimer>(Page::ID_Timer)->isRunning());
            BOOST_TEST((GAMECLIENT.GetState() == ClientState::Loading));
            tick();
            BOOST_TEST(page.GetCtrl<ctrlText>(Page::ID_Status)->GetText() == _("Loading failed"));
            BOOST_TEST(!page.GetCtrl<ctrlTimer>(Page::ID_Timer)->isRunning());
            acknowledge(true);
            BOOST_TEST_REQUIRE(desktopAs<dskHome>());
            cleanup();
        }
    });
}

BOOST_FIXTURE_TEST_CASE(PhysicalResizeAndPendingActivationKeepContentInFrameWithoutChangingParty, LoadingFixture)
{
    checked([&] {
        prepare();
        auto& page = show();
        for(const auto size : {VideoMode(800, 600), VideoMode(1280, 800)})
        {
            VIDEODRIVER.ResizeScreen(size, DisplayMode::Windowed);
            frame();
            BOOST_TEST((page.GetSize() == VIDEODRIVER.GetRenderSize()));
            const auto& area = page.GetFrame().content;
            for(const unsigned id : {Page::ID_Tip, Page::ID_Controls})
            {
                const auto* control = page.GetCtrl<Window>(id);
                if(control->IsVisible())
                {
                    const auto rect = control->GetDrawRect();
                    BOOST_TEST(rect.left >= area.left);
                    BOOST_TEST(rect.top >= area.top);
                    BOOST_TEST(rect.right <= area.right);
                    BOOST_TEST(rect.bottom <= area.bottom);
                }
            }
            BOOST_TEST(page.GetCtrl<Window>(Page::ID_Controls)->IsVisible() == true);
            BOOST_TEST(padInput().GetParty().Members().size() == 2u);
        }
        // Resize callbacks retain the legacy minimum. Actual small render units apply on entry.
        VIDEODRIVER.ResizeScreen(VideoMode(400, 240), DisplayMode::Windowed);
        frame();
        BOOST_TEST(page.GetSize().x >= VideoDriverWrapper::MinWindowSize.width);
        WINDOWMANAGER.Switch(std::make_unique<GraphicsBypass>(ci().game));
        frame();
        auto* small = desktopAs<Page>();
        BOOST_TEST_REQUIRE(small);
        BOOST_TEST_REQUIRE(small->GetFrame().content.getSize().y < 180u);
        BOOST_TEST(!small->GetCtrl<Window>(Page::ID_Controls)->IsVisible());
        BOOST_TEST(!small->GetCtrl<Window>(Page::ID_Tip)->IsVisible());
        VIDEODRIVER.ResizeScreen(VideoMode(1280, 800), DisplayMode::Windowed);
        frame();
        BOOST_TEST(small->GetCtrl<Window>(Page::ID_Controls)->IsVisible());
        WINDOWMANAGER.Switch(std::make_unique<GraphicsBypass>(ci().game));
        VIDEODRIVER.ResizeScreen(VideoMode(800, 600), DisplayMode::Windowed);
        frame();
        BOOST_TEST((desktopAs<Page>()->GetSize() == VIDEODRIVER.GetRenderSize()));
        BOOST_TEST(desktopAs<Page>()->GetCtrl<Window>(Page::ID_Controls)->IsVisible());
    });
}

BOOST_FIXTURE_TEST_CASE(LoadingConsumesNoPadEventsAndErrorAcknowledgementCannotReachHomeInTheSameBatch, LoadingFixture)
{
    checked([&] {
        prepare();
        auto& page = show();
        for(const auto button : {PadButton::A, PadButton::B, PadButton::Start, PadButton::DpadDown})
            tap(pad, button);
        const auto pending = video.padEvents_.size();
        frame();
        BOOST_TEST(video.padEvents_.size() == pending);
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Return));
        frame();
        BOOST_TEST(desktopAs<Page>() == &page);
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Loading));
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        video.padEvents_.clear();
        page.CI_Error(ClientError::ConnectionLost);
        page.LC_Status_Error("second failure must not open another modal");
        auto* error = WINDOWMANAGER.GetTopMostWindow();
        BOOST_TEST_REQUIRE(error);
        BOOST_TEST(page.WantsPadInput());
        BOOST_TEST(!page.GetCtrl<ctrlTimer>(Page::ID_Timer)->isRunning());
        page.CI_GameStarted();
        BOOST_TEST(!WINDOWMANAGER.IsSwitchPending());
        tap(pad, PadButton::A);
        tap(pad, PadButton::B);
        frame();
        frame();
        BOOST_TEST_REQUIRE(desktopAs<dskHome>());
        BOOST_TEST(padInput().GetParty().Members().size() == 2u);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
    });
}

BOOST_FIXTURE_TEST_CASE(RealMissingResourcesStopLoadingAndPadAcknowledgementReturnsHome, LoadingFixture)
{
    checked([&] {
        prepare();
        rttr::test::TmpFolder empty;
        rttr::test::ConfigOverride gameOverride("GAME", empty);
        auto& page = show(false);
        BOOST_TEST_REQUIRE(until([] { return WINDOWMANAGER.GetTopMostWindow(); }));
        BOOST_TEST(page.GetCtrl<ctrlText>(Page::ID_Status)->GetText() == _("Loading failed"));
        BOOST_TEST(!page.GetCtrl<ctrlTimer>(Page::ID_Timer)->isRunning());
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Loading));
        tick();
        BOOST_TEST(desktopAs<Page>() == &page);
        acknowledge(true);
        BOOST_TEST_REQUIRE(desktopAs<dskHome>());
    });
}

BOOST_FIXTURE_TEST_CASE(ViewInitializationFailureHasOneModalAndMouseCanAcknowledgeIt, LoadingFixture)
{
    checked([&] {
        prepare();
        auto& page = show(true, 1);
        BOOST_TEST_REQUIRE(until([] { return WINDOWMANAGER.GetTopMostWindow(); }));
        auto* error = WINDOWMANAGER.GetTopMostWindow();
        page.CI_Error(ClientError::ConnectionLost);
        frame();
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == error);
        BOOST_TEST(!page.GetCtrl<ctrlTimer>(Page::ID_Timer)->isRunning());
        acknowledge(false);
        BOOST_TEST_REQUIRE(desktopAs<dskHome>());
    });
}

BOOST_FIXTURE_TEST_CASE(ErrorBeforeActivationIsShownAfterThePendingSwitchAndDoesNotAdvanceLoading, LoadingFixture)
{
    checked([&] {
        prepare();
        auto pending = std::make_unique<GraphicsBypass>(ci().game);
        pending->CI_Error(ClientError::ConnectionLost);
        WINDOWMANAGER.Switch(std::move(pending));
        frame();
        auto* page = desktopAs<Page>();
        BOOST_TEST_REQUIRE(page);
        BOOST_TEST_REQUIRE(dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow()));
        BOOST_TEST(page->GetCtrl<ctrlText>(Page::ID_Status)->GetText() == _("Loading failed"));
        BOOST_TEST(!page->GetCtrl<ctrlTimer>(Page::ID_Timer)->isRunning());
        acknowledge(true);
        BOOST_TEST_REQUIRE(desktopAs<dskHome>());
    });
}

BOOST_FIXTURE_TEST_CASE(ClassicKeepsOriginalTextsAndMouseErrorReturnToSinglePlayer, LoadingFixture)
{
    checked([&] {
        prepare();
        frontend::SetMenuStyle(frontend::MenuStyle::Classic);
        auto& page = show();
        BOOST_TEST(!page.GetCtrl<Window>(Page::ID_Tip));
        BOOST_TEST(!page.GetCtrl<Window>(Page::ID_txtTitle)->IsVisible());
        tick();
        tick();
        BOOST_TEST(page.GetCtrl<ctrlText>(Page::ID_MapTitle)->GetText() == GAMECLIENT.GetMapTitle());
        BOOST_TEST(page.GetCtrl<ctrlText>(Page::ID_Status)->GetText() == _("Map was loaded and pinned at the wall..."));
        page.CI_Error(ClientError::ConnectionLost);
        acknowledge(false);
        BOOST_TEST_REQUIRE(desktopAs<dskSinglePlayer>());
    });
}

BOOST_FIXTURE_TEST_CASE(ClassicCompletesTheSameRealLoadingHandshakeAndMissionStart, LoadingFixture)
{
    checked([&] {
        prepare();
        frontend::SetMenuStyle(frontend::MenuStyle::Classic);
        show();
        BOOST_TEST_REQUIRE(until([] { return desktopAs<rttr::test::TestableGameInterface>(); }));
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Game));
        BOOST_TEST(ci().game->IsStarted());
        const auto* box = ci().game->world_.GetPostMgr().GetPostBox(GAMECLIENT.GetPlayerId());
        BOOST_TEST_REQUIRE(box);
        BOOST_TEST(box->GetCurrentMissionGoal() == "Loading mission goal");
    });
}

BOOST_FIXTURE_TEST_CASE(ExceptionalFixtureCleanupStopsARealStartedGameAndAllowsAnotherLoadingRun, LoadingFixture)
{
    bool reached = false;
    BOOST_CHECK_THROW(checked([&] {
                          prepare();
                          show();
                          BOOST_TEST_REQUIRE(until([] { return desktopAs<rttr::test::TestableGameInterface>(); }));
                          BOOST_TEST_REQUIRE((GAMECLIENT.GetState() == ClientState::Game));
                          reached = true;
                          cleanupProbe();
                      }),
                      std::runtime_error);
    BOOST_TEST_REQUIRE(reached);
    BOOST_TEST((GAMECLIENT.GetState() == ClientState::Stopped));
    BOOST_TEST((VIDEODRIVER.GetWindowSize() == savedSize));
    checked([&] {
        prepare();
        auto& page = show();
        page.CI_Error(ClientError::ConnectionLost);
        acknowledge(true);
        BOOST_TEST_REQUIRE(desktopAs<dskHome>());
    });
}

BOOST_AUTO_TEST_SUITE_END()
