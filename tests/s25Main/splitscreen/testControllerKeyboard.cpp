// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Loader.h"
#include "MenuPadFixture.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlEdit.h"
#include "controls/ctrlGroup.h"
#include "controls/ctrlTextDeepening.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "ingameWindows/iwControllerKeyboard.h"
#include <boost/test/unit_test.hpp>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {
constexpr PadDeviceId owner = 211;
constexpr PadDeviceId other = 212;

class KeyboardDesktop : public Desktop
{
public:
    KeyboardDesktop(EditType type, unsigned short limit, bool password) : Desktop(nullptr)
    {
        auto* target = AddEdit(1, DrawPoint(20, 30), Extent(200, 24), TextureColor::Green2, NormalFont, limit, password,
                               false, true);
        target->SetType(type);
        target->SetControllerKeyboardEnabled();
        AddEdit(2, DrawPoint(20, 70), Extent(200, 24), TextureColor::Green2, NormalFont);
        AddTextButton(3, DrawPoint(20, 110), Extent(200, 24), TextureColor::Green2, "Back", NormalFont);
    }
    bool WantsPadInput() const override { return true; }
    unsigned GetNumPadSlots() const override { return slots; }
    bool AllowsPadWindowInput(unsigned slot) const override { return !restrictWindows || slot == 1u; }
    unsigned slots = 2;
    bool restrictWindows = false;
    Window* GetPadEntryCtrl(unsigned) override { return GetCtrl<ctrlEdit>(1); }
    bool Msg_PadCommand(unsigned, PadButton button) override
    {
        if(button == PadButton::Start)
        {
            ++starts;
            return true;
        }
        return false;
    }
    unsigned starts = 0;
    void Msg_EditChange(unsigned) override { ++changes; }
    void Msg_EditEnter(unsigned) override { ++enters; }
    unsigned changes = 0;
    unsigned enters = 0;
};

class KeyboardParentWindow : public IngameWindow
{
public:
    KeyboardParentWindow()
        : IngameWindow(CGI_INPUTWINDOW, posCenter, Extent(300, 150), "Target", LOADER.GetImageN("resource", 41))
    {
        auto* edit =
          AddEdit(1, DrawPoint(20, 40), Extent(200, 24), TextureColor::Green2, NormalFont, 0, false, false, true);
        edit->SetControllerKeyboardEnabled();
    }
    void Msg_EditChange(unsigned) override { ++changes; }
    unsigned changes = 0;
};

[[noreturn]] void throwKeyboardCleanupProbe()
{
    throw std::runtime_error("keyboard cleanup probe");
}

struct KeyboardFixture : rttr::test::MenuPadFixture
{
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
    KeyboardDesktop& screen() const { return *desktopAs<KeyboardDesktop>(); }
    ctrlEdit& target() const
    {
        auto* field = screen().GetCtrl<ctrlEdit>(1);
        if(!field)
        {
            auto* group = screen().GetCtrl<ctrlGroup>(4);
            BOOST_TEST_REQUIRE(group != nullptr);
            field = group->GetCtrl<ctrlEdit>(1);
        }
        BOOST_TEST_REQUIRE(field != nullptr);
        return *field;
    }
    void begin(const std::string& text = "seed", EditType type = EditType::Text, unsigned short limit = 0,
               bool password = false)
    {
        LOADER.LoadDummyLanguageFiles();
        WINDOWMANAGER.Switch(std::make_unique<KeyboardDesktop>(type, limit, password));
        frame();
        target().SetText(text);
        screen().changes = 0;
        pickUp(owner);
    }
    iwControllerKeyboard& keyboard() const
    {
        auto* window = dynamic_cast<iwControllerKeyboard*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(window != nullptr);
        return *window;
    }
    ctrlEdit& draft() const { return *keyboard().GetCtrl<ctrlEdit>(0); }
    void open()
    {
        press(owner, PadButton::A);
        frame();
        BOOST_TEST_REQUIRE(focused(router().GetSlot(owner)) == keyboard().GetCtrl<ctrlButton>(10));
    }
    void click(Window* control)
    {
        const auto position = control->GetDrawRect().getOrigin() + DrawPoint(5, 5);
        WINDOWMANAGER.Msg_LeftDown(MouseCoords(position));
        WINDOWMANAGER.Msg_LeftUp(MouseCoords(position));
        frame();
    }
    void key(const KeyEvent& event) { WINDOWMANAGER.Msg_KeyDown(event); }
};
} // namespace

