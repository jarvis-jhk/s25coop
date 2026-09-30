// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

// s25coop: the minimal start goods and the start goods chosen per player

#include "GamePlayer.h"
#include "JoinPlayerInfo.h"
#include "buildings/nobHQ.h"
#include "network/GameMessages.h"
#include "worldFixtures/CreateEmptyWorld.h"
#include "worldFixtures/WorldFixture.h"
#include "gameTypes/GameTypesOutput.h"
#include "s25util/Serializer.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(StartWaresSuite)

BOOST_AUTO_TEST_CASE(MinimalIsTheScriptsLoadout)
{
    const GoodsAndPeopleCounts inv = nobHQ::getStartInventory(StartWares::Minimal);
    GoodsAndPeopleCounts expected;
    expected[GoodType::Wood] = 2;
    expected[GoodType::Boards] = 2;
    expected[GoodType::Stones] = 4;
    expected[GoodType::Tongs] = 1;
    expected[GoodType::Hammer] = 1;
    expected[GoodType::Axe] = 1;
    expected[GoodType::Saw] = 1;
    expected[GoodType::Iron] = 2;
    expected[GoodType::IronOre] = 3;
    expected[GoodType::Coal] = 3;
    expected[Job::General] = 2;
    expected[Job::PackDonkey] = 1;
    BOOST_TEST(inv.goods == expected.goods, boost::test_tools::per_element());
    BOOST_TEST(inv.people == expected.people, boost::test_tools::per_element());

    const GoodsAndPeopleCounts plus = nobHQ::getStartInventory(StartWares::MinimalPlus);
    expected[GoodType::Boards] = 10;
    expected[GoodType::Stones] = 10;
    expected[GoodType::Shovel] = 1;
    expected[GoodType::Iron] = 10;
    expected[Job::Private] = 5;
    BOOST_TEST(plus.goods == expected.goods, boost::test_tools::per_element());
    BOOST_TEST(plus.people == expected.people, boost::test_tools::per_element());
}

using WorldFixtureEmpty2P = WorldFixture<CreateEmptyWorld, 2>;
BOOST_FIXTURE_TEST_CASE(EachPlayerGetsItsOwnStartGoods, WorldFixtureEmpty2P)
{
    ggs.startWares = StartWares::Normal;
    world.GetPlayer(1).startWares = StartWares::Minimal;
    addStartResources();

    const GoodsAndPeopleCounts normal = nobHQ::getStartInventory(StartWares::Normal);
    const GoodsAndPeopleCounts minimal = nobHQ::getStartInventory(StartWares::Minimal);
    const Inventory& inv0 = world.GetPlayer(0).GetHQ()->GetInventory();
    const Inventory& inv1 = world.GetPlayer(1).GetHQ()->GetInventory();
    BOOST_TEST(inv0.goods == normal.goods, boost::test_tools::per_element());
    BOOST_TEST(inv1.goods == minimal.goods, boost::test_tools::per_element());
    BOOST_TEST(inv1[Job::Helper] == 0u);
    // One general is kept in the HQ's reserve, as with the script
    BOOST_TEST(world.GetPlayer(1).GetInventory()[Job::General] == 2u);
    BOOST_TEST(inv1[Job::PackDonkey] == 1u);
    BOOST_TEST(inv0[Job::Helper] == normal[Job::Helper]);
}

BOOST_AUTO_TEST_CASE(StartGoodsGoOverTheNetwork)
{
    for(const std::optional<StartWares> value :
        {std::optional<StartWares>{}, std::optional<StartWares>{StartWares::VLow},
         std::optional<StartWares>{StartWares::MinimalPlus}})
    {
        JoinPlayerInfo player;
        player.ps = PlayerState::Occupied;
        player.name = "Jan";
        player.startWares = value;
        Serializer ser;
        player.Serialize(ser);
        const JoinPlayerInfo loaded(ser);
        BOOST_TEST((loaded.startWares == value));
        BOOST_TEST(loaded.name == "Jan");

        Serializer msgSer;
        GameMessage_Player_StartWares(3, value).Serialize(msgSer);
        GameMessage_Player_StartWares msg;
        msg.Deserialize(msgSer);
        BOOST_TEST(msg.player == 3u);
        BOOST_TEST((msg.startWares == value));
    }

    // A value past the end is refused rather than read as some loadout
    Serializer bad;
    bad.PushUnsignedChar(helpers::MaxEnumValue_v<StartWares> + 2u);
    BOOST_CHECK_THROW(BasePlayerInfo::popStartWares(bad), std::range_error);
}

BOOST_AUTO_TEST_SUITE_END()
