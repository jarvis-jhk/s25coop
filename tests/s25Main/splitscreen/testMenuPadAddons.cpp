// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GameLobby.h"
#include "GlobalGameSettings.h"
#include "ILobbyClient.hpp"
#include "Loader.h"
#include "LocalGameFixture.h"
#include "MenuPadFixture.h"
#include "addons/Addon.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlCheck.h"
#include "controls/ctrlComboBox.h"
#include "controls/ctrlGroup.h"
#include "controls/ctrlOptionGroup.h"
#include "controls/ctrlScrollBar.h"
#include "desktops/dskGameLobby.h"
#include "desktops/dskMainMenu.h"
#include "desktops/dskOptions.h"
#include "driver/MouseCoords.h"
#include "files.h"
#include "ingameWindows/iwAddons.h"
#include "libsiedler2/Archiv.h"
#include "libsiedler2/ArchivItem_Ini.h"
#include "libsiedler2/libsiedler2.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
constexpr PadDeviceId pad = 102;
constexpr unsigned apply = 1;
constexpr unsigned abortChanges = 2;
constexpr unsigned defaults = 3;
constexpr unsigned categories = 6;
constexpr unsigned scrollbar = 7;
constexpr unsigned addonGroups = 8;

[[noreturn]] void throwCleanupProbe()
{
    throw std::runtime_error("addon cleanup probe");
}

struct AddonsPadFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder userData;
    rttr::test::ConfigOverride userDataOverride{"USERDATA", userData};
    decltype(SETTINGS.addons.configuration)& configuration = SETTINGS.addons.configuration;
    decltype(SETTINGS.addons.configuration) savedConfiguration = configuration;
    GlobalGameSettings model;
    iwAddons* window = nullptr;

    AddonsPadFixture()
    {
        LOADER.LoadDummyLanguageFiles();
        configuration.clear();
        model.SaveSettings();
    }
    ~AddonsPadFixture() override { configuration.swap(savedConfiguration); }

    template<class F>
    static void run(F&& test)
    {
        try
        {
            std::forward<F>(test)();
        } catch(...)
        {
            WINDOWMANAGER.CleanUp();
            throw;
        }
        // dskOptions saves its own ggs on destruction. Destroy it before restoring SETTINGS.
        WINDOWMANAGER.CleanUp();
    }

    void focusUntil(const Window* target)
    {
        // Shoulder navigation deliberately has no wrap. First search backwards, then forwards.
        for(unsigned i = 0; i < 100 && focused(0) != target; ++i)
        {
            const auto* before = focused(0);
            press(pad, PadButton::LeftShoulder);
            if(focused(0) == before)
                break;
        }
        for(unsigned i = 0; i < 100 && focused(0) != target; ++i)
            press(pad, PadButton::RightShoulder);
        BOOST_TEST_CONTEXT("target id=" << target->GetID() << " focusable=" << target->CanFocus()
                                        << " visible=" << target->IsVisible()
                                        << " parent=" << (target->GetParent() ? target->GetParent()->GetID() : 0)
                                        << " root=" << padInput().GetFocus(0).GetRoot())
        {
            BOOST_TEST_REQUIRE(focused(0) == target);
        }
    }

    void act(const unsigned id)
    {
        focusUntil(window->GetCtrl<ctrlButton>(id));
        press(pad, PadButton::A);
    }

    void selectCategory(const AddonGroup group)
    {
        auto* tabs = window->GetCtrl<ctrlOptionGroup>(categories);
        focusUntil(tabs->GetCtrl<ctrlButton>(static_cast<unsigned>(group)));
        press(pad, PadButton::A);
        BOOST_TEST(tabs->GetSelection() == static_cast<unsigned>(group));
        BOOST_TEST(window->GetCtrl<ctrlScrollBar>(scrollbar)->GetScrollPos() == 0u);
    }

    ctrlGroup& groupFor(const AddonId id) const
    {
        for(unsigned i = 0; i < model.getNumAddons(); ++i)
        {
            if(model.getAddon(i)->getId() == id)
                return *window->GetCtrl<ctrlGroup>(addonGroups + i);
        }
        throw std::runtime_error("Expected addon not registered"); // LCOV_EXCL_LINE
    }

    void reveal(const AddonId id)
    {
        auto& group = groupFor(id);
        auto* scroll = window->GetCtrl<ctrlScrollBar>(scrollbar);
        for(unsigned i = 0; i < model.getNumAddons() && !group.IsVisible(); ++i)
        {
            focusUntil(scroll);
            press(pad, PadButton::DpadDown);
        }
        BOOST_TEST_REQUIRE(group.IsVisible());
    }

    ctrlCheck& peaceful()
    {
        reveal(AddonId::PEACEFULMODE);
        auto* check = groupFor(AddonId::PEACEFULMODE).GetCtrl<ctrlCheck>(2);
        BOOST_TEST_REQUIRE(check != nullptr);
        return *check;
    }

    void togglePeaceful()
    {
        auto& check = peaceful();
        const bool before = check.isChecked();
        focusUntil(&check);
        press(pad, PadButton::A);
        BOOST_TEST(check.isChecked() != before);
    }

    void expectSkipped(const Window* locked)
    {
        focusUntil(window->GetCtrl<ctrlButton>(abortChanges));
        for(const auto direction : {PadButton::LeftShoulder, PadButton::RightShoulder})
        {
            bool reachedEnd = false;
            for(unsigned i = 0; i < 100; ++i)
            {
                BOOST_TEST(focused(0) != locked);
                const auto* before = focused(0);
                press(pad, direction);
                if(focused(0) == before)
                {
                    reachedEnd = true;
                    break;
                }
            }
            BOOST_TEST_REQUIRE(reachedEnd);
        }
    }

    void enterOptions()
    {
        WINDOWMANAGER.Switch(std::make_unique<dskOptions>());
        frame();
        pickUp(pad);
        const auto buttons = desktop()->GetCtrls<ctrlButton>();
        const auto addonButton = std::find_if(buttons.begin(), buttons.end(), [](const ctrlButton* button) {
            return button->GetPos() == DrawPoint(520, 570);
        });
        BOOST_TEST_REQUIRE((addonButton != buttons.end()));
        focusUntil(*addonButton);
        press(pad, PadButton::A);
        window = dynamic_cast<iwAddons*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(window != nullptr);
        frame();
    }
};

