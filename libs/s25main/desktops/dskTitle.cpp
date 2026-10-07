// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dskTitle.h"
#include "Loader.h"
#include "RTTR_Version.h"
#include "WindowManager.h"
#include "controls/ctrlButton.h"
#include "desktops/dskHome.h"
#include "helpers/containerUtils.h"
#include "input/KeyGlyph.h"
#include "input/MenuPadInput.h"
#include "mygettext/mygettext.h"
#include "ogl/FontStyle.h"
#include "ogl/glFont.h"
#include "s25util/colors.h"
#include <algorithm>

namespace {
constexpr Extent startButtonSize(300, 48);
constexpr unsigned hintHeight = 40;

PadDeviceId DeviceInSlot0()
{
    const PadRouter& router = WINDOWMANAGER.GetPadInput().GetRouter();
    for(const PadDeviceId device : router.GetDevices())
    {
        if(router.GetSlot(device) == 0)
            return device;
    }
    return InvalidPadDevice;
}
} // namespace

dskTitle::dskTitle() : dskFrontEndPage(rttr::version::GetTitle())
{
    AddTextButton(ID_Start, DrawPoint(0, 0), startButtonSize, TextureColor::Green2, _("Start"), LargeFont);
    Layout();
}

std::unique_ptr<Desktop> dskTitle::Create()
{
    return dskFrontEndPage::Create([] { return std::make_unique<dskTitle>(); });
}

unsigned dskTitle::GetNumPadSlots() const
{
    return MenuPadInput::MaxSlots;
}

void dskTitle::Msg_PaintBefore()
{
    dskFrontEndPage::Msg_PaintBefore();
    MenuPadInput& pads = WINDOWMANAGER.GetPadInput();
    const PadRouter& router = pads.GetRouter();
    // An unplugged controller may join again when it comes back.
    seen_.erase(std::remove_if(seen_.begin(), seen_.end(), [&router](PadDeviceId d) { return !router.HasDevice(d); }),
                seen_.end());
    // The press that gave a controller its slot was swallowed (MenuPadInput's pick-up press); that press
    // IS the join. Only once per controller and page, so leaving with B sticks.
    for(const PadDeviceId device : router.GetDevices())
    {
        if(router.GetSlot(device) == PadRouter::NoSlot || helpers::contains(seen_, device))
            continue;
        seen_.push_back(device);
        pads.GetParty().Join(device);
    }
}

bool dskTitle::Msg_PadCommand(const unsigned slot, const PadButton button)
{
    MenuPadInput& pads = WINDOWMANAGER.GetPadInput();
    Party& party = pads.GetParty();
    const PadDeviceId device = pads.GetActingDevice();
    if(button == PadButton::A || button == PadButton::Start)
    {
        if(!party.Contains(device))
        {
            party.Join(device);
            return true;
        }
        if(button == PadButton::Start)
        {
            Msg_ButtonClick(ID_Start);
            return true;
        }
        return false; // a member's A clicks the focused button (Start)
    }
    if(button == PadButton::B)
    {
        if(party.Leave(device))
            return true;
        return dskFrontEndPage::Msg_PadCommand(slot, button);
    }
    return false;
}

void dskTitle::OnChoose(const unsigned ctrl_id)
{
    if(ctrl_id == ID_Start)
        WINDOWMANAGER.Switch(dskHome::Create());
}

void dskTitle::OnLayout()
{
    const Rect& content = GetFrame().content;
    const Extent size = content.getSize();
    hintArea_ = Rect(content.getOrigin(), Extent(size.x, std::min(hintHeight, size.y)));
    const int startTop = content.bottom - static_cast<int>(startButtonSize.y);
    if(auto* start = GetCtrl<Window>(ID_Start))
        start->SetPos(DrawPoint(content.left + static_cast<int>(size.x - startButtonSize.x) / 2, startTop));
    const int cardsTop = hintArea_.bottom + static_cast<int>(frontend::itemGap);
    const int cardsBottom = startTop - static_cast<int>(frontend::itemGap);
    const Rect cardsArea(DrawPoint(content.left, cardsTop),
                         Extent(size.x, static_cast<unsigned>(std::max(0, cardsBottom - cardsTop))));
    // One row, P1 to P4 from left to right - the order players join and the order of the strip. A 2×2 grid
    // would give bigger cards but no reading order.
    const unsigned n = Party::MaxMembers;
    const unsigned gap = frontend::itemGap;
    const Extent area = cardsArea.getSize();
    unsigned cardW = std::min(180u, area.x > (n - 1) * gap ? (area.x - (n - 1) * gap) / n : 1u);
    const unsigned cardH = std::max(1u, std::min(cardW * 4 / 3, area.y));
    cardW = std::max(1u, std::min(cardW, cardH * 3 / 4));
    const unsigned rowW = n * cardW + (n - 1) * gap;
    const DrawPoint origin = cardsArea.getOrigin()
                             + DrawPoint(static_cast<int>(area.x > rowW ? (area.x - rowW) / 2 : 0),
                                         static_cast<int>(area.y > cardH ? (area.y - cardH) / 2 : 0));
    cards_.clear();
    for(unsigned i = 0; i < n; ++i)
        cards_.push_back(Rect(origin + DrawPoint(static_cast<int>(i * (cardW + gap)), 0), Extent(cardW, cardH)));
}

std::vector<brief::KeyHint> dskTitle::FooterKeys(const FocusPath& /*focus*/) const
{
    using brief::KeyAction;
    if(!WINDOWMANAGER.GetPadInput().GetParty().Contains(DeviceInSlot0()))
        return {brief::KeyHint{PadButton::A, KeyAction::JoinParty}};
    return {brief::KeyHint{PadButton::A, KeyAction::Choose}, brief::KeyHint{PadButton::B, KeyAction::LeaveParty}};
}

void dskTitle::Draw_()
{
    dskFrontEndPage::Draw_();
    if(!NormalFont || !LargeFont)
        return;
    NormalFont->Draw(DrawPoint((hintArea_.left + hintArea_.right) / 2, (hintArea_.top + hintArea_.bottom) / 2),
                     _("Every player: press A to join"), FontStyle::CENTER | FontStyle::VCENTER, COLOR_YELLOW);
    const auto& members = WINDOWMANAGER.GetPadInput().GetParty().Members();
    for(unsigned i = 0; i < cards_.size(); ++i)
    {
        const Rect& card = cards_[i];
        const DrawPoint mid((card.left + card.right) / 2, (card.top + card.bottom) / 2);
        if(i < members.size())
        {
            brief::EmitKeyBadge(card, PLAYER_COLORS[i % PLAYER_COLORS.size()],
                                [](const Rect& r, unsigned c) { DrawRectangle(r, c); });
            LargeFont->Draw(mid, "P" + std::to_string(i + 1), FontStyle::CENTER | FontStyle::VCENTER, COLOR_WHITE);
        } else
        {
            DrawRectangle(card, 0x90000000);
            NormalFont->Draw(mid, _("Press A"), FontStyle::CENTER | FontStyle::VCENTER, COLOR_WHITE);
        }
    }
}