BOOST_AUTO_TEST_SUITE(ControllerKeyboardTests)

BOOST_FIXTURE_TEST_CASE(OptInFocusLeavesTheMouseFieldAndLegacyContractAlone, KeyboardFixture)
{
    run([&] {
        begin();
        auto* mouseField = screen().GetCtrl<ctrlEdit>(2);
        click(mouseField);
        BOOST_TEST_REQUIRE(focused(0) == &target());
        BOOST_TEST(!target().HasFocus());
        BOOST_TEST(!mouseField->CanFocus());
        BOOST_TEST(mouseField->HasFocus());
        key(KeyEvent(U'm'));
        BOOST_TEST(mouseField->GetText() == "m");
        key(KeyEvent(KeyType::Return));
        BOOST_TEST(screen().enters == 1u);
        BOOST_TEST(target().GetText() == "seed");
        press(owner, PadButton::DpadDown);
        press(owner, PadButton::DpadUp);
        BOOST_TEST(!target().HasFocus());
        BOOST_TEST(mouseField->HasFocus());
        BOOST_TEST(screen().changes == 0u);
        target().SetDisabled();
        BOOST_TEST(!target().CanFocus());
        BOOST_TEST(!target().CanActivate());
    });
}

BOOST_FIXTURE_TEST_CASE(OwnerGridShortcutsAndMixedInputCommitOnlyOnce, KeyboardFixture)
{
    run([&] {
        begin();
        open();
        press(owner, PadButton::A);
        press(owner, PadButton::DpadDown);
        press(owner, PadButton::Y);
        press(owner, PadButton::A);
        press(owner, PadButton::X);
        press(owner, PadButton::B);
        key(KeyEvent(U'\u00fc'));
        BOOST_TEST(draft().GetText() == "seed1Q\xc3\xbc");
        press(owner, PadButton::B);
        click(keyboard().GetCtrl<ctrlButton>(40));
        BOOST_TEST(draft().GetText() == "seed1QZ");
        BOOST_TEST(target().GetText() == "seed");
        BOOST_TEST(screen().changes == 0u);
        BOOST_TEST(screen().enters == 0u);
        press(owner, PadButton::Start);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(target().GetText() == "seed1QZ");
        BOOST_TEST(screen().changes == 1u);
        BOOST_TEST(screen().enters == 0u);
    });
}

BOOST_FIXTURE_TEST_CASE(MouseActionsAndEscapeDiscardTheDraft, KeyboardFixture)
{
    run([&] {
        begin("");
        open();
        click(keyboard().GetCtrl<ctrlButton>(10));
        click(keyboard().GetCtrl<ctrlButton>(100));
        click(keyboard().GetCtrl<ctrlButton>(102));
        click(keyboard().GetCtrl<ctrlButton>(30));
        click(keyboard().GetCtrl<ctrlButton>(101));
        BOOST_TEST(draft().GetText() == "A ");
        click(keyboard().GetCtrl<ctrlButton>(104));
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(target().GetText().empty());
        BOOST_TEST(screen().changes == 0u);
        open();
        key(KeyEvent(U'x'));
        key(KeyEvent(KeyType::Escape));
        frame();
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(target().GetText().empty());
        BOOST_TEST(screen().changes == 0u);
    });
}

BOOST_FIXTURE_TEST_CASE(NumberLengthAndPhysicalEnterUseTheOriginalEditPolicy, KeyboardFixture)
{
    run([&] {
        begin("1", EditType::Number, 3);
        open();
        key(KeyEvent(U'a'));
        press(owner, PadButton::X);
        press(owner, PadButton::A);
        press(owner, PadButton::A);
        press(owner, PadButton::A);
        BOOST_TEST(draft().GetText() == "111");
        press(owner, PadButton::B);
        key(KeyEvent(U'.'));
        BOOST_TEST(draft().GetText() == "11");
        key(KeyEvent(KeyType::Return));
        frame();
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(target().GetText() == "11");
        BOOST_TEST(screen().changes == 1u);
        BOOST_TEST(screen().enters == 0u);
    });
}