struct LobbyAddonsPadFixture : rttr::test::LocalGameFixture, AddonsPadFixture
{
    void frame() override
    {
        GAMECLIENT.Run();
        GAMESERVER.Run();
        AddonsPadFixture::frame();
    }

    void enterLobby()
    {
        hostAndEnterLobby();
        WINDOWMANAGER.Switch(std::make_unique<dskGameLobby>(ServerType::Local, GAMECLIENT.GetGameLobby(),
                                                            GAMECLIENT.GetPlayerId(), nullptr));
        frame();
        pickUp(pad);
        BOOST_TEST_REQUIRE(desktopAs<dskGameLobby>() != nullptr);
    }

    void openFromLobby()
    {
        const auto buttons = desktop()->GetCtrls<ctrlButton>();
        const auto addonButton = std::find_if(buttons.begin(), buttons.end(), [](const ctrlButton* button) {
            return button->GetPos() == DrawPoint(600, 495);
        });
        BOOST_TEST_REQUIRE((addonButton != buttons.end()));
        focusUntil(*addonButton);
        press(pad, PadButton::A);
        window = dynamic_cast<iwAddons*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(window != nullptr);
        frame();
    }

    void openPolicy(const AddonChangeAllowed policy)
    {
        // Isolate the window's policy contract from campaign Lua policy selection. The parent
        // is still the real, connected lobby: Apply must use its production settings callback.
        auto owned = std::make_unique<iwAddons>(GAMECLIENT.GetGameLobby()->getSettings(), desktop(), policy,
                                                std::vector<AddonId>{AddonId::PEACEFULMODE});
        window = owned.get();
        WINDOWMANAGER.Show(std::move(owned));
        frame();
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(MenuPadAddonsTests)

BOOST_FIXTURE_TEST_CASE(OptionsApplyChangesSettingsAndTheSavedConfig, AddonsPadFixture)
{
    run([&] {
        enterOptions();
        togglePeaceful();
        BOOST_TEST(configuration.at(static_cast<unsigned>(AddonId::PEACEFULMODE)) == 0u);
        act(apply);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        GlobalGameSettings loaded;
        loaded.LoadSettings();
        BOOST_TEST(loaded.getSelection(AddonId::PEACEFULMODE) == 1u);
        press(pad, PadButton::B); // Options' existing save-and-return action writes the file.
        BOOST_TEST_REQUIRE(desktopAs<dskMainMenu>() != nullptr);
        libsiedler2::Archiv persisted;
        BOOST_TEST_REQUIRE(libsiedler2::Load(RTTRCONFIG.ExpandPath(s25::resources::config), persisted) == 0);
        const auto* addons = dynamic_cast<const libsiedler2::ArchivItem_Ini*>(persisted.find("addons"));
        BOOST_TEST_REQUIRE(addons != nullptr);
        BOOST_TEST(addons->getIntValue(std::to_string(static_cast<unsigned>(AddonId::PEACEFULMODE))) == 1);
    });
}

BOOST_FIXTURE_TEST_CASE(AbortAndCustomCloseGesturesPreserveOptions, AddonsPadFixture)
{
    run([&] {
        enterOptions();
        togglePeaceful();
        const auto before = configuration;
        press(pad, PadButton::B);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == window);
        WINDOWMANAGER.Msg_RightUp(MouseCoords(window->GetDrawPos() + DrawPoint(10, 10)));
        frame();
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == window);
        act(abortChanges);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST((configuration == before));
        enterOptions();
        BOOST_TEST(!peaceful().isChecked());
        act(abortChanges);
    });
}

