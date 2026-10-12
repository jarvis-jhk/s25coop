// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "LocalGameFixture.h"
#include "PadFixture.h"
#include "Savegame.h"
#include "controls/ctrlComboBox.h"
#include "controls/ctrlEdit.h"
#include "controls/ctrlImageButton.h"
#include "controls/ctrlTable.h"
#include "controls/ctrlTextButton.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "files.h"
#include "frontend/SavePreview.h"
#include "helpers/containerUtils.h"
#include "helpers/optional_io.h"
#include "ingameWindows/iwMainMenu.h"
#include "ingameWindows/iwMsgbox.h"
#include "ingameWindows/iwOptionsWindow.h"
#include "ingameWindows/iwPadSystemMenu.h"
#include "ingameWindows/iwSave.h"
#include "gameData/GameConsts.h"
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#ifndef _WIN32
#    include <csignal>
#    include <sys/resource.h>
#    include <sys/wait.h>
#    include <unistd.h>
#endif

namespace {
constexpr PadDeviceId pad = 102;

[[noreturn]] void throwCleanupProbe()
{
    throw std::runtime_error("save cleanup probe");
}

struct SavePadFixture : uiHelper::Fixture, rttr::test::LocalGameFixture
{
    rttr::test::PadFeeder pads{*uiHelper::GetVideoDriver()};
    rttr::test::TestableGameInterface* dsk = nullptr;
    iwSave* window = nullptr;
    const bool savedDebugMode = SETTINGS.global.debugMode;
    const boost::filesystem::path saveDir = RTTRCONFIG.ExpandPath(s25::folders::save);

    template<class F>
    void run(F&& body)
    {
        // Destroy the real desktop and its windows while the game and its interfaces still exist.
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
    }

    void cleanup()
    {
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        WINDOWMANAGER.Draw();
        if(ci().game)
            world().SetGameInterface(nullptr);
        dsk = nullptr;
        window = nullptr;
        SETTINGS.global.debugMode = savedDebugMode;
    }

    void beginGame()
    {
        SETTINGS.global.debugMode = false;
        boost::filesystem::create_directories(saveDir);
        hostAndEnterLobby();
        lobby().SetPlayerState(1, PlayerState::AI, AI::Info(AI::Type::Dummy));
        lobby().SetPlayerState(2, PlayerState::AI, AI::Info(AI::Type::Dummy));
        pumpUntil(
          [] {
              const auto lobby = GAMECLIENT.GetGameLobby();
              return lobby->getPlayer(1).ps == PlayerState::AI && lobby->getPlayer(2).ps == PlayerState::AI;
          },
          "dummy AI configuration");
        startGame();
        LOADER.LoadDummyMapFiles();
        auto desktop = std::make_unique<rttr::test::TestableGameInterface>(ci().game, GAMECLIENT.GetNWFInfo(),
                                                                           GAMECLIENT.GetPlayerId(), false);
        dsk = desktop.get();
        WINDOWMANAGER.Switch(std::move(desktop));
        WINDOWMANAGER.Draw();
        BOOST_TEST_REQUIRE(dsk->GetNumViews() == 1u);
        pads.pickUp(pad);
        frame();
        BOOST_TEST_REQUIRE(dsk->GetPlayerView(0).HasPadCursor());
    }

