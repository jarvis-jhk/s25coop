// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MenuPadFixture.h"
#include "frontend/LanServers.h"
#include "gameData/LanDiscoveryCfg.h"
#include "s25util/Serializer.h"
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <chrono>
#include <limits>
#include <thread>

namespace {
LANDiscoveryClient::ServiceInfo service(LanGameInfo info)
{
    Serializer ser;
    info.Serialize(ser);
    LANDiscoveryClient::ServiceInfo result{};
    result.ip = "127.0.0.1";
    BOOST_TEST_REQUIRE(ser.GetLength() <= result.info.GetPayload().size());
    std::copy_n(ser.GetData(), ser.GetLength(), result.info.GetPayload().data());
    return result;
}
LanGameInfo game()
{
    return {"Alpha", true, "Map", 1, 4, 2345, false, "Version", "Revision"};
}
} // namespace

BOOST_AUTO_TEST_SUITE(LanServerTests)
BOOST_AUTO_TEST_CASE(ProductionSerializationRetainsEndpointAndMetadata)
{
    const auto packet = service(game());
    const auto parsed = frontend::ReadLanServer(packet);
    BOOST_TEST_REQUIRE(parsed.has_value());
    BOOST_TEST(parsed->info.name == "Alpha");
    BOOST_TEST(parsed->info.map == "Map");
    BOOST_TEST(parsed->info.hasPwd);
    BOOST_TEST(parsed->info.curNumPlayers == 1u);
    BOOST_TEST(parsed->info.maxNumPlayers == 4u);
    BOOST_TEST(parsed->info.version == "Version");
    BOOST_TEST(parsed->info.revision == "Revision");
    BOOST_TEST(parsed->Key() == "127.0.0.1:2345/4");
    auto ipv6 = game();
    ipv6.isIPv6 = true;
    ipv6.hasPwd = false;
    BOOST_TEST(frontend::ReadLanServer(service(ipv6))->Key() == "127.0.0.1:2345/6");
    BOOST_TEST(!frontend::ReadLanServer(service(ipv6))->info.hasPwd);
    auto longRevision = game();
    longRevision.revision = std::string(128, 'r');
    const auto longParsed = frontend::ReadLanServer(service(longRevision));
    BOOST_TEST_REQUIRE(longParsed.has_value());
    BOOST_TEST(longParsed->info.revision == longRevision.revision);
}
BOOST_AUTO_TEST_CASE(RejectsInvalidCountsPortsAndEmptyAddresses)
{
    for(unsigned field = 0; field < 4; ++field)
    {
        auto info = game();
        if(field == 0)
            info.port = 0;
        else if(field == 1)
            info.maxNumPlayers = 0;
        else if(field == 2)
            info.maxNumPlayers = 255;
        else
            info.curNumPlayers = 5;
        BOOST_TEST(!frontend::ReadLanServer(service(info)).has_value());
    }
    auto packet = service(game());
    packet.ip.clear();
    BOOST_TEST(!frontend::ReadLanServer(packet).has_value());
}
BOOST_AUTO_TEST_CASE(UntrustedStringLengthsAndBooleanBytesDoNotAllocateOrAssert)
{
    auto packet = service(game());
    packet.info.GetPayload()[6] = 2; // After length byte and "Alpha": password boolean.
    BOOST_TEST(!frontend::ReadLanServer(packet).has_value());
    for(const unsigned length : {65u, 128u, std::numeric_limits<unsigned>::max()})
    {
        Serializer ser;
        ser.PushVarSize(length);
        std::copy_n(ser.GetData(), ser.GetLength(), packet.info.GetPayload().data());
        BOOST_TEST(!frontend::ReadLanServer(packet).has_value());
    }
    packet.info.GetPayload().fill(static_cast<char>(0xff));
    BOOST_TEST(!frontend::ReadLanServer(packet).has_value());
    // Valid bounded lengths up to the fixed packet boundary, then an incomplete last string.
    Serializer ser;
    ser.PushString(std::string(64, 'n'));
    ser.PushBool(false);
    ser.PushString(std::string(64, 'm'));
    ser.PushUnsignedChar(1);
    ser.PushUnsignedChar(4);
    ser.PushUnsignedShort(2345);
    ser.PushBool(false);
    ser.PushString(std::string(16, 'v'));
    ser.PushVarSize(100);
    packet.info.GetPayload().fill(0);
    std::copy_n(ser.GetData(), ser.GetLength(), packet.info.GetPayload().data());
    BOOST_TEST(!frontend::ReadLanServer(packet).has_value());
}
BOOST_FIXTURE_TEST_CASE(DiscoveryBoundsInvalidTrafficAndRetainsTheProductionWireProtocol, rttr::test::MenuPadFixture)
{
    frontend::LanDiscovery discovery(LAN_DISCOVERY_CFG);
    BOOST_TEST(discovery.Run());
    discovery.Refresh();
    BOOST_TEST_REQUIRE(discovery.Start());
    BOOST_TEST(discovery.Start());
    Socket sender;
    BOOST_TEST_REQUIRE(sender.Create(AF_INET, true));
    // BSD sendto requires the IPv4 address length, not PeerAddr's storage-union size.
    sockaddr_in receiver{};
    receiver.sin_family = AF_INET;
    receiver.sin_port = htons(LAN_DISCOVERY_CFG.portResponse);
    receiver.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    LANDiscoveryBase::Info packet{};
    packet.GetMagic() = LAN_DISCOVERY_CFG.magicResponse;
    packet.GetVersion() = LAN_DISCOVERY_CFG.version;
    packet.GetIsActive() = 1;
    for(unsigned i = 0; i < frontend::LanDiscovery::packetBudget; ++i)
    {
        auto invalid = packet;
        const unsigned kind = i % 5;
        if(kind == 0)
            invalid.GetMagic()[0] = '?';
        else if(kind == 1)
            invalid.GetVersion() = 255;
        else if(kind == 2)
            invalid.GetIsActive() = 2;
        std::array<char, sizeof(invalid) + 1> bytes{};
        std::copy(invalid.data.begin(), invalid.data.end(), bytes.begin());
        const int size = sizeof(invalid) - (kind == 3 ? 1 : 0) + (kind == 4 ? 1 : 0);
        BOOST_TEST_REQUIRE(sendto(sender.GetSocket(), bytes.data(), size, 0,
                                  reinterpret_cast<const sockaddr*>(&receiver), sizeof(receiver))
                           == size);
    }
    packet.GetPayload() = service(game()).info.GetPayload();
    BOOST_TEST_REQUIRE(sendto(sender.GetSocket(), packet.data.data(), sizeof(packet), 0,
                              reinterpret_cast<const sockaddr*>(&receiver), sizeof(receiver))
                       == static_cast<int>(sizeof(packet)));
    BOOST_TEST(!discovery.Run());
    BOOST_TEST(discovery.GetServices().empty());
    BOOST_TEST(discovery.Run());
    BOOST_TEST_REQUIRE(discovery.GetServices().size() == 1u);
    BOOST_TEST_REQUIRE(frontend::ReadLanServer(discovery.GetServices().begin()->second).has_value());
    BOOST_TEST(frontend::ReadLanServer(discovery.GetServices().begin()->second)->Key() == "127.0.0.1:2345/4");
    packet.GetIsActive() = 0;
    BOOST_TEST_REQUIRE(sendto(sender.GetSocket(), packet.data.data(), sizeof(packet), 0,
                              reinterpret_cast<const sockaddr*>(&receiver), sizeof(receiver))
                       == static_cast<int>(sizeof(packet)));
    // Loopback delivery is asynchronous on macOS: the withdrawal may not be readable yet right after sendto.
    // The sleep sits in the loop header so the coverage gate sees it executed where the first read already succeeds.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    for(; !discovery.GetServices().empty() && std::chrono::steady_clock::now() < deadline;
        std::this_thread::sleep_for(std::chrono::milliseconds(1)))
        BOOST_TEST(discovery.Run());
    BOOST_TEST(discovery.GetServices().empty());
    discovery.Refresh();
    BOOST_TEST(discovery.Run());
}
BOOST_AUTO_TEST_SUITE_END()