BOOST_FIXTURE_TEST_CASE(CategoryChangesResetScrollAndKeepPendingCheckboxEdits, AddonsPadFixture)
{
    run([&] {
        enterOptions();
        togglePeaceful();
        BOOST_TEST(window->GetCtrl<ctrlScrollBar>(scrollbar)->GetScrollPos() > 0u);
        selectCategory(AddonGroup::Economy);
        BOOST_TEST(!groupFor(AddonId::PEACEFULMODE).IsVisible());
        selectCategory(AddonGroup::Military);
        BOOST_TEST(peaceful().isChecked());
        selectCategory(AddonGroup::All);
        BOOST_TEST(peaceful().isChecked());
        act(abortChanges);
    });
}

BOOST_FIXTURE_TEST_CASE(BDiscardsAddonDropdownBeforeReturningToItsParent, AddonsPadFixture)
{
    run([&] {
        enterOptions();
        selectCategory(AddonGroup::Military);
        reveal(AddonId::LIMIT_CATAPULTS);
        auto* combo = groupFor(AddonId::LIMIT_CATAPULTS).GetCtrl<ctrlComboBox>(2);
        BOOST_TEST_REQUIRE(combo != nullptr);
        focusUntil(combo);
        const auto before = combo->GetSelection();
        press(pad, PadButton::A);
        BOOST_TEST_REQUIRE(combo->IsListOpen());
        press(pad, PadButton::DpadDown);
        BOOST_TEST_REQUIRE((combo->GetSelection() != before));
        press(pad, PadButton::B);
        BOOST_TEST(!combo->IsListOpen());
        BOOST_TEST((combo->GetSelection() == before));
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == window);
        act(abortChanges);
    });
}

