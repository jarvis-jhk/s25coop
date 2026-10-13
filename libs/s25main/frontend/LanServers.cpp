// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "LanServers.h"
#include "gameData/MaxPlayers.h"
#include "s25util/Serializer.h"
#include <algorithm>
#include <stdexcept>

namespace {
std::string readString(Serializer& ser, unsigned maxLength)
{
    const unsigned length = ser.PopVarSize();
    if(length > maxLength || length > ser.GetBytesLeft())
        throw std::runtime_error("Invalid LAN string length");
    const auto* bytes = ser.PopAndDiscard(length);
    return std::string(reinterpret_cast<const char*>(bytes), length);
}

bool readBool(Serializer& ser)
{
    const unsigned value = ser.PopUnsignedChar();
    if(value > 1)
        throw std::runtime_error("Invalid LAN boolean");
    return value != 0;
}
} // namespace

namespace frontend {
LanDiscovery::LanDiscovery(const Config& config) : LANDiscoveryBase(config, false) {}

bool LanDiscovery::Start()
{
    if(!listening_)
    {
        listening_ = LANDiscoveryBase::Start();
        if(listening_)
            Refresh();
    }
    return listening_;
}

void LanDiscovery::Refresh()
{
    services_.clear();
    if(listening_)
        Broadcast(request.data.data(), sizeof(request));
}

bool LanDiscovery::Run()
{
    if(!listening_)
        return true;
    for(unsigned i = 0; i < packetBudget && IsDataAvailable(); ++i)
    {
        LANDiscoveryClient::ServiceInfo service{};
        PeerAddr address;
        // One extra byte detects an oversized datagram even when recvfrom truncates it on POSIX.
        std::array<char, sizeof(service.info) + 1> data{};
        if(socket.Recv(data.data(), data.size(), address) != static_cast<int>(sizeof(service.info)))
            continue;
        std::copy_n(data.begin(), service.info.data.size(), service.info.data.begin());
        if(service.info.GetMagic() != config.magicResponse || service.info.GetVersion() != config.version
           || service.info.GetIsActive() > 1)
            continue;
        service.ip = address.GetIp();
        if(!service.info.GetIsActive())
            services_.erase(service.ip);
        else if(services_.count(service.ip) || services_.size() < serviceLimit)
            services_[service.ip] = std::move(service);
    }
    return !IsDataAvailable();
}

std::string LanServer::Key() const
{
    return ip + ":" + std::to_string(info.port) + (info.isIPv6 ? "/6" : "/4");
}

std::optional<LanServer> ReadLanServer(const LANDiscoveryClient::ServiceInfo& service)
{
    try
    {
        const auto& payload = service.info.GetPayload();
        Serializer ser(payload.data(), payload.size());
        LanServer result;
        result.ip = service.ip;
        auto& info = result.info;
        info.name = readString(ser, 64);
        info.hasPwd = readBool(ser);
        info.map = readString(ser, 64);
        info.curNumPlayers = ser.PopUnsignedChar();
        info.maxNumPlayers = ser.PopUnsignedChar();
        info.port = ser.PopUnsignedShort();
        info.isIPv6 = readBool(ser);
        info.version = readString(ser, 16);
        info.revision = readString(ser, payload.size());
        if(result.ip.empty() || !info.port || !info.maxNumPlayers || info.maxNumPlayers > MAX_PLAYERS
           || info.curNumPlayers > info.maxNumPlayers)
            return std::nullopt;
        return result;
    } catch(const std::runtime_error&)
    {
        return std::nullopt;
    }
}
} // namespace frontend
