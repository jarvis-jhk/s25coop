// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "controls/ctrlTextButton.h"
#include "input/LobbyPlayerCardModel.h"

/// One focus leaf per local seat. The lobby owns device authority and network acceptance.
class ctrlLobbyPlayerCard final : public ctrlTextButton
{
public:
    ctrlLobbyPlayerCard(Window* parent, unsigned id, DrawPoint pos, const Extent& size, const glFont* font,
                        LobbyPlayerCardModel model);
    LobbyPlayerCardModel& GetModel() { return model_; }
    const LobbyPlayerCardModel& GetModel() const { return model_; }
    void SetJoined(bool joined) { joined_ = joined; }
    bool IsJoined() const { return joined_; }
    // Cards use explicit D-pad row input. Analog travel must not escape into another owner's controls.
    bool CanStepValue(const Position& dir) const override { return dir != Position(0, 0); }

protected:
    void DrawContent() const override;

private:
    LobbyPlayerCardModel model_;
    bool joined_ = false;
};