BOOST_FIXTURE_TEST_CASE(LobbyApplyUsesTheRealSettingsCallbackAndNetworkRoundtrip, LobbyAddonsPadFixture)
{
    run([&] {
        enterLobby();
        openFromLobby();
        togglePeaceful();
        BOOST_TEST(GAMECLIENT.GetGameLobby()->getSettings().getSelection(AddonId::PEACEFULMODE) == 0u);
        act(apply);
        // Clear only the local copy after Apply queued its message. It must be restored by the
        // real server broadcast, so this assertion cannot pass on the window's local edit alone.
        GAMECLIENT.GetGameLobby()->getSettings().setSelection(AddonId::PEACEFULMODE, 0);
        pumpUntil([] { return GAMECLIENT.GetGameLobby()->getSettings().getSelection(AddonId::PEACEFULMODE) == 1u; },
                  "addon apply to reach the server");
        BOOST_TEST(GAMECLIENT.GetGameLobby()->getSettings().getSelection(AddonId::PEACEFULMODE) == 1u);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        openFromLobby();
        BOOST_TEST(peaceful().isChecked());
        togglePeaceful();
        act(abortChanges);
        BOOST_TEST(GAMECLIENT.GetGameLobby()->getSettings().getSelection(AddonId::PEACEFULMODE) == 1u);
    });
}

BOOST_FIXTURE_TEST_CASE(ReadOnlyPolicyExcludesEditingAndApply, LobbyAddonsPadFixture)
{
    run([&] {
        enterLobby();
        openPolicy(AddonChangeAllowed::None);
        auto& check = peaceful();
        BOOST_TEST(check.isReadOnly());
        BOOST_TEST(!check.CanFocus());
        expectSkipped(&check);
        BOOST_TEST(window->GetCtrl<ctrlButton>(apply) == nullptr);
        BOOST_TEST(window->GetCtrl<ctrlButton>(defaults) == nullptr);
        WINDOWMANAGER.Msg_LeftDown(MouseCoords(check.GetDrawRect().getOrigin() + DrawPoint(5, 5)));
        frame();
        BOOST_TEST(!check.isChecked());
        act(abortChanges);
        BOOST_TEST(GAMECLIENT.GetGameLobby()->getSettings().getSelection(AddonId::PEACEFULMODE) == 0u);
    });
}

BOOST_FIXTURE_TEST_CASE(WhitelistDefaultAndApplyPreserveLockedNonDefaultSelections, LobbyAddonsPadFixture)
{
    run([&] {
        enterLobby();
        GAMECLIENT.GetGameLobby()->getSettings().setSelection(AddonId::LIMIT_CATAPULTS, 2);
        openPolicy(AddonChangeAllowed::WhitelistOnly);
        selectCategory(AddonGroup::Military);
        reveal(AddonId::LIMIT_CATAPULTS);
        auto* locked = groupFor(AddonId::LIMIT_CATAPULTS).GetCtrl<ctrlComboBox>(2);
        BOOST_TEST_REQUIRE(locked != nullptr);
        BOOST_TEST(!locked->CanFocus());
        expectSkipped(locked);
        BOOST_TEST((locked->GetSelection() == 2u));
        togglePeaceful();
        act(defaults);
        BOOST_TEST(!peaceful().isChecked());
        BOOST_TEST((locked->GetSelection() == 2u));
        togglePeaceful();
        act(apply);
        // Clear only the local copy after Apply queued its message. It must be restored by the
        // real server broadcast, so this assertion cannot pass on the window's local edit alone.
        GAMECLIENT.GetGameLobby()->getSettings().setSelection(AddonId::PEACEFULMODE, 0);
        pumpUntil([] { return GAMECLIENT.GetGameLobby()->getSettings().getSelection(AddonId::PEACEFULMODE) == 1u; },
                  "whitelisted addon apply to reach the server");
        BOOST_TEST(GAMECLIENT.GetGameLobby()->getSettings().getSelection(AddonId::LIMIT_CATAPULTS) == 2u);
    });
}

BOOST_AUTO_TEST_CASE(ExceptionDestroysOptionsBeforeRestoringCapturedConfiguration)
{
    const auto original = SETTINGS.addons.configuration;
    {
        AddonsPadFixture fixture;
        fixture.run([&] {
            fixture.enterOptions();
            fixture.togglePeaceful();
            fixture.act(apply);
            BOOST_CHECK_THROW(fixture.run(throwCleanupProbe), std::runtime_error);
            BOOST_TEST(fixture.desktop() == nullptr);
        });
    }
    BOOST_TEST((SETTINGS.addons.configuration == original));
}

BOOST_AUTO_TEST_SUITE_END()