    void frame() const
    {
        dsk->UpdateInput(16, Position(-10000, -10000));
        WINDOWMANAGER.Draw();
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

    template<class T>
    static T& only(Window& parent)
    {
        const auto controls = parent.GetCtrls<T>();
        BOOST_TEST_REQUIRE(controls.size() == 1u);
        return *controls.front();
    }

    void enter()
    {
        press(PadButton::Back);
        auto* system = dynamic_cast<iwPadSystemMenu*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(system != nullptr);
        focusUntil(system->GetCtrl<ctrlTextButton>(iwPadSystemMenu::ID_MAIN_SELECTION));
        press(PadButton::A);
        auto* main = dynamic_cast<iwMainMenu*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(main != nullptr);
        ctrlImageButton* options = nullptr;
        for(auto* button : main->GetCtrls<ctrlImageButton>())
        {
            if(button->GetPos() == DrawPoint(12, 231))
                options = button;
        }
        BOOST_TEST_REQUIRE(options != nullptr);
        focusUntil(options);
        press(PadButton::A);
        openSaveFromGameMenu();
    }

    void openSaveFromGameMenu()
    {
        auto* gameMenu = dynamic_cast<iwOptionsWindow*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(gameMenu != nullptr);
        press(PadButton::Y);
        ctrlImageButton* save = nullptr;
        for(auto* button : gameMenu->GetCtrls<ctrlImageButton>())
        {
            if(button->GetPos() == DrawPoint(35, 221))
                save = button;
        }
        BOOST_TEST_REQUIRE(save != nullptr);
        focusUntil(save);
        press(PadButton::A);
        window = dynamic_cast<iwSave*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(window != nullptr);
        press(PadButton::Y);
        BOOST_TEST_REQUIRE(dsk->GetPlayerView(0).GetFocus().GetRoot() == window);
        BOOST_TEST_REQUIRE(focused() == (table().GetNumRows() ? static_cast<Window*>(&table()) : &saveButton()));
        BOOST_TEST_REQUIRE(!edit().CanFocus());
    }

    ctrlTable& table() const { return only<ctrlTable>(*window); }
    ctrlEdit& edit() const { return only<ctrlEdit>(*window); }
    ctrlComboBox& autosave() const { return only<ctrlComboBox>(*window); }
    ctrlImageButton& saveButton() const { return only<ctrlImageButton>(*window); }

    void typeFilename(const std::string& name) const
    {
        MouseCoords mc(edit().GetDrawPos() + DrawPoint(10, 10));
        mc.ldown = true;
        WINDOWMANAGER.Msg_LeftDown(mc);
        mc.ldown = false;
        WINDOWMANAGER.Msg_LeftUp(mc);
        BOOST_TEST_REQUIRE(edit().HasFocus());
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::End));
        const auto length = edit().GetText().size();
        for(unsigned i = 0; i < length; ++i)
            WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Backspace));
        for(const unsigned char c : name)
            WINDOWMANAGER.Msg_KeyDown(KeyEvent(c));
        frame();
        BOOST_TEST_REQUIRE(edit().GetText() == name);
    }

    void expectSave(const std::string& name, const SerializedGameData& snapshot, const unsigned gf) const
    {
        Savegame saved;
        BOOST_TEST_REQUIRE(saved.Load(saveDir / (name + ".sav"), SaveGameDataToLoad::All));
        BOOST_TEST(saved.start_gf == gf);
        BOOST_TEST(saved.GetNumPlayers() == 3u);
        BOOST_TEST(!saved.GetMapName().empty());
        BOOST_TEST_REQUIRE(saved.sgd.GetLength() == snapshot.GetLength());
        BOOST_CHECK_EQUAL_COLLECTIONS(saved.sgd.GetData(), saved.sgd.GetData() + saved.sgd.GetLength(),
                                      snapshot.GetData(), snapshot.GetData() + snapshot.GetLength());
        const auto preview = frontend::ReadSavePreview(saveDir / (name + ".sav"));
        BOOST_TEST_REQUIRE(preview.has_value());
        BOOST_TEST(preview->getWidth() == frontend::savePreviewWidth);
        BOOST_TEST(preview->getHeight() == frontend::savePreviewHeight);
        BOOST_TEST(helpers::contains_if(*preview, [](const auto color) { return color.asValue() != 0xFF000000u; }));
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(PadSaveDialogTests)

BOOST_FIXTURE_TEST_CASE(ControllerEntryAndSaveWriteTheActualGameSnapshot, SavePadFixture)
{
    run([this] {
        enter();
        BOOST_TEST(table().GetNumRows() == 0u);
        typeFilename("Replace me");
        typeFilename("   Controller");
        focusUntil(&saveButton());
        SerializedGameData snapshot;
        snapshot.MakeSnapshot(*ci().game);
        const auto gf = GAMECLIENT.GetGFNumber();
        press(PadButton::A);
        expectSave("Controller", snapshot, gf);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == window);
        BOOST_TEST(edit().GetText().empty());
        BOOST_TEST(table().GetNumRows() == 1u);
    });
}

