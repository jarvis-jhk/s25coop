// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AddonPadNavigation.h"
#include "GlobalGameSettings.h"
#include "Loader.h"
#include "addons/Addon.h"
#include "addons/AddonCategory.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlCheck.h"
#include "controls/ctrlComboBox.h"
#include "controls/ctrlEdit.h"
#include "controls/ctrlGroup.h"
#include "controls/ctrlOptionGroup.h"
#include "controls/ctrlScrollBar.h"
#include "controls/ctrlTable.h"
#include "controls/ctrlText.h"
#include "desktops/dskMainMenu.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "files.h"
#include "ingameWindows/iwAddonPresets.h"
#include "ingameWindows/iwAddons.h"
#include "libsiedler2/Archiv.h"
#include "libsiedler2/ArchivItem_Ini.h"
#include "libsiedler2/libsiedler2.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {
constexpr PadDeviceId pad = 137;
constexpr unsigned tabsId = 6;
constexpr unsigned scrollId = 7;
constexpr unsigned changedId = 1000;
constexpr unsigned developerId = 1001;
constexpr unsigned emptyId = 1002;

[[noreturn]] void throwBrowserCleanupProbe()
{
    throw std::runtime_error("category browser cleanup probe");
}

class BrowserDesktop : public dskMainMenu
{
public:
    unsigned GetNumPadSlots() const override { return 2; }
};

struct CategoryBrowserFixture : rttr::test::MenuPadFixture
{
    rttr::test::TmpFolder userData;
    rttr::test::ConfigOverride userDataOverride{"USERDATA", userData};
    GlobalGameSettings model;
    iwAddons* window = nullptr;

    template<class F>
    void run(F&& test)
    {
        try
        {
            std::forward<F>(test)();
        } catch(...)
        {
            WINDOWMANAGER.CleanUp();
            throw;
        }
        WINDOWMANAGER.CleanUp();
    }

    void open(AddonChangeAllowed policy = AddonChangeAllowed::All)
    {
        LOADER.LoadDummyLanguageFiles();
        WINDOWMANAGER.Switch(std::make_unique<BrowserDesktop>());
        frame();
        auto owned = std::make_unique<iwAddons>(model, desktop(), policy);
        window = owned.get();
        WINDOWMANAGER.Show(std::move(owned));
        frame();
        pickUp(pad);
    }

    void focus(const Window* target) { rttr::test::FocusAddonControl(*this, pad, target); }

    void click(Window* target, const DrawPoint& offset = DrawPoint(5, 5))
    {
        const auto point = target->GetDrawRect().getOrigin() + offset;
        WINDOWMANAGER.Msg_LeftDown(MouseCoords(point));
        WINDOWMANAGER.Msg_LeftUp(MouseCoords(point));
        frame();
    }

    void toggle(unsigned id)
    {
        focus(window->GetCtrl<ctrlCheck>(id));
        press(pad, PadButton::A);
    }

    ctrlGroup& group(AddonId id) const
    {
        for(unsigned i = 0; i < model.getNumAddons(); ++i)
        {
            if(model.getAddon(i)->getId() == id)
                return *window->GetCtrl<ctrlGroup>(8 + i);
        }
        throw std::runtime_error("Expected registered addon");
    }

    void category(AddonCategory value)
    {
        auto* tabs = window->GetCtrl<ctrlOptionGroup>(tabsId);
        for(unsigned i = 0; i < 7 && tabs->GetSelection() != static_cast<unsigned>(value); ++i)
            press(pad, PadButton::RightShoulder);
        BOOST_TEST_REQUIRE(tabs->GetSelection() == static_cast<unsigned>(value));
    }

    void selectFirstPreset(iwAddonPresetsBase* presets)
    {
        auto* table = presets->GetCtrl<ctrlTable>(iwAddonPresetsBase::ID_tblPresets);
        BOOST_TEST_REQUIRE(table->GetNumRows() > 0u);
        click(table, DrawPoint(5, table->GetCtrl<ctrlButton>(1)->GetSize().y + 3));
    }

