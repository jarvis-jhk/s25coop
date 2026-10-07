// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "frontend/PageKeys.h"
#include "Window.h"
#include "input/FocusPath.h"

namespace frontend {

std::vector<brief::KeyHint> PageKeys(const FocusPath& focus, const bool canGoBack)
{
    using brief::KeyAction;
    std::vector<brief::KeyHint> keys;
    const auto add = [&keys](const PadButton button, const KeyAction action) {
        keys.push_back(brief::KeyHint{button, action});
    };
    const Window* focused = focus.IsActive() ? focus.GetFocused() : nullptr;
    if(focused && focused->CanActivate())
        add(PadButton::A, KeyAction::Choose);
    if(focused)
    {
        // Left, right, up, down: the order in which GroupKeys merges equal neighbours ("Left/Right Adjust").
        const std::pair<PadButton, Position> dirs[] = {{PadButton::DpadLeft, Position(-1, 0)},
                                                       {PadButton::DpadRight, Position(1, 0)},
                                                       {PadButton::DpadUp, Position(0, -1)},
                                                       {PadButton::DpadDown, Position(0, 1)}};
        for(const auto& [button, dir] : dirs)
        {
            switch(focus.PeekStep(dir))
            {
                case FocusPath::StepEffect::ChangeValue: add(button, KeyAction::AdjustValue); break;
                case FocusPath::StepEffect::MoveFocus: add(button, KeyAction::MoveFocus); break;
                // Text controls are no focus stations (ctrlEdit::CanFocus); silence beats a guessed word.
                case FocusPath::StepEffect::MoveTextCursor:
                case FocusPath::StepEffect::None: break;
            }
        }
    }
    if(canGoBack)
        add(PadButton::B, KeyAction::PageBack);
    return keys;
}

} // namespace frontend