BOOST_FIXTURE_TEST_CASE(KeyboardEnterAndControllerRowSelectionCanOverwriteAnExistingSave, SavePadFixture)
{
    run([this] {
        enter();
        typeFilename("Existing");
        SerializedGameData before;
        before.MakeSnapshot(*ci().game);
        const auto oldGF = GAMECLIENT.GetGFNumber();
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Return));
        frame();
        expectSave("Existing", before, oldGF);
        pumpUntilGF(oldGF + 20u);
        focusUntil(&table());
        press(PadButton::DpadDown);
        BOOST_TEST((table().GetSelection() == 0u));
        BOOST_TEST(edit().GetText() == "Existing");
        focusUntil(&saveButton());
        SerializedGameData after;
        after.MakeSnapshot(*ci().game);
        const auto gf = GAMECLIENT.GetGFNumber();
        BOOST_TEST_REQUIRE(gf > oldGF);
        press(PadButton::A);
        expectSave("Existing", after, gf);
        BOOST_TEST(table().GetNumRows() == 1u);
    });
}

#ifndef _WIN32
BOOST_FIXTURE_TEST_CASE(PartialPreviewWriteStillReturnsASuccessfulLoadableWorld, SavePadFixture)
{
    run([this] {
        enter();
        const auto path = saveDir / "Preview failure.sav";
        SerializedGameData snapshot;
        snapshot.MakeSnapshot(*ci().game);
        BOOST_TEST_REQUIRE(GAMECLIENT.SaveToFile(path));
        expectSave("Preview failure", snapshot, GAMECLIENT.GetGFNumber());
        const auto worldEnd = boost::filesystem::file_size(path) - 64016;
        rlimit limit{};
        BOOST_TEST_REQUIRE(getrlimit(RLIMIT_FSIZE, &limit) == 0);
        limit.rlim_cur = worldEnd + 16;
        // A child isolates the OS write-failure probe from the suite's log files and global limits.
        const auto child = fork();
        BOOST_TEST_REQUIRE(child >= 0);
        if(child == 0)
        {
            std::signal(SIGXFSZ, SIG_IGN);
            const bool limited = setrlimit(RLIMIT_FSIZE, &limit) == 0;
            const bool saved = limited && GAMECLIENT.SaveToFile(path);
            Savegame loaded;
            const bool loadable = loaded.Load(path, SaveGameDataToLoad::All);
            const auto size = boost::filesystem::file_size(path);
            const bool equal =
              loaded.sgd.GetLength() == snapshot.GetLength()
              && std::equal(loaded.sgd.GetData(), loaded.sgd.GetData() + loaded.sgd.GetLength(), snapshot.GetData());
            const bool good = saved && loadable && equal && size > worldEnd && size < worldEnd + 64016
                              && !frontend::ReadSavePreview(path);
            cleanup();
            GAMECLIENT.Stop();
            GAMESERVER.Stop();
            GAMECLIENT.SetInterface(&ci());
            // Normal exit also flushes coverage counters for the executed child-side assertions.
            std::exit(good ? 0 : 1);
        }
        int status = 0;
        BOOST_TEST_REQUIRE(waitpid(child, &status, 0) == child);
        BOOST_TEST_REQUIRE(WIFEXITED(status));
        BOOST_TEST(WEXITSTATUS(status) == 0);
        BOOST_TEST(!frontend::ReadSavePreview(path));
        Savegame loaded;
        BOOST_TEST_REQUIRE(loaded.Load(path, SaveGameDataToLoad::All));
        BOOST_TEST_REQUIRE(loaded.sgd.GetLength() == snapshot.GetLength());
        BOOST_CHECK_EQUAL_COLLECTIONS(loaded.sgd.GetData(), loaded.sgd.GetData() + loaded.sgd.GetLength(),
                                      snapshot.GetData(), snapshot.GetData() + snapshot.GetLength());
    });
}
#endif

BOOST_FIXTURE_TEST_CASE(EmptyAndReservedFilenamesRequireAcknowledgementAndNeverSave, SavePadFixture)
{
    run([this] {
        enter();
        for(const std::string name : {"", "CON"})
        {
            typeFilename(name);
            focusUntil(&saveButton());
            press(PadButton::A);
            auto* warning = dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow());
            BOOST_TEST_REQUIRE(warning != nullptr);
            press(PadButton::Y);
            press(PadButton::B);
            press(PadButton::B);
            BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == warning);
            BOOST_TEST(boost::filesystem::is_empty(saveDir));
            press(PadButton::Y);
            press(PadButton::A);
            BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
            BOOST_TEST(table().GetNumRows() == 0u);
            BOOST_TEST(edit().GetText() == name);
            BOOST_TEST(boost::filesystem::is_empty(saveDir));
            press(PadButton::Y);
        }
    });
}