    void saveWithMouse(const std::string& filename)
    {
        auto* save = dynamic_cast<iwSaveAddonPreset*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(save != nullptr);
        click(save->GetCtrl<ctrlEdit>(iwAddonPresetsBase::ID_edtName));
        for(const char c : filename)
            WINDOWMANAGER.Msg_KeyDown(KeyEvent{static_cast<char32_t>(c)});
        click(save->GetCtrl<ctrlButton>(iwAddonPresetsBase::ID_btAction));
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
    }

    void persistPreview(bool accept, bool save)
    {
        model.setSelection(AddonId::NUM_SCOUTS_EXPLORATION, 1);
        open();
        category(AddonCategory::Economy);
        reveal(AddonId::NUM_SCOUTS_EXPLORATION);
        auto* combo = group(AddonId::NUM_SCOUTS_EXPLORATION).GetCtrl<ctrlComboBox>(2);
        focus(combo);
        press(pad, PadButton::A);
        press(pad, PadButton::DpadDown);
        BOOST_TEST_REQUIRE((combo->GetSelection() == 2u));
        if(accept)
            press(pad, PadButton::A);
        const PadDeviceId other = 138;
        pickUp(other);
        BOOST_TEST_REQUIRE(focused(1) == window->GetCtrl<ctrlButton>(1));
        if(save)
        {
            press(other, PadButton::DpadUp);
            BOOST_TEST_REQUIRE(focused(1) == window->GetCtrl<ctrlButton>(4));
            press(other, PadButton::A);
            saveWithMouse("Preview");
            libsiedler2::Archiv stored;
            BOOST_TEST_REQUIRE(
              libsiedler2::Load(RTTRCONFIG.ExpandPath(s25::folders::addonPresets) / "Preview.ini", stored) == 0);
            const auto* values = dynamic_cast<const libsiedler2::ArchivItem_Ini*>(stored.find("addons"));
            BOOST_TEST_REQUIRE(values != nullptr);
            BOOST_TEST(values->getIntValue(std::to_string(static_cast<unsigned>(AddonId::NUM_SCOUTS_EXPLORATION)))
                       == (accept ? 2 : 1));
            BOOST_TEST(model.getSelection(AddonId::NUM_SCOUTS_EXPLORATION) == 1u);
        } else
        {
            press(other, PadButton::A);
            BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
            BOOST_TEST(model.getSelection(AddonId::NUM_SCOUTS_EXPLORATION) == (accept ? 2u : 1u));
        }
    }

    void reveal(AddonId id)
    {
        auto& row = group(id);
        auto* scroll = window->GetCtrl<ctrlScrollBar>(scrollId);
        for(unsigned i = 0; i < model.getNumAddons() && !row.IsVisible(); ++i)
        {
            focus(scroll);
            press(pad, PadButton::DpadDown);
        }
        BOOST_TEST_REQUIRE(row.IsVisible());
    }
};
} // namespace

BOOST_AUTO_TEST_SUITE(AddonCategoryBrowserTests)

BOOST_FIXTURE_TEST_CASE(ShouldersCycleRealCategoriesAndDeveloperRequiresOptIn, CategoryBrowserFixture)
{
    run([&] {
        open();
        auto* tabs = window->GetCtrl<ctrlOptionGroup>(tabsId);
        auto* developer = tabs->GetCtrl<ctrlButton>(static_cast<unsigned>(AddonCategory::Developer));
        BOOST_TEST(!developer->IsVisible());
        BOOST_TEST(!group(AddonId::AI_DEBUG_WINDOW).IsVisible());
        for(unsigned expected = 1; expected <= 6; ++expected)
        {
            press(pad, PadButton::RightShoulder);
            BOOST_TEST(tabs->GetSelection() == expected % 6);
            BOOST_TEST(focused(0) == tabs->GetCtrl<ctrlButton>(expected % 6));
            BOOST_TEST(window->GetCtrl<ctrlScrollBar>(scrollId)->GetScrollPos() == 0u);
        }
        press(pad, PadButton::LeftShoulder);
        BOOST_TEST(tabs->GetSelection() == static_cast<unsigned>(AddonCategory::Easier));
        toggle(developerId);
        BOOST_TEST(developer->IsVisible());
        press(pad, PadButton::RightShoulder);
        BOOST_TEST(tabs->GetSelection() == static_cast<unsigned>(AddonCategory::Developer));
        BOOST_TEST(group(AddonId::AI_DEBUG_WINDOW).IsVisible());
        click(window->GetCtrl<ctrlCheck>(developerId));
        BOOST_TEST(!developer->IsVisible());
        BOOST_TEST(tabs->GetSelection() == static_cast<unsigned>(AddonCategory::All));
        BOOST_TEST(!group(AddonId::AI_DEBUG_WINDOW).IsVisible());
        BOOST_TEST(model.getSelection(AddonId::AI_DEBUG_WINDOW) == 0u);
    });
}

