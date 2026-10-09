// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "IngameWindow.h"
#include <memory>
#include <string>

class ctrlEdit;
struct KeyEvent;

/// Reusable menu modal. Confirm commits text once; it never forwards an Enter action
/// to the original parent (for example, silently sending chat or starting a connection).
class iwControllerKeyboard : public IngameWindow
{
public:
    iwControllerKeyboard(ctrlEdit& target, PadDeviceId owner);
    std::optional<bool> AllowsMenuPadInput(PadDeviceId device) const override;
    bool HandleMenuPadButton(PadDeviceId device, PadButton button) override;
    void ReconcileMenuPads(const PadRouter& router) override;
    bool Msg_KeyDown(const KeyEvent& event) override;

protected:
    void Msg_ButtonClick(unsigned id) override;

private:
    bool TargetAvailable() const;
    void Type(char32_t character);
    void Delete();
    void Shift();
    void Confirm();

    ctrlEdit* target_;
    std::weak_ptr<unsigned> targetLifetime_;
    const unsigned initialRevision_;
    const PadDeviceId owner_;
    const std::string initialText_;
    bool shifted_ = false;
};