BOOST_FIXTURE_TEST_CASE(BReleasesFocusThenClosesWithoutChangingExistingSaves, SavePadFixture)
{
    run([this] {
        SerializedGameData snapshot;
        snapshot.MakeSnapshot(*ci().game);
        const auto gf = GAMECLIENT.GetGFNumber();
        BOOST_TEST_REQUIRE(GAMECLIENT.SaveToFile(saveDir / "Preserved.sav"));
        enter();
        typeFilename("Cancelled");
        press(PadButton::B);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == window);
        BOOST_TEST(!dsk->GetPlayerView(0).GetFocus().IsActive());
        BOOST_TEST(!boost::filesystem::exists(saveDir / "Cancelled.sav"));
        press(PadButton::B);
        BOOST_TEST(dynamic_cast<iwOptionsWindow*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
        BOOST_TEST(!boost::filesystem::exists(saveDir / "Cancelled.sav"));
        expectSave("Preserved", snapshot, gf);
        BOOST_TEST((GAMECLIENT.GetState() == ClientState::Game));
    });
}

BOOST_FIXTURE_TEST_CASE(EscapeAndRightClickCloseWithoutSavingAndAllowControllerReentry, SavePadFixture)
{
    run([this] {
        enter();
        for(const bool keyboard : {true, false})
        {
            typeFilename("Cancelled");
            if(keyboard)
                WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Escape));
            else
                WINDOWMANAGER.Msg_RightDown(MouseCoords(window->GetPos() + DrawPoint(10, 10)));
            frame();
            BOOST_TEST_REQUIRE(dynamic_cast<iwOptionsWindow*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
            BOOST_TEST(boost::filesystem::is_empty(saveDir));
            BOOST_TEST(!dsk->GetPlayerView(0).GetFocus().IsActive());
            openSaveFromGameMenu();
            BOOST_TEST(edit().GetText().empty());
        }
    });
}

BOOST_FIXTURE_TEST_CASE(AutosaveBrowsingCancelsOrCommitsWithoutSavingTheFilename, SavePadFixture)
{
    run([this] {
        enter();
        BOOST_TEST(SETTINGS.interface.autosaveInterval == 0u);
        typeFilename("NotSaved");
        focusUntil(&autosave());
        BOOST_TEST((autosave().GetSelection() == 0u));
        press(PadButton::A);
        press(PadButton::DpadDown);
        BOOST_TEST(autosave().IsListOpen());
        BOOST_TEST((autosave().GetSelection() == 1u));
        BOOST_TEST(SETTINGS.interface.autosaveInterval == 0u);
        press(PadButton::B);
        BOOST_TEST(!autosave().IsListOpen());
        BOOST_TEST((autosave().GetSelection() == 0u));
        BOOST_TEST(focused() == &autosave());
        press(PadButton::A);
        press(PadButton::DpadDown);
        press(PadButton::A);
        BOOST_TEST(!autosave().IsListOpen());
        BOOST_TEST(SETTINGS.interface.autosaveInterval == duration_to_gfs(std::chrono::minutes(1)));
        BOOST_TEST(edit().GetText() == "NotSaved");
        BOOST_TEST(boost::filesystem::is_empty(saveDir));
    });
}

BOOST_AUTO_TEST_CASE(ExceptionalCleanupDestroysTheDesktopBeforeTheBackend)
{
    const auto autosaveBefore = SETTINGS.interface.autosaveInterval;
    const auto debugBefore = SETTINGS.global.debugMode;
    {
        SavePadFixture fixture;
        BOOST_CHECK_THROW(fixture.run([&fixture] {
            fixture.enter();
            throwCleanupProbe();
        }),
                          std::runtime_error);
        BOOST_TEST(dynamic_cast<dskGameInterface*>(WINDOWMANAGER.GetCurrentDesktop()) == nullptr);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(SETTINGS.global.debugMode == debugBefore);
    }
    BOOST_TEST(SETTINGS.interface.autosaveInterval == autosaveBefore);
    BOOST_TEST(SETTINGS.global.debugMode == debugBefore);
}

BOOST_AUTO_TEST_SUITE_END()