BOOST_FIXTURE_TEST_CASE(ChangedOnlyUsesStagedEditsAndDefaultsCanEmptyTheLastPage, CategoryBrowserFixture)
{
    run([&] {
        model.setSelection(AddonId::PEACEFULMODE, 1);
        model.setSelection(AddonId::EXHAUSTIBLE_WATER, 2);
        open();
        toggle(changedId);
        BOOST_TEST(group(AddonId::PEACEFULMODE).IsVisible());
        BOOST_TEST(group(AddonId::EXHAUSTIBLE_WATER).IsVisible());
        BOOST_TEST(!group(AddonId::WINE).IsVisible());
        BOOST_TEST(group(AddonId::PEACEFULMODE).GetCtrl<ctrlText>(4)->GetText() == "Easier");
        BOOST_TEST(group(AddonId::EXHAUSTIBLE_WATER).GetCtrl<ctrlText>(4)->GetText() == "Harder");
        auto* peaceful = group(AddonId::PEACEFULMODE).GetCtrl<ctrlCheck>(2);
        focus(peaceful);
        press(pad, PadButton::A);
        BOOST_TEST(!group(AddonId::PEACEFULMODE).IsVisible());
        BOOST_TEST(model.getSelection(AddonId::PEACEFULMODE) == 1u);
        category(AddonCategory::Easier);
        BOOST_TEST(window->GetCtrl<ctrlText>(emptyId)->IsVisible());
        category(AddonCategory::All);
        click(window->GetCtrl<ctrlButton>(3));
        BOOST_TEST(window->GetCtrl<ctrlText>(emptyId)->IsVisible());
        BOOST_TEST(!window->GetCtrl<ctrlScrollBar>(scrollId)->IsVisible());
        toggle(changedId);
        reveal(AddonId::PEACEFULMODE);
        BOOST_TEST(!peaceful->isChecked());
        BOOST_TEST(group(AddonId::PEACEFULMODE).GetCtrl<ctrlText>(4)->GetText().empty());
        focus(window->GetCtrl<ctrlScrollBar>(scrollId));
        pressN(pad, PadButton::DpadDown, 60);
        BOOST_TEST(window->GetCtrl<ctrlScrollBar>(scrollId)->GetScrollPos() > 0u);
        toggle(changedId);
        BOOST_TEST(window->GetCtrl<ctrlScrollBar>(scrollId)->GetScrollPos() == 0u);
        BOOST_TEST(window->GetCtrl<ctrlText>(emptyId)->IsVisible());
    });
}

