// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "desktops/ControllerPanelPages.h"
#include "AddonHelperFunctions.h"
#include "BuildingRegister.h"
#include "GamePlayer.h"
#include "Loader.h"
#include "Window.h"
#include "helpers/containerUtils.h"
#include "mygettext/mygettext.h"
#include "ogl/FontStyle.h"
#include "ogl/ITexture.h"
#include "ogl/glFont.h"
#include "world/GameWorld.h"
#include "gameData/BuildingConsts.h"
#include "gameData/BuildingProperties.h"
#include "gameData/GoodConsts.h"
#include "gameData/JobConsts.h"
#include "gameData/ShieldConsts.h"
#include "s25util/colors.h"
#include <algorithm>

namespace panel {
namespace {
    // The classic stock window's order (iwWares), so a player switching presentations finds things
    // in the same place.
    const std::vector<GoodType> wareOrder{
      GoodType::Wood,    GoodType::Boards,     GoodType::Stones,   GoodType::Ham,        GoodType::Grain,
      GoodType::Flour,   GoodType::Fish,       GoodType::Meat,     GoodType::Bread,      GoodType::Water,
      GoodType::Beer,    GoodType::Coal,       GoodType::IronOre,  GoodType::Gold,       GoodType::Iron,
      GoodType::Coins,   GoodType::Tongs,      GoodType::Axe,      GoodType::Saw,        GoodType::PickAxe,
      GoodType::Hammer,  GoodType::Shovel,     GoodType::Crucible, GoodType::RodAndLine, GoodType::Scythe,
      GoodType::Cleaver, GoodType::Rollingpin, GoodType::Bow,      GoodType::Sword,      GoodType::ShieldRomans,
      GoodType::Boat,    GoodType::Grapes,     GoodType::Wine,     GoodType::Skins,      GoodType::Leather,
      GoodType::Armor,
    };
    const std::vector<Job> jobOrder{
      Job::Helper,        Job::Builder,
      Job::Planer,        Job::Woodcutter,
      Job::Forester,      Job::Stonemason,
      Job::Fisher,        Job::Hunter,
      Job::Carpenter,     Job::Farmer,
      Job::PigBreeder,    Job::DonkeyBreeder,
      Job::Miller,        Job::Baker,
      Job::Butcher,       Job::Brewer,
      Job::Miner,         Job::IronFounder,
      Job::Armorer,       Job::Minter,
      Job::Metalworker,   Job::Shipwright,
      Job::Geologist,     Job::Scout,
      Job::PackDonkey,    Job::CharBurner,
      Job::Winegrower,    Job::Vintner,
      Job::TempleServant, Job::Skinner,
      Job::Tanner,        Job::LeatherWorker,
      Job::Private,       Job::PrivateFirstClass,
      Job::Sergeant,      Job::Officer,
      Job::General,
    };
    // The classic buildings window's order (iwBuildings): military, mines, then production.
    const std::vector<BuildingType> buildingOrder{
      BuildingType::Barracks,   BuildingType::Guardhouse,     BuildingType::Watchtower,
      BuildingType::Fortress,   BuildingType::GraniteMine,    BuildingType::CoalMine,
      BuildingType::IronMine,   BuildingType::GoldMine,       BuildingType::LookoutTower,
      BuildingType::Catapult,   BuildingType::Woodcutter,     BuildingType::Fishery,
      BuildingType::Quarry,     BuildingType::Forester,       BuildingType::Slaughterhouse,
      BuildingType::Hunter,     BuildingType::Brewery,        BuildingType::Armory,
      BuildingType::Metalworks, BuildingType::Ironsmelter,    BuildingType::PigFarm,
      BuildingType::Storehouse, BuildingType::Mill,           BuildingType::Bakery,
      BuildingType::Sawmill,    BuildingType::Mint,           BuildingType::Well,
      BuildingType::Shipyard,   BuildingType::Farm,           BuildingType::DonkeyBreeder,
      BuildingType::Charburner, BuildingType::HarborBuilding, BuildingType::Vineyard,
      BuildingType::Winery,     BuildingType::Temple,         BuildingType::Skinner,
      BuildingType::Tannery,    BuildingType::LeatherWorks,
    };

    /// Military buildings, stores and the lookout tower have no output to measure.
    bool HasProductivity(const BuildingType type)
    {
        return !BuildingProperties::IsMilitary(type) && !BuildingProperties::IsWareHouse(type)
               && type != BuildingType::LookoutTower && type != BuildingType::Catapult;
    }

