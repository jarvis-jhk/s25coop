// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "ctrlGroup.h"
#include "network/ClientInterface.h"
#include "gameTypes/ServerType.h"
#include <functional>
#include <memory>

class ILobbyClient;

/// A connection occupies its form's full content area, retaining inputs privately for retries.
class ctrlConnectionProgress final : public ctrlGroup, public ClientInterface
{
public:
    ctrlConnectionProgress(Window* parent, unsigned id, std::function<void(ClientError)> onError);
    ~ctrlConnectionProgress() override;
    void Start(ServerType type);
    void Abort();
    bool IsConnecting() const { return connecting_; }
    void Resize(const Extent& size) override;
    void Msg_ButtonClick(unsigned id) override;
    enum ControlIds
    {
        ID_Status,
        ID_Progress,
        ID_Abort
    };

private:
    void CI_Error(ClientError error) override;
    void CI_NextConnectState(ConnectState state) override;
    void CI_MapPartReceived(uint32_t received, uint32_t total) override;
    void SetStatus(const std::string& text);
    ServerType type_ = ServerType::Direct;
    std::unique_ptr<ILobbyClient> lobbyClient_;
    std::function<void(ClientError)> onError_;
    unsigned short progress_ = 0;
    bool connecting_ = false;
};