BOOST_FIXTURE_TEST_CASE(OpenDropdownKeepsItsPreviewUntilAOrBAndFiltersSeeOnlyAcceptedValues, CategoryBrowserFixture)
{
    run([&] {
        model.setSelection(AddonId::NUM_SCOUTS_EXPLORATION, 1);
        open();
        category(AddonCategory::Economy);
        toggle(changedId);
        auto* combo = group(AddonId::NUM_SCOUTS_EXPLORATION).GetCtrl<ctrlComboBox>(2);
        focus(combo);
        press(pad, PadButton::A);
        press(pad, PadButton::DpadDown);
        BOOST_TEST_REQUIRE((combo->GetSelection() == 2u));
        press(pad, PadButton::LeftShoulder);
        press(pad, PadButton::RightShoulder);
        BOOST_TEST(combo->IsListOpen());
        BOOST_TEST(focused(0) == combo);
        BOOST_TEST(window->GetCtrl<ctrlOptionGroup>(tabsId)->GetSelection()
                   == static_cast<unsigned>(AddonCategory::Economy));
        BOOST_TEST(group(AddonId::NUM_SCOUTS_EXPLORATION).GetCtrl<ctrlText>(4)->GetText() == "Easier");
        WINDOWMANAGER.Msg_WheelDown(MouseCoords(window->GetDrawPos() + DrawPoint(15, 150)));
        frame();
        press(pad, PadButton::B);
        BOOST_TEST(!combo->IsListOpen());
        BOOST_TEST((combo->GetSelection() == 1u));
        BOOST_TEST(group(AddonId::NUM_SCOUTS_EXPLORATION).IsVisible());
        press(pad, PadButton::A);
        press(pad, PadButton::DpadDown);
        press(pad, PadButton::A);
        BOOST_TEST(!combo->IsListOpen());
        BOOST_TEST(!group(AddonId::NUM_SCOUTS_EXPLORATION).IsVisible());
        BOOST_TEST(window->GetCtrl<ctrlText>(emptyId)->IsVisible());
        BOOST_TEST(model.getSelection(AddonId::NUM_SCOUTS_EXPLORATION) == 1u);
        click(window->GetCtrl<ctrlButton>(2));
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(model.getSelection(AddonId::NUM_SCOUTS_EXPLORATION) == 1u);
    });
}

BOOST_FIXTURE_TEST_CASE(MouseWheelScrollsClosedRowsButPreservesAnOpenPreview, CategoryBrowserFixture)
{
    run([&] {
        model.setSelection(AddonId::NUM_SCOUTS_EXPLORATION, 1);
        open();
        category(AddonCategory::All);
        auto* scroll = window->GetCtrl<ctrlScrollBar>(scrollId);
        const MouseCoords outside(window->GetDrawPos() + DrawPoint(15, 150));
        BOOST_TEST_REQUIRE(scroll->GetScrollPos() == 0u);
        WINDOWMANAGER.Msg_WheelDown(outside);
        frame();
        BOOST_TEST(scroll->GetScrollPos() == 2u);
        WINDOWMANAGER.Msg_WheelUp(outside);
        frame();
        BOOST_TEST(scroll->GetScrollPos() == 0u);
        reveal(AddonId::NUM_SCOUTS_EXPLORATION);
        auto* combo = group(AddonId::NUM_SCOUTS_EXPLORATION).GetCtrl<ctrlComboBox>(2);
        focus(combo);
        press(pad, PadButton::A);
        press(pad, PadButton::DpadDown);
        BOOST_TEST_REQUIRE(combo->IsListOpen());
        BOOST_TEST_REQUIRE((combo->GetSelection() == 2u));
        const auto before = scroll->GetScrollPos();
        WINDOWMANAGER.Msg_WheelUp(outside);
        frame();
        BOOST_TEST(scroll->GetScrollPos() == before);
        BOOST_TEST(combo->IsListOpen());
        BOOST_TEST((combo->GetSelection() == 2u));
        WINDOWMANAGER.Msg_WheelDown(outside);
        frame();
        BOOST_TEST(scroll->GetScrollPos() == before);
        BOOST_TEST(combo->IsListOpen());
        BOOST_TEST((combo->GetSelection() == 2u));
        BOOST_TEST(group(AddonId::NUM_SCOUTS_EXPLORATION).GetCtrl<ctrlText>(4)->GetText() == "Easier");
        press(pad, PadButton::B);
        BOOST_TEST(!combo->IsListOpen());
        BOOST_TEST((combo->GetSelection() == 1u));
        click(window->GetCtrl<ctrlButton>(1));
        BOOST_TEST(model.getSelection(AddonId::NUM_SCOUTS_EXPLORATION) == 1u);
    });
}

