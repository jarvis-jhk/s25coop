// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "controls/ctrlTextButton.h"
#include "input/LobbyPlayerCardModel.h"

/// A local seat editor or a read-only roster card. The lobby owns network acceptance.
class ctrlLobbyPlayerCard final : public ctrlTextButton
{
public:
    ctrlLobbyPlayerCard(Window* parent, unsigned id, DrawPoint pos, const Extent& size, const glFont* font,
                        LobbyPlayerCardModel model, bool readOnly = false);
    LobbyPlayerCardModel& GetModel() { return model_; }
    const LobbyPlayerCardModel& GetModel() const { return model_; }
    void SetJoined(bool joined) { joined_ = joined; }
    bool IsJoined() const { return joined_; }
    void SetCursorColor(std::optional<unsigned> color) { cursorColor_ = color; }
    std::optional<unsigned> GetCursorColor() const { return cursorColor_; }
    bool IsReadOnly() const { return readOnly_; }
    bool CanActivate() const override { return !readOnly_ && ctrlTextButton::CanActivate(); }
    bool Msg_LeftDown(const MouseCoords& mc) override;
    bool Msg_LeftUp(const MouseCoords& mc) override;
    // Cards use explicit D-pad row input. Analog travel must not escape into another owner's controls.
    bool CanStepValue(const Position& dir) const override { return !readOnly_ && dir != Position(0, 0); }

protected:
    void DrawContent() const override;

private:
    LobbyPlayerCardModel model_;
    bool joined_ = false;
    bool readOnly_;
    std::optional<unsigned> cursorColor_;
};