    unsigned PercentColor(const unsigned percent)
    {
        if(percent >= 60)
            return COLOR_60_PERCENT;
        if(percent >= 30)
            return COLOR_30_PERCENT;
        if(percent >= 20)
            return COLOR_20_PERCENT;
        return COLOR_0_PERCENT;
    }

    void DrawIcon(ITexture* icon, const Rect& box)
    {
        if(!icon)
            return;
        const Extent size = icon->GetSize();
        if(size.x == 0 || size.y == 0)
            return;
        // Fit, never stretch: ware and building icons have very different aspect ratios.
        const unsigned scale100 =
          std::min(100u, std::min(box.getSize().x * 100 / size.x, box.getSize().y * 100 / size.y));
        const Extent dst(std::max(1u, size.x * scale100 / 100), std::max(1u, size.y * scale100 / 100));
        const Position pos = box.getOrigin() + (Position(box.getSize()) - Position(dst)) / 2;
        icon->Draw(Rect(pos, dst), Rect(Position(0, 0), size));
    }
} // namespace

std::vector<Row> StockRows(const GamePlayer& player)
{
    const GlobalGameSettings& ggs = player.GetGameWorld().GetGGS();
    const auto isUnusedWare = makeIsUnusedWare(ggs);
    const auto isUnusedJob = makeIsUnusedJob(ggs);
    const Inventory& inventory = player.GetInventory();
    std::vector<Row> rows;
    for(const GoodType ware : wareOrder)
    {
        if(isUnusedWare(ware))
            continue;
        const unsigned amount = inventory[ware];
        rows.push_back({rttr::enum_cast(ware), LOADER.GetWareTex(convertShieldToNation(ware, player.nation)),
                        _(WARE_NAMES[ware]), std::to_string(amount), amount ? COLOR_YELLOW : COLOR_RED});
    }
    for(const Job job : jobOrder)
    {
        if(isUnusedJob(job))
            continue;
        const unsigned amount = inventory[job];
        rows.push_back({JobRowBase + rttr::enum_cast(job), LOADER.GetJobTex(job), _(JOB_NAMES[job]),
                        std::to_string(amount), amount ? COLOR_YELLOW : COLOR_RED});
    }
    return rows;
}

std::vector<Row> BuildingRows(const GamePlayer& player)
{
    const auto isUnused = makeIsUnusedBuilding(player.GetGameWorld().GetGGS());
    const BuildingRegister& reg = player.GetBuildingRegister();
    const BuildingCount count = reg.GetBuildingNums();
    const auto productivity = reg.CalcProductivities();
    std::vector<Row> rows;
    for(const BuildingType type : buildingOrder)
    {
        if(isUnused(type))
            continue;
        const unsigned built = count.buildings[type];
        const unsigned sites = count.buildingSites[type];
        std::string value = std::to_string(built);
        if(sites)
            value += " (+" + std::to_string(sites) + ")";
        unsigned color = built ? COLOR_YELLOW : COLOR_GREY;
        if(built && HasProductivity(type))
        {
            value += "  " + std::to_string(productivity[type]) + "%";
            color = PercentColor(productivity[type]);
        }
        rows.push_back({rttr::enum_cast(type), LOADER.GetNationIcon(player.nation, type), _(BUILDING_NAMES[type]),
                        std::move(value), color});
    }
    return rows;
}

bool HasOwnPage(const Tab tab)
{
    return tab == Tab::Buildings || tab == Tab::Stock;
}

std::string TabName(const Tab tab)
{
    switch(tab)
    {
        case Tab::Buildings: return _("Buildings");
        case Tab::Stock: return _("Stock");
        case Tab::Statistics: return _("Statistics");
        case Tab::Post: return _("Post office");
        case Tab::Settings: return _("Menu");
    }
    return {};
}

std::string LegacyText(const Tab tab)
{
    switch(tab)
    {
        case Tab::Statistics: return _("A opens the statistics window.");
        case Tab::Post: return _("A opens your messages and the diary.");
        case Tab::Settings: return _("A opens the menu: map display, saving and leaving the game.");
        default: return {};
    }
}

void Draw(const ControllerPanelLayout& layout, const ControllerPanelNavigation& nav, const std::vector<Row>& rows,
          const unsigned seatColor)
{
    const glFont& font = *NormalFont;
    Window::DrawRectangle(layout.panel, 0xE0201810);
    Window::DrawRectangle(Rect(layout.panel.getOrigin(), Extent(layout.panel.getSize().x, 2)), seatColor);

    // Tabs: the current one in the middle, its neighbours at the ends. All five translated names
    // never fit the narrow tier, and LB/RB is what the hints at the ends say.
    const Tab tab = nav.GetTab();
    const auto neighbour = [tab](const int dir) {
        return static_cast<Tab>((static_cast<unsigned>(tab) + 5 + dir) % 5);
    };
    const Rect& tabs = layout.tabs;
    const int midY = tabs.top + static_cast<int>(tabs.getSize().y) / 2;
    const unsigned third = tabs.getSize().x / 3;
    font.Draw(DrawPoint(tabs.left, midY), "LB " + TabName(neighbour(-1)), FontStyle::LEFT | FontStyle::VCENTER,
              COLOR_GREY, static_cast<unsigned short>(third));
    font.Draw(DrawPoint(tabs.right, midY), TabName(neighbour(+1)) + " RB", FontStyle::RIGHT | FontStyle::VCENTER,
              COLOR_GREY, static_cast<unsigned short>(third));
    font.Draw(DrawPoint(tabs.left + static_cast<int>(tabs.getSize().x) / 2, midY), TabName(tab),
              FontStyle::CENTER | FontStyle::VCENTER, COLOR_YELLOW, static_cast<unsigned short>(third));
    Window::DrawRectangle(Rect(DrawPoint(tabs.left, tabs.bottom - 2), Extent(tabs.getSize().x, 1)), COLOR_GREY);

    const Rect& content = layout.content;
    std::string footer;
    if(!HasOwnPage(tab))
    {
        font.Draw(DrawPoint(content.left, content.top + 8), LegacyText(tab), FontStyle{}, COLOR_WHITE,
                  static_cast<unsigned short>(content.getSize().x));
        footer = _("A open   B close   LB/RB page");
    } else
    {
        const auto& page = nav.GetPage(tab);
        const unsigned visible = VisibleRows(layout);
        // A transient tiny resize cell has no room for rows; draw only the frame.
        if(content.getSize().x <= RowHeight + 6 || content.getSize().y < RowHeight)
            return;
        const unsigned end = std::min<unsigned>(page.firstVisibleRow + visible, rows.size());
        DrawPoint pos(content.left, content.top + 4);
        for(unsigned i = page.firstVisibleRow; i < end; ++i)
        {
            const Row& row = rows[i];
            const Rect rowRect(pos, Extent(content.getSize().x - 6, RowHeight));
            if(page.selectedId && *page.selectedId == row.id)
            {
                Window::DrawRectangle(rowRect, (seatColor & 0x00FFFFFF) | 0x60000000);
                Window::DrawRectangle(Rect(pos, Extent(2, RowHeight)), seatColor);
            }
            DrawIcon(row.icon, Rect(pos + DrawPoint(4, 2), Extent(RowHeight - 4, RowHeight - 4)));
            const int textY = pos.y + static_cast<int>(RowHeight) / 2;
            const unsigned valueWidth = font.getWidth(row.value);
            const unsigned nameRoom =
              rowRect.getSize().x > RowHeight + valueWidth + 16 ? rowRect.getSize().x - RowHeight - valueWidth - 16 : 0;
            font.Draw(DrawPoint(pos.x + static_cast<int>(RowHeight) + 4, textY), row.name,
                      FontStyle::LEFT | FontStyle::VCENTER, COLOR_WHITE, static_cast<unsigned short>(nameRoom));
            font.Draw(DrawPoint(rowRect.right - 4, textY), row.value, FontStyle::RIGHT | FontStyle::VCENTER,
                      row.valueColor);
            pos.y += static_cast<int>(RowHeight + RowGap);
        }
        // A scroll bar, so a long list never looks like it ended.
        if(rows.size() > visible)
        {
            const unsigned h = content.getSize().y;
            const unsigned barH = std::min(h, std::max(8u, h * visible / static_cast<unsigned>(rows.size())));
            const unsigned barY = (h - barH) * page.firstVisibleRow / static_cast<unsigned>(rows.size() - visible);
            Window::DrawRectangle(Rect(DrawPoint(content.right - 3, content.top), Extent(3, h)), 0x60FFFFFF);
            Window::DrawRectangle(
              Rect(DrawPoint(content.right - 3, content.top + static_cast<int>(barY)), Extent(3, barH)), COLOR_YELLOW);
        }
        footer = tab == Tab::Buildings ? _("A go there   B close   LB/RB page") : _("B close   LB/RB page");
    }
    font.Draw(DrawPoint(layout.footer.left, layout.footer.top + static_cast<int>(layout.footer.getSize().y) / 2),
              footer, FontStyle::LEFT | FontStyle::VCENTER, COLOR_GREY,
              static_cast<unsigned short>(layout.footer.getSize().x));
}

} // namespace panel