BOOST_FIXTURE_TEST_CASE(MouseCategoriesAndReadonlyChangedSettingsRemainBrowsable, CategoryBrowserFixture)
{
    run([&] {
        model.setSelection(AddonId::WINE, 1);
        open(AddonChangeAllowed::None);
        auto* tabs = window->GetCtrl<ctrlOptionGroup>(tabsId);
        click(tabs->GetCtrl<ctrlButton>(static_cast<unsigned>(AddonCategory::Content)));
        BOOST_TEST(group(AddonId::WINE).IsVisible());
        auto* wine = group(AddonId::WINE).GetCtrl<ctrlCheck>(2);
        BOOST_TEST(wine->isReadOnly());
        BOOST_TEST(wine->isChecked());
        toggle(changedId);
        BOOST_TEST(group(AddonId::WINE).IsVisible());
        BOOST_TEST(!group(AddonId::LEATHER).IsVisible());
        click(wine);
        BOOST_TEST(wine->isChecked());
        BOOST_CHECK_THROW(rttr::test::FocusAddonControl(*this, pad, wine), std::runtime_error);
        BOOST_CHECK_THROW(group(static_cast<AddonId>(0xFFFFFFFF)), std::runtime_error);
        BOOST_TEST(window->GetCtrl<ctrlButton>(1) == nullptr);
        click(window->GetCtrl<ctrlButton>(2));
        BOOST_TEST(model.getSelection(AddonId::WINE) == 1u);
    });
}

BOOST_FIXTURE_TEST_CASE(PresetLoadRebuildsFilteredRowsAndSaveRetainsHiddenValues, CategoryBrowserFixture)
{
    run([&] {
        const auto folder = RTTRCONFIG.ExpandPath(s25::folders::addonPresets);
        boost::filesystem::create_directories(folder);
        libsiedler2::Archiv seed;
        auto ini = std::make_unique<libsiedler2::ArchivItem_Ini>("addons");
        ini->setValue(std::to_string(static_cast<unsigned>(AddonId::WINE)), "1");
        ini->setValue(std::to_string(static_cast<unsigned>(AddonId::PEACEFULMODE)), "1");
        ini->setValue(std::to_string(static_cast<unsigned>(AddonId::AI_DEBUG_WINDOW)), "1");
        ini->setValue(std::to_string(static_cast<unsigned>(AddonId::SHIP_SPEED)), "999");
        seed.push(std::move(ini));
        BOOST_TEST_REQUIRE(libsiedler2::Write(folder / "Fixture.ini", seed) == 0);
        open();
        category(AddonCategory::Content);
        toggle(changedId);
        BOOST_TEST(window->GetCtrl<ctrlText>(emptyId)->IsVisible());
        click(window->GetCtrl<ctrlButton>(5));
        auto* load = dynamic_cast<iwLoadAddonPreset*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(load != nullptr);
        selectFirstPreset(load);
        click(load->GetCtrl<ctrlButton>(iwAddonPresetsBase::ID_btAction));
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        BOOST_TEST(group(AddonId::WINE).IsVisible());
        BOOST_TEST(group(AddonId::WINE).GetCtrl<ctrlCheck>(2)->isChecked());
        BOOST_TEST(!group(AddonId::PEACEFULMODE).IsVisible());
        BOOST_TEST(!group(AddonId::AI_DEBUG_WINDOW).IsVisible());
        BOOST_TEST(model.getSelection(AddonId::WINE) == 0u);
        click(window->GetCtrl<ctrlButton>(4));
        saveWithMouse("Saved");
        libsiedler2::Archiv stored;
        BOOST_TEST_REQUIRE(libsiedler2::Load(folder / "Saved.ini", stored) == 0);
        const auto* values = dynamic_cast<const libsiedler2::ArchivItem_Ini*>(stored.find("addons"));
        BOOST_TEST_REQUIRE(values != nullptr);
        BOOST_TEST(values->getIntValue(std::to_string(static_cast<unsigned>(AddonId::WINE))) == 1);
        BOOST_TEST(values->getIntValue(std::to_string(static_cast<unsigned>(AddonId::PEACEFULMODE))) == 1);
        BOOST_TEST(values->getIntValue(std::to_string(static_cast<unsigned>(AddonId::AI_DEBUG_WINDOW))) == 1);
        BOOST_TEST(values->getIntValue(std::to_string(static_cast<unsigned>(AddonId::SHIP_SPEED))) == 2);
        click(window->GetCtrl<ctrlButton>(2));
    });
}

