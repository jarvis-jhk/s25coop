// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "IngameWindow.h"
#include <memory>

class GameLobby;

/// A live, read-only roster. The normal lobby retains all editing and host authority.
class iwLobbyPlayerCards final : public IngameWindow
{
public:
    explicit iwLobbyPlayerCards(std::shared_ptr<const GameLobby> lobby);
    void Msg_PaintBefore() override;
    void Msg_ButtonClick(unsigned id) override;

private:
    void Refresh();
    std::shared_ptr<const GameLobby> lobby_;
    unsigned page_ = 0;
    unsigned pageCount_ = 1;
};
