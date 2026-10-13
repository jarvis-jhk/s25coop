// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "gameTypes/LanGameInfo.h"
#include "s25util/LANDiscoveryClient.h"
#include <optional>

namespace frontend {
/// The new browser shares the wire protocol, but bounds work and storage on its UI thread.
class LanDiscovery : private LANDiscoveryBase
{
public:
    explicit LanDiscovery(const Config& config);
    bool Start();
    void Refresh();
    /// true only after the current receive queue was drained; Join must defer otherwise.
    bool Run();
    const LANDiscoveryClient::ServiceMap& GetServices() const { return services_; }
    static constexpr unsigned packetBudget = 16;
    static constexpr unsigned serviceLimit = 128;

private:
    bool listening_ = false;
    LANDiscoveryClient::ServiceMap services_;
};

struct LanServer
{
    std::string ip;
    LanGameInfo info;
    std::string Key() const;
};

/// Discovery is untrusted UDP input, including lengths and boolean bytes.
std::optional<LanServer> ReadLanServer(const LANDiscoveryClient::ServiceInfo& service);
} // namespace frontend