BOOST_FIXTURE_TEST_CASE(FilenameAndPasswordDraftPreserveRestrictionsAndMasking, KeyboardFixture)
{
    run([&] {
        begin("ab", EditType::Filename, 4, true);
        open();
        key(KeyEvent(U'/'));
        key(KeyEvent(U'\u00fc'));
        press(owner, PadButton::A);
        press(owner, PadButton::A);
        BOOST_TEST(draft().GetText()
                   == "ab\xc3\xbc"
                      "1");
        BOOST_TEST(draft().GetCtrl<ctrlTextDeepening>(0)->GetText() == "****");
        click(keyboard().GetCtrl<ctrlButton>(103));
        BOOST_TEST(target().GetText()
                   == "ab\xc3\xbc"
                      "1");
        BOOST_TEST(target().GetCtrl<ctrlTextDeepening>(0)->GetText() == "****");
        BOOST_TEST(screen().changes == 1u);
    });
}

BOOST_FIXTURE_TEST_CASE(ForeignDevicesCannotNavigateTypeConfirmOrTakeOverAfterDisconnect, KeyboardFixture)
{
    run([&] {
        begin();
        pickUp(other);
        open();
        const auto* initialFocus = focused(1);
        press(other, PadButton::DpadRight);
        press(other, PadButton::A);
        press(other, PadButton::X);
        press(other, PadButton::Y);
        press(other, PadButton::B);
        press(other, PadButton::Start);
        BOOST_TEST(target().GetText() == "seed");
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() != nullptr);
        BOOST_TEST(focused(1) == initialFocus);
        BOOST_TEST(draft().GetText() == "seed");
        press(owner, PadButton::A);
        disconnect(owner);
        tap(other, PadButton::Start);
        frame();
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(target().GetText() == "seed");
        BOOST_TEST(screen().changes == 0u);
    });
}

BOOST_FIXTURE_TEST_CASE(TargetRemovalAndDesktopReplacementNeverApplyAStaleDraft, KeyboardFixture)
{
    run([&] {
        begin();
        open();
        press(owner, PadButton::A);
        screen().DeleteCtrl(1);
        tap(owner, PadButton::Start);
        frame();
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(screen().changes == 0u);
        WINDOWMANAGER.Switch(std::make_unique<KeyboardDesktop>(EditType::Text, 0, false));
        frame();
        open();
        press(owner, PadButton::A);
        WINDOWMANAGER.Switch(std::make_unique<KeyboardDesktop>(EditType::Text, 0, false));
        tap(owner, PadButton::Start);
        frame();
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(target().GetText().empty());
        BOOST_TEST(screen().changes == 0u);
    });
}

BOOST_FIXTURE_TEST_CASE(ExternalReplacementOrDisableCancelsInsteadOfOverwriting, KeyboardFixture)
{
    run([&] {
        begin();
        open();
        press(owner, PadButton::A);
        target().SetText("external");
        click(keyboard().GetCtrl<ctrlButton>(103));
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(target().GetText() == "external");
        BOOST_TEST(screen().changes == 1u);
        open();
        target().SetDisabled();
        press(owner, PadButton::Start);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(target().GetText() == "external");
        BOOST_TEST(screen().changes == 1u);
    });
}

BOOST_FIXTURE_TEST_CASE(DeviceOwnershipSurvivesARealSlotPolicyChange, KeyboardFixture)
{
    run([&] {
        begin();
        disconnect(owner);
        frame();
        pickUp(other);
        pickUp(owner);
        BOOST_TEST_REQUIRE(router().GetSlot(owner) == 1u);
        screen().restrictWindows = true;
        open();
        press(owner, PadButton::A);
        screen().slots = 1;
        disconnect(other);
        frame();
        BOOST_TEST_REQUIRE(router().GetSlot(owner) == 0u);
        BOOST_TEST(!screen().AllowsPadWindowInput(0));
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() != nullptr);
        press(owner, PadButton::DpadRight);
        press(owner, PadButton::A);
        press(owner, PadButton::Start);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(target().GetText() == "seed12");
        BOOST_TEST(screen().changes == 1u);
    });
}

