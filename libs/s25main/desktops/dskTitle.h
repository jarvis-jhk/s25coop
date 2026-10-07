// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "desktops/dskFrontEndPage.h"
#include <memory>
#include <vector>

/// The title page (doc/coop/FrontEnd.md, F2): the first screen after the splash.
///
/// Every controller joins by being used once - the press that gives it a slot here is the join, so
/// "everybody presses A" really is one press each. Joined controllers form the Party that the
/// player strip shows on every later page. A or Start by a member, or a click on Start, opens Home;
/// B takes a member out of the party again.
class dskTitle : public dskFrontEndPage
{
public:
    dskTitle();
    static std::unique_ptr<Desktop> Create();

    /// Every controller gets its own slot here, so each one's first press is seen.
    unsigned GetNumPadSlots() const override;
    /// A and Start from a controller that has not joined are a join, not a click on the focused button.
    bool HandlesPadControlCommands() const override { return true; }
    bool Msg_PadCommand(unsigned slot, PadButton button) override;
    void Msg_PaintBefore() override;

    enum ControlIds
    {
        ID_Start = dskFrontEndPage::ID_FIRST_FREE,
    };

    /// The player cards as last laid out, one per possible member.
    const std::vector<Rect>& GetCards() const { return cards_; }

protected:
    void OnChoose(unsigned ctrl_id) override;
    void OnLayout() override;
    std::vector<brief::KeyHint> FooterKeys(const FocusPath& focus) const override;
    void Draw_() override;

private:
    /// Devices that have already been offered their automatic join on this page. One who left with B
    /// stays out until they press A again.
    std::vector<PadDeviceId> seen_;
    std::vector<Rect> cards_;
    Rect hintArea_;
};
