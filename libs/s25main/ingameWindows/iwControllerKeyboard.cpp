// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "iwControllerKeyboard.h"
#include "Loader.h"
#include "WindowManager.h"
#include "controls/ctrlEdit.h"
#include "controls/ctrlTextButton.h"
#include "driver/KeyEvent.h"
#include "input/PadRouter.h"
#include "s25util/utf8.h"
#include <string_view>

namespace {
constexpr unsigned draftId = 0;
constexpr unsigned keyStart = 10;
constexpr unsigned deleteId = 100;
constexpr unsigned spaceId = 101;
constexpr unsigned shiftId = 102;
constexpr unsigned confirmId = 103;
constexpr unsigned cancelId = 104;
constexpr unsigned columns = 10;
constexpr std::u32string_view keys = U"1234567890qwertyuiopasdfghjkl:zxcvbnm.-_";

char32_t keyCharacter(unsigned index, bool shifted)
{
    const char32_t value = keys[index];
    return shifted && value >= U'a' && value <= U'z' ? value - U'a' + U'A' : value;
}
} // namespace

iwControllerKeyboard::iwControllerKeyboard(ctrlEdit& target, const PadDeviceId owner)
    : IngameWindow(CGI_CONTROLLER_KEYBOARD, posCenter, Extent(560, 290), _("Keyboard"),
                   LOADER.GetImageN("resource", 41), true, CloseBehavior::NoRightClick),
      target_(&target), targetLifetime_(target.KeyboardLifetime()),
      initialRevision_(*target.controllerKeyboardLifetime_), owner_(owner), initialText_(target.GetText())
{
    auto* draft = AddEdit(draftId, DrawPoint(20, 32), Extent(520, 26), TextureColor::Green2, NormalFont,
                          target.maxLength_, target.isPassword_);
    draft->SetType(target.editType_);
    draft->SetText(initialText_);
    draft->SetFocus();
    for(unsigned index = 0; index < keys.size(); ++index)
        AddTextButton(keyStart + index, DrawPoint(20 + (index % columns) * 52, 72 + (index / columns) * 32),
                      Extent(48, 28), TextureColor::Green2, s25util::utf32to8(std::u32string(1, keys[index])),
                      NormalFont);
    AddTextButton(deleteId, DrawPoint(20, 210), Extent(100, 26), TextureColor::Green2, _("Delete"), NormalFont);
    AddTextButton(spaceId, DrawPoint(125, 210), Extent(100, 26), TextureColor::Green2, _("Space"), NormalFont);
    AddTextButton(shiftId, DrawPoint(230, 210), Extent(100, 26), TextureColor::Green2, _("Shift"), NormalFont);
    AddTextButton(confirmId, DrawPoint(335, 210), Extent(100, 26), TextureColor::Green2, _("Confirm"), NormalFont);
    AddTextButton(cancelId, DrawPoint(440, 210), Extent(100, 26), TextureColor::Red1, _("Cancel"), NormalFont);
    AddText(1, DrawPoint(20, 250), _("A: type   B: delete   X: space   Y: shift   Menu: confirm"), COLOR_YELLOW,
            FontStyle::TOP, SmallFont);
}

bool iwControllerKeyboard::TargetAvailable() const
{
    // Do not inspect the raw target until its edit-owned lifetime is known to exist.
    const auto lifetime = targetLifetime_.lock();
    if(!lifetime || *lifetime != initialRevision_ || target_->isDisabled_ || WINDOWMANAGER.IsSwitchPending())
        return false;
    for(const Window* ancestor = target_->GetParent(); ancestor; ancestor = ancestor->GetParent())
    {
        const auto* window = dynamic_cast<const IngameWindow*>(ancestor);
        if(window && window->ShouldBeClosed())
            return false;
    }
    return true;
}

std::optional<bool> iwControllerKeyboard::AllowsMenuPadInput(const PadDeviceId device) const
{
    return !ShouldBeClosed() && device == owner_ && TargetAvailable();
}

void iwControllerKeyboard::ReconcileMenuPads(const PadRouter& router)
{
    if(!router.HasDevice(owner_) || !TargetAvailable())
        Close();
}

bool iwControllerKeyboard::HandleMenuPadButton(const PadDeviceId /*device*/, const PadButton button)
{
    switch(button)
    {
        case PadButton::B: Delete(); return true;
        case PadButton::X: Type(U' '); return true;
        case PadButton::Y: Shift(); return true;
        case PadButton::Start: Confirm(); return true;
        default: return false;
    }
}

void iwControllerKeyboard::Type(const char32_t character)
{
    auto* draft = GetCtrl<ctrlEdit>(draftId);
    draft->SetFocus();
    draft->Msg_KeyDown(KeyEvent(character));
}

void iwControllerKeyboard::Delete()
{
    auto* draft = GetCtrl<ctrlEdit>(draftId);
    draft->SetFocus();
    draft->Msg_KeyDown(KeyEvent(KeyType::Backspace));
}

void iwControllerKeyboard::Shift()
{
    shifted_ = !shifted_;
    for(unsigned index = 0; index < keys.size(); ++index)
        GetCtrl<ctrlTextButton>(keyStart + index)
          ->SetText(s25util::utf32to8(std::u32string(1, keyCharacter(index, shifted_))));
}

void iwControllerKeyboard::Confirm()
{
    // All entry paths validate ownership/lifetime before calling this private commit.
    const std::string text = GetCtrl<ctrlEdit>(draftId)->GetText();
    auto* target = target_;
    Close();
    // Notification may replace the target's parent or close this dialog. Touch neither afterwards.
    target->SetText(text);
}

bool iwControllerKeyboard::Msg_KeyDown(const KeyEvent& event)
{
    if(ShouldBeClosed() || !TargetAvailable())
        return true;
    if(event.kt == KeyType::Return)
        Confirm();
    else
    {
        auto* draft = GetCtrl<ctrlEdit>(draftId);
        draft->SetFocus();
        draft->Msg_KeyDown(event);
    }
    return true;
}

void iwControllerKeyboard::Msg_ButtonClick(const unsigned id)
{
    if(ShouldBeClosed() || !TargetAvailable())
        return;
    if(id >= keyStart && id < keyStart + keys.size())
        Type(keyCharacter(id - keyStart, shifted_));
    else
    {
        switch(id)
        {
            case deleteId: Delete(); break;
            case spaceId: Type(U' '); break;
            case shiftId: Shift(); break;
            case confirmId: Confirm(); break;
            case cancelId: Close(); break;
        }
    }
}
