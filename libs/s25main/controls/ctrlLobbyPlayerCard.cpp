// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ctrlLobbyPlayerCard.h"
#include "helpers/EnumArray.h"
#include "ogl/glFont.h"
#include "gameData/NationConsts.h"
#include "s25util/colors.h"
#include <array>
#include <mygettext/mygettext.h>
#include <utility>

ctrlLobbyPlayerCard::ctrlLobbyPlayerCard(Window* parent, const unsigned id, const DrawPoint pos, const Extent& size,
                                         const glFont* font, LobbyPlayerCardModel model, const bool readOnly)
    : ctrlTextButton(parent, id, pos, size, TextureColor::Green2, "", font, ""), model_(std::move(model)),
      readOnly_(readOnly)
{}

bool ctrlLobbyPlayerCard::Msg_LeftDown(const MouseCoords& mc)
{
    return !readOnly_ && ctrlTextButton::Msg_LeftDown(mc);
}

bool ctrlLobbyPlayerCard::Msg_LeftUp(const MouseCoords& mc)
{
    return !readOnly_ && ctrlTextButton::Msg_LeftUp(mc);
}

void ctrlLobbyPlayerCard::DrawContent() const
{
    const auto origin = GetDrawPos();
    const auto width = static_cast<unsigned short>(GetSize().x - 8u);
    font->Draw(origin + DrawPoint(4, 3), GetText(), FontStyle::LEFT, COLOR_YELLOW, cursorColor_ ? width - 70u : width);
    if(cursorColor_)
    {
        font->Draw(origin + DrawPoint(GetSize().x - 70, 3), _("Cursor"), FontStyle::LEFT, COLOR_WHITE, 48);
        DrawRectangle(Rect(origin + DrawPoint(GetSize().x - 18, 5), Extent(10, 10)), *cursorColor_);
    }
    if(!joined_)
        return;
    const auto& values = model_.GetSnapshot().values;
    constexpr helpers::EnumArray<const char*, Team> teams = {"-", "?", "1", "2", "3", "4", "1-2", "1-3", "1-4"};
    const std::array labels = {std::string(_("Color")), std::string(_("Race")) + ": " + _(NationNames[values.nation]),
                               std::string(_("Team")) + ": " + teams[values.team],
                               std::string(_("Tribe")) + ": " + (values.sharedTribe ? _("Together") : _("Own"))};
    for(unsigned row = 0; row < labels.size(); ++row)
    {
        const DrawPoint pos = origin + DrawPoint(4, 22 + (static_cast<int>(row) * 16));
        const bool selected = !readOnly_ && static_cast<unsigned>(model_.GetRow()) == row;
        if(selected)
            DrawRectangle(Rect(pos, Extent(GetSize().x - 8u, 16)), 0xFF304860);
        font->Draw(pos, labels[row], FontStyle::LEFT,
                   model_.GetSnapshot().lockReasons[row].empty() ? COLOR_WHITE : 0xFFBBBBBB, row == 0 ? 65 : width);
        if(row == 0)
        {
            constexpr unsigned swatchWidth = 6;
            for(unsigned i = 0; i < PLAYER_COLORS.size(); ++i)
            {
                const DrawPoint swatch = pos + DrawPoint(68 + static_cast<int>(i * swatchWidth), 2);
                if(PLAYER_COLORS[i] == values.color)
                    DrawRectangle(Rect(swatch - DrawPoint(1, 1), Extent(swatchWidth, 13)), COLOR_WHITE);
                DrawRectangle(Rect(swatch, Extent(swatchWidth - 2u, 11)), PLAYER_COLORS[i]);
            }
        }
    }
    const auto& reason = model_.GetLockReason();
    font->Draw(origin + DrawPoint(4, 88), reason.empty() ? _("D-pad: edit; B: back") : reason, FontStyle::LEFT,
               COLOR_YELLOW, width);
}