BOOST_FIXTURE_TEST_CASE(ClosingOrReplacingTheOwningWindowDropsItsDraft, KeyboardFixture)
{
    run([&] {
        begin();
        auto first = std::make_unique<KeyboardParentWindow>();
        auto* parent = first.get();
        WINDOWMANAGER.Show(std::move(first));
        frame();
        open();
        press(owner, PadButton::A);
        press(owner, PadButton::Start);
        BOOST_TEST(parent->GetCtrl<ctrlEdit>(1)->GetText() == "1");
        BOOST_TEST(parent->changes == 1u);
        parent->GetCtrl<ctrlEdit>(1)->SetText("");
        parent->changes = 0;
        open();
        press(owner, PadButton::A);
        parent->Close();
        key(KeyEvent(KeyType::Return));
        BOOST_TEST(parent->GetCtrl<ctrlEdit>(1)->GetText().empty());
        BOOST_TEST(parent->changes == 0u);
        tap(owner, PadButton::Start);
        frame();
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(screen().changes == 0u);
        auto second = std::make_unique<KeyboardParentWindow>();
        parent = second.get();
        WINDOWMANAGER.Show(std::move(second));
        frame();
        open();
        press(owner, PadButton::A);
        WINDOWMANAGER.ReplaceWindow(std::make_unique<KeyboardParentWindow>());
        tap(owner, PadButton::Start);
        frame();
        auto* replacement = dynamic_cast<KeyboardParentWindow*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(replacement != nullptr);
        BOOST_TEST(replacement->GetCtrl<ctrlEdit>(1)->GetText().empty());
        BOOST_TEST(replacement->changes == 0u);
    });
}

BOOST_FIXTURE_TEST_CASE(RestoringTheInitialTextDoesNotRevalidateAnOldDraft, KeyboardFixture)
{
    run([&] {
        begin();
        open();
        press(owner, PadButton::A);
        target().SetText("intervening");
        target().SetText("seed");
        press(owner, PadButton::Start);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(target().GetText() == "seed");
        BOOST_TEST(screen().changes == 2u);
    });
}

BOOST_FIXTURE_TEST_CASE(QueuedActivationsDoNotOpenASecondModal, KeyboardFixture)
{
    run([&] {
        begin();
        press(owner, PadButton::Start);
        BOOST_TEST(screen().starts == 1u);
        press(owner, PadButton::B);
        screen().starts = 0;
        pickUp(other);
        tap(owner, PadButton::A);
        tap(other, PadButton::A);
        tap(other, PadButton::Start);
        frame();
        frame();
        BOOST_TEST(screen().starts == 0u);
        press(owner, PadButton::A);
        BOOST_TEST(draft().GetText() == "seed1");
        click(keyboard().GetCtrl<ctrlButton>(104));
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(target().GetText() == "seed");
        BOOST_TEST(screen().changes == 0u);
    });
}

BOOST_FIXTURE_TEST_CASE(NestedFrontendFieldUsesTheSameRootAndLifetimeContract, KeyboardFixture)
{
    run([&] {
        begin();
        screen().DeleteCtrl(1);
        auto* group = screen().AddGroup(4);
        group->SetPos(DrawPoint(20, 30));
        group->Resize(Extent(240, 100));
        auto* field = group->AddEdit(1, DrawPoint(10, 10), Extent(200, 24), TextureColor::Green2, NormalFont);
        field->SetControllerKeyboardEnabled();
        BOOST_TEST(&target() == field);
        disconnect(owner);
        frame();
        pickUp(owner);
        // Default physical entry is Back; the D-pad reaches the nested field without keyboard focus.
        press(owner, PadButton::DpadUp);
        BOOST_TEST_REQUIRE(focused(0) == field);
        BOOST_TEST(!field->HasFocus());
        open();
        press(owner, PadButton::A);
        press(owner, PadButton::Start);
        BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST(field->GetText() == "1");
    });
}

BOOST_AUTO_TEST_CASE(ExceptionCleanupRunsWithTheKeyboardOpen)
{
    KeyboardFixture fixture;
    bool reached = false;
    BOOST_CHECK_THROW(fixture.run([&] {
        fixture.begin();
        fixture.open();
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() != nullptr);
        reached = true;
        throwKeyboardCleanupProbe();
    }),
                      std::runtime_error);
    BOOST_TEST(reached);
    BOOST_TEST(fixture.desktop() == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