BOOST_FIXTURE_TEST_CASE(DefaultsFromAnotherControllerResolveTheOldPreviewBeforeReplacingIt, CategoryBrowserFixture)
{
    run([&] {
        model.setSelection(AddonId::NUM_SCOUTS_EXPLORATION, 1);
        open();
        category(AddonCategory::Economy);
        reveal(AddonId::NUM_SCOUTS_EXPLORATION);
        auto* combo = group(AddonId::NUM_SCOUTS_EXPLORATION).GetCtrl<ctrlComboBox>(2);
        focus(combo);
        press(pad, PadButton::A);
        press(pad, PadButton::DpadDown);
        BOOST_TEST_REQUIRE((combo->GetSelection() == 2u));
        const PadDeviceId other = 138;
        pickUp(other);
        press(other, PadButton::DpadUp);
        pressN(other, PadButton::DpadRight, 2);
        BOOST_TEST_REQUIRE(focused(1) == window->GetCtrl<ctrlButton>(3));
        press(other, PadButton::A);
        BOOST_TEST(!combo->IsListOpen());
        BOOST_TEST((combo->GetSelection() == 2u));
        press(pad, PadButton::B);
        BOOST_TEST((combo->GetSelection() == 2u));
        BOOST_TEST(window->GetCtrl<ctrlText>(emptyId)->IsVisible() == false);
        disconnect(other);
        frame();
        pickUp(other);
        BOOST_TEST_REQUIRE(focused(1) == window->GetCtrl<ctrlButton>(1));
        press(other, PadButton::A);
        BOOST_TEST(model.getSelection(AddonId::NUM_SCOUTS_EXPLORATION) == 2u);
    });
}

