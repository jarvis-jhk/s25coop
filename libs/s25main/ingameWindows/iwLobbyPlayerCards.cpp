// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "iwLobbyPlayerCards.h"
#include "GameLobby.h"
#include "JoinPlayerInfo.h"
#include "Loader.h"
#include "controls/ctrlLobbyPlayerCard.h"
#include "controls/ctrlText.h"
#include "coop/CoopLobby.h"
#include "helpers/format.hpp"
#include "network/GameClient.h"
#include <algorithm>
#include <utility>

namespace {
constexpr unsigned cardsPerPage = 4;
constexpr unsigned previousId = cardsPerPage;
constexpr unsigned nextId = previousId + 1;
constexpr unsigned closeId = nextId + 1;
constexpr unsigned pageId = closeId + 1;
} // namespace

iwLobbyPlayerCards::iwLobbyPlayerCards(std::shared_ptr<const GameLobby> lobby)
    : IngameWindow(CGI_LOBBYPLAYERCARDS, posCenter, Extent(420, 310), _("Player cards"),
                   LOADER.GetImageN("resource", 41), true),
      lobby_(std::move(lobby))
{
    for(unsigned i = 0; i < cardsPerPage; ++i)
    {
        LobbyPlayerCardModel::Snapshot snapshot{{0, Nation::Romans, Team::None, false}, {}, {}};
        AddCtrl(std::make_unique<ctrlLobbyPlayerCard>(
          this, i, DrawPoint(20 + (static_cast<int>(i % 2) * 194), 30 + (static_cast<int>(i / 2) * 112)),
          Extent(186, 108), NormalFont, LobbyPlayerCardModel(i, std::move(snapshot)), true));
    }
    AddTextButton(previousId, DrawPoint(20, 266), Extent(100, 22), TextureColor::Grey, _("Previous"), NormalFont);
    AddTextButton(nextId, DrawPoint(130, 266), Extent(100, 22), TextureColor::Grey, _("Next"), NormalFont);
    AddTextButton(closeId, DrawPoint(294, 266), Extent(100, 22), TextureColor::Red1, _("Close"), NormalFont);
    AddText(pageId, DrawPoint(210, 248), "", COLOR_YELLOW, FontStyle::CENTER, NormalFont);
    Refresh();
}

void iwLobbyPlayerCards::Refresh()
{
    struct Entry
    {
        unsigned player;
        std::string name;
        bool shared;
    };
    std::vector<Entry> entries;
    const auto& members = GAMECLIENT.GetCoopMembers();
    for(unsigned i = 0; i < lobby_->getNumPlayers(); ++i)
    {
        const auto& player = lobby_->getPlayer(i);
        if(player.isUsed())
            entries.push_back(
              {i, coop::lobby::playerRowName(player.name, members, i), coop::lobby::countMembers(members, i) != 0});
    }
    for(const auto& member : members)
    {
        if(member.leader < lobby_->getNumPlayers() && lobby_->getPlayer(member.leader).isUsed())
            entries.push_back({member.leader, coop::lobby::memberLabel(*lobby_, member), true});
    }
    pageCount_ = std::max(1u, (static_cast<unsigned>(entries.size()) + cardsPerPage - 1) / cardsPerPage);
    page_ = std::min(page_, pageCount_ - 1);
    for(unsigned i = 0; i < cardsPerPage; ++i)
    {
        auto* card = GetCtrl<ctrlLobbyPlayerCard>(i);
        const unsigned index = (page_ * cardsPerPage) + i;
        card->SetVisible(index < entries.size());
        if(index >= entries.size())
            continue;
        const auto& entry = entries[index];
        const auto& player = lobby_->getPlayer(entry.player);
        LobbyPlayerCardModel::Snapshot snapshot{{player.color, player.nation, player.team, entry.shared}, {}, {}};
        snapshot.lockReasons.fill(_("Read-only"));
        card->GetModel().UpdateSnapshot(std::move(snapshot));
        card->SetText(entry.name);
        card->SetTooltip(entry.name);
        card->SetJoined(true);
    }
    GetCtrl<ctrlTextButton>(previousId)->SetEnabled(page_ != 0);
    GetCtrl<ctrlTextButton>(nextId)->SetEnabled(page_ + 1 < pageCount_);
    GetCtrl<ctrlText>(pageId)->SetText(helpers::format(_("Page %1% of %2%"), page_ + 1, pageCount_));
}

void iwLobbyPlayerCards::Msg_PaintBefore()
{
    IngameWindow::Msg_PaintBefore();
    Refresh();
}

void iwLobbyPlayerCards::Msg_ButtonClick(const unsigned id)
{
    if(id == closeId)
        Close();
    else if(id == previousId && page_ != 0)
        --page_;
    else if(id == nextId && page_ + 1 < pageCount_)
        ++page_;
    Refresh();
}