BOOST_FIXTURE_TEST_CASE(FilterFromAnotherControllerCancelsPreviewAndUsesTheAcceptedValue, CategoryBrowserFixture)
{
    run([&] {
        model.setSelection(AddonId::NUM_SCOUTS_EXPLORATION, 1);
        open();
        category(AddonCategory::Economy);
        reveal(AddonId::NUM_SCOUTS_EXPLORATION);
        auto* combo = group(AddonId::NUM_SCOUTS_EXPLORATION).GetCtrl<ctrlComboBox>(2);
        focus(combo);
        press(pad, PadButton::A);
        press(pad, PadButton::DpadDown);
        BOOST_TEST_REQUIRE((combo->GetSelection() == 2u));
        const PadDeviceId other = 138;
        pickUp(other);
        auto* filter = window->GetCtrl<ctrlCheck>(changedId);
        rttr::test::FocusAddonControl(*this, other, filter, 1);
        press(other, PadButton::A);
        BOOST_TEST(filter->isChecked());
        BOOST_TEST(!combo->IsListOpen());
        BOOST_TEST((combo->GetSelection() == 1u));
        BOOST_TEST(group(AddonId::NUM_SCOUTS_EXPLORATION).IsVisible());
        BOOST_TEST(group(AddonId::NUM_SCOUTS_EXPLORATION).GetCtrl<ctrlText>(4)->GetText() == "Easier");
        press(pad, PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        BOOST_TEST((combo->GetSelection() == 1u));
    });
}

BOOST_FIXTURE_TEST_CASE(LoadingAPresetDuringPreviewLeavesItsReplacementAccepted, CategoryBrowserFixture)
{
    run([&] {
        const auto folder = RTTRCONFIG.ExpandPath(s25::folders::addonPresets);
        boost::filesystem::create_directories(folder);
        libsiedler2::Archiv seed;
        auto ini = std::make_unique<libsiedler2::ArchivItem_Ini>("addons");
        ini->setValue(std::to_string(static_cast<unsigned>(AddonId::NUM_SCOUTS_EXPLORATION)), "2");
        seed.push(std::move(ini));
        BOOST_TEST_REQUIRE(libsiedler2::Write(folder / "Preview.ini", seed) == 0);
        model.setSelection(AddonId::NUM_SCOUTS_EXPLORATION, 1);
        open();
        category(AddonCategory::Economy);
        reveal(AddonId::NUM_SCOUTS_EXPLORATION);
        auto* combo = group(AddonId::NUM_SCOUTS_EXPLORATION).GetCtrl<ctrlComboBox>(2);
        focus(combo);
        press(pad, PadButton::A);
        press(pad, PadButton::DpadDown);
        const PadDeviceId other = 138;
        pickUp(other);
        press(other, PadButton::DpadUp);
        press(other, PadButton::DpadRight);
        BOOST_TEST_REQUIRE(focused(1) == window->GetCtrl<ctrlButton>(5));
        press(other, PadButton::A);
        auto* load = dynamic_cast<iwLoadAddonPreset*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(load != nullptr);
        selectFirstPreset(load);
        click(load->GetCtrl<ctrlButton>(iwAddonPresetsBase::ID_btAction));
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        BOOST_TEST(!combo->IsListOpen());
        BOOST_TEST((combo->GetSelection() == 2u));
        focus(combo);
        press(pad, PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        BOOST_TEST((combo->GetSelection() == 2u));
        click(window->GetCtrl<ctrlButton>(1));
        BOOST_TEST(model.getSelection(AddonId::NUM_SCOUTS_EXPLORATION) == 2u);
    });
}

BOOST_FIXTURE_TEST_CASE(PresetLoadCancelsAMouseOpenedListWithoutControllerFocus, CategoryBrowserFixture)
{
    run([&] {
        const auto folder = RTTRCONFIG.ExpandPath(s25::folders::addonPresets);
        boost::filesystem::create_directories(folder);
        libsiedler2::Archiv seed;
        auto ini = std::make_unique<libsiedler2::ArchivItem_Ini>("addons");
        ini->setValue(std::to_string(static_cast<unsigned>(AddonId::NUM_SCOUTS_EXPLORATION)), "2");
        seed.push(std::move(ini));
        BOOST_TEST_REQUIRE(libsiedler2::Write(folder / "MousePreview.ini", seed) == 0);
        model.setSelection(AddonId::NUM_SCOUTS_EXPLORATION, 1);
        open();
        category(AddonCategory::Economy);
        reveal(AddonId::NUM_SCOUTS_EXPLORATION);
        auto* combo = group(AddonId::NUM_SCOUTS_EXPLORATION).GetCtrl<ctrlComboBox>(2);
        focus(window->GetCtrl<ctrlButton>(5));
        click(combo);
        BOOST_TEST_REQUIRE(focused(0) == window->GetCtrl<ctrlButton>(5));
        BOOST_TEST_REQUIRE(combo->IsListOpen());
        press(pad, PadButton::A);
        auto* load = dynamic_cast<iwLoadAddonPreset*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(load != nullptr);
        selectFirstPreset(load);
        // Root reconciliation must leave this mouse-owned list open until the real load callback.
        BOOST_TEST_REQUIRE(combo->IsListOpen());
        click(load->GetCtrl<ctrlButton>(iwAddonPresetsBase::ID_btAction));
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        BOOST_TEST(!combo->IsListOpen());
        BOOST_TEST((combo->GetSelection() == 2u));
        focus(combo);
        press(pad, PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == window);
        BOOST_TEST((combo->GetSelection() == 2u));
        click(window->GetCtrl<ctrlButton>(1));
        BOOST_TEST(model.getSelection(AddonId::NUM_SCOUTS_EXPLORATION) == 2u);
    });
}

BOOST_FIXTURE_TEST_CASE(ApplyFromAnotherControllerRejectsUnacceptedPreview, CategoryBrowserFixture)
{
    run([&] { persistPreview(false, false); });
}

BOOST_FIXTURE_TEST_CASE(ApplyFromAnotherControllerKeepsAcceptedSelection, CategoryBrowserFixture)
{
    run([&] { persistPreview(true, false); });
}

BOOST_FIXTURE_TEST_CASE(SaveFromAnotherControllerRejectsUnacceptedPreview, CategoryBrowserFixture)
{
    run([&] { persistPreview(false, true); });
}

BOOST_FIXTURE_TEST_CASE(SaveFromAnotherControllerKeepsAcceptedSelection, CategoryBrowserFixture)
{
    run([&] { persistPreview(true, true); });
}

BOOST_AUTO_TEST_CASE(ExceptionCleanupRunsAfterAnOpenBrowser)
{
    CategoryBrowserFixture fixture;
    bool reached = false;
    BOOST_CHECK_THROW(fixture.run([&] {
        fixture.open();
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == fixture.window);
        reached = true;
        throwBrowserCleanupProbe();
    }),
                      std::runtime_error);
    BOOST_TEST(reached);
    BOOST_TEST(fixture.desktop() == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
