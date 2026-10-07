// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dskFrontEndPage.h"
#include "Loader.h"
#include "WindowManager.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlText.h"
#include "controls/ctrlTextButton.h"
#include "driver/KeyEvent.h"
#include "drivers/VideoDriverWrapper.h"
#include "frontend/PageKeys.h"
#include "input/KeyGlyph.h"
#include "input/MenuPadInput.h"
#include "mygettext/mygettext.h"
#include "ogl/FontStyle.h"
#include "ogl/glArchivItem_Bitmap.h"
#include "ogl/glFont.h"
#include "s25util/colors.h"
#include <algorithm>

namespace {
constexpr Extent backButtonSize(96, 28);
constexpr Extent stripBadge(30, 22);
constexpr unsigned barColor = 0xB4000000;
constexpr unsigned footerLines = 2;
constexpr unsigned itemTextPadding = 8;

unsigned lineHeight()
{
    return NormalFont ? NormalFont->getHeight() + 2u : 14u;
}
} // namespace

dskFrontEndPage::dskFrontEndPage(std::string title) : Desktop(nullptr), pageBackground_(LOADER.GetImageN("menu", 0))
{
    // Positions are actual render units (Layout), not an 800×600 design scaled up by the window.
    SetScale(false);
    AddTextButton(ID_btBack, DrawPoint(0, 0), backButtonSize, TextureColor::Red1, _("Back"), NormalFont);
    AddText(ID_txtTitle, DrawPoint(0, 0), title, COLOR_YELLOW, FontStyle::LEFT | FontStyle::VCENTER, LargeFont);
    Layout();
}

dskFrontEndPage::~dskFrontEndPage() = default;

std::unique_ptr<Desktop> dskFrontEndPage::Create(const Factory& factory)
{
    auto page = factory();
    if(auto* fe = dynamic_cast<dskFrontEndPage*>(page.get()))
    {
        fe->self_ = factory;
        fe->Layout();
    }
    return page;
}

Desktop* dskFrontEndPage::Show(const Factory& factory)
{
    return WINDOWMANAGER.Switch(Create(factory));
}

Desktop* dskFrontEndPage::Open(const Factory& next)
{
    if(Leaving())
        return nullptr;
    auto page = next();
    if(auto* fe = dynamic_cast<dskFrontEndPage*>(page.get()))
    {
        fe->trail_ = trail_;
        // A page created directly has no factory for itself; the trail then simply ends below the new page.
        if(self_)
            fe->trail_.push_back(TrailEntry{self_, lastChosen_});
        fe->self_ = next;
        fe->Layout();
    }
    return WINDOWMANAGER.Switch(std::move(page));
}

bool dskFrontEndPage::CanGoBack() const
{
    return !trail_.empty();
}

bool dskFrontEndPage::Leaving()
{
    return WINDOWMANAGER.IsSwitchPending();
}

bool dskFrontEndPage::GoBack()
{
    if(Leaving())
        return true;
    if(trail_.empty())
        return OnBackAtRoot();
    const TrailEntry& back = trail_.back();
    auto page = back.factory();
    if(auto* fe = dynamic_cast<dskFrontEndPage*>(page.get()))
    {
        fe->trail_.assign(trail_.begin(), trail_.end() - 1);
        fe->self_ = back.factory;
        if(back.focusId)
        {
            fe->restoreFocus_ = back.focusId;
            fe->lastChosen_ = back.focusId;
        }
        fe->Layout();
    }
    WINDOWMANAGER.Switch(std::move(page));
    return true;
}

bool dskFrontEndPage::Msg_PadCommand(unsigned /*slot*/, const PadButton button)
{
    if(button == PadButton::B)
        return GoBack();
    return false;
}

bool dskFrontEndPage::Msg_KeyDown(const KeyEvent& ke)
{
    if(ke.kt == KeyType::Escape)
        return GoBack();
    return false;
}

void dskFrontEndPage::Msg_ButtonClick(const unsigned ctrl_id)
{
    if(Leaving())
        return;
    if(ctrl_id == ID_btBack)
    {
        GoBack();
        return;
    }
    lastChosen_ = ctrl_id;
    OnChoose(ctrl_id);
}

void dskFrontEndPage::Msg_ScreenResize(const ScreenResizeEvent& sr)
{
    Desktop::Msg_ScreenResize(sr);
    Layout();
}

void dskFrontEndPage::SetActive(const bool activate)
{
    Desktop::SetActive(activate);
    const Extent renderSize = VIDEODRIVER.GetRenderSize();
    if(activate && GetSize() != renderSize)
    {
        Resize(renderSize);
        Layout();
    }
}

Window* dskFrontEndPage::GetPadEntryCtrl(unsigned /*slot*/)
{
    if(restoreFocus_)
    {
        Window* wnd = GetCtrl<Window>(restoreFocus_);
        if(wnd && wnd->IsVisible() && wnd->CanFocus())
            return wnd;
    }
    for(const unsigned id : items_)
    {
        Window* wnd = GetCtrl<Window>(id);
        if(wnd && wnd->IsVisible() && wnd->CanFocus())
            return wnd;
    }
    return nullptr;
}

void dskFrontEndPage::UseTiles(const Extent& aspect, const Extent& maxTile)
{
    RTTR_Assert(aspect.x && aspect.y);
    layout_ = ContentLayout::Tiles;
    tileAspect_ = aspect;
    maxTile_ = maxTile;
    Layout();
}

void dskFrontEndPage::UseList(const unsigned rowHeight, const unsigned maxWidth)
{
    layout_ = ContentLayout::List;
    rowHeight_ = rowHeight;
    listWidth_ = maxWidth;
    Layout();
}

ctrlButton* dskFrontEndPage::AddItem(const unsigned id, const std::string& label, const TextureColor tc)
{
    RTTR_Assert(id >= ID_FIRST_FREE);
    RTTR_Assert(items_.empty() || items_.back() < id);
    auto* button = AddTextButton(id, DrawPoint(0, 0), Extent(1, 1), tc, label, LargeFont);
    items_.push_back(id);
    Layout();
    return button;
}

void dskFrontEndPage::SetTitle(const std::string& title)
{
    GetCtrl<ctrlText>(ID_txtTitle)->SetText(title);
}

void dskFrontEndPage::Layout()
{
    frame_ = frontend::LayoutFrame(GetSize(), footerLines, lineHeight());
    const Rect& header = frame_.header;
    const int headerMid = header.top + static_cast<int>(header.getSize().y / 2);

    auto* back = GetCtrl<ctrlButton>(ID_btBack);
    back->SetVisible(HasBackAction());
    back->SetPos(DrawPoint(static_cast<int>(frontend::pageMargin), headerMid - static_cast<int>(backButtonSize.y / 2)));
    const int titleLeft = static_cast<int>(frontend::pageMargin)
                          + (back->IsVisible() ? static_cast<int>(backButtonSize.x + frontend::pageMargin) : 0);
    GetCtrl<ctrlText>(ID_txtTitle)->SetPos(DrawPoint(titleLeft, headerMid));

    std::vector<unsigned> shown;
    for(const unsigned id : items_)
    {
        if(GetCtrl<Window>(id)->IsVisible())
            shown.push_back(id);
    }
    std::vector<Rect> rects;
    if(layout_ == ContentLayout::Tiles)
        rects = frontend::LayoutTiles(frame_.content, static_cast<unsigned>(shown.size()), tileAspect_, maxTile_).tiles;
    else
        rects = frontend::LayoutList(frame_.content, static_cast<unsigned>(shown.size()), rowHeight_, listWidth_);
    for(unsigned i = 0; i < std::min(shown.size(), rects.size()); ++i)
    {
        auto* item = GetCtrl<Window>(shown[i]);
        item->SetPos(rects[i].getOrigin());
        item->Resize(rects[i].getSize());
        // Large text where it fits, else the normal font: a long translation on a small tile must not run
        // over the tile's edges.
        if(auto* text = dynamic_cast<ctrlTextButton*>(item); text && LargeFont && NormalFont)
        {
            const Extent size = rects[i].getSize();
            const bool large = LargeFont->getWidth(text->GetText()) + 2 * itemTextPadding <= size.x
                               && LargeFont->getHeight() + itemTextPadding <= size.y;
            text->SetFont(large ? LargeFont : NormalFont);
        }
    }
    OnLayout();
}

std::vector<brief::KeyHint> dskFrontEndPage::FooterKeys(const FocusPath& focus) const
{
    return frontend::PageKeys(focus, HasBackAction());
}

void dskFrontEndPage::Draw_()
{
    if(pageBackground_)
        pageBackground_->DrawFull(GetDrawRect());
    DrawRectangle(frame_.header, barColor);
    DrawRectangle(frame_.footer, barColor);
    DrawPlayerStrip();
    DrawFooter();
    Desktop::Draw_();
}

void dskFrontEndPage::DrawPlayerStrip()
{
    // Player number and colour index per badge: the party that joined on the title page (F2), else every
    // slot that has a controller in hand (a page shown without going through the title, e.g. in tests).
    const MenuPadInput& pads = WINDOWMANAGER.GetPadInput();
    std::vector<unsigned> players;
    const Party& party = pads.GetParty();
    for(unsigned i = 0; i < party.Members().size(); ++i)
        players.push_back(i);
    if(players.empty())
    {
        for(unsigned slot = 0; slot < MenuPadInput::MaxSlots; ++slot)
        {
            if(pads.HasDevice(slot))
                players.push_back(slot);
        }
    }
    stripRects_ = frontend::LayoutPlayerStrip(frame_.header, static_cast<unsigned>(players.size()), stripBadge);
    for(unsigned i = 0; i < players.size(); ++i)
    {
        // The same colour as the player's focus ring (MenuPadInput::DrawRings) and their in-game seat.
        const unsigned color = PLAYER_COLORS[players[i] % PLAYER_COLORS.size()];
        brief::EmitKeyBadge(stripRects_[i], color, [](const Rect& r, unsigned c) { DrawRectangle(r, c); });
        if(NormalFont)
        {
            const Rect& r = stripRects_[i];
            NormalFont->Draw(DrawPoint((r.left + r.right) / 2, (r.top + r.bottom) / 2),
                             "P" + std::to_string(players[i] + 1), FontStyle::CENTER | FontStyle::VCENTER, COLOR_WHITE);
        }
    }
}

void dskFrontEndPage::DrawFooter()
{
    // The help line belongs to the controller that owns the page (slot 0, MenuPadInput gives menus one slot).
    // Without a controller in hand there is nothing to explain: the mouse sees the buttons themselves.
    const MenuPadInput& pads = WINDOWMANAGER.GetPadInput();
    const FocusPath& focus = pads.GetFocus(0);
    if(pads.HasDevice(0) && focus.GetRoot() == this)
        footerKeys_ = FooterKeys(focus);
    else
        footerKeys_.clear();
    if(footerKeys_.empty() || !NormalFont)
        return;
    const glFont& font = *NormalFont;
    const auto textWidth = static_cast<unsigned short>(
      std::max(16, static_cast<int>(frame_.footer.getSize().x) - 2 * static_cast<int>(frontend::pageMargin)));
    const auto lines = brief::LayoutKeyGlyphs(footerKeys_, font, textWidth, COLOR_WHITE);
    DrawPoint pos = frame_.footer.getOrigin() + DrawPoint(static_cast<int>(frontend::pageMargin), 4);
    // The footer has room for footerLines; a longer wrap (a long translation on 800 px) is cut rather
    // than drawn off the screen.
    for(const auto& line : lines)
    {
        if(pos.y + static_cast<int>(lineHeight()) > frame_.footer.bottom)
            break;
        if(line.runs.empty())
            font.Draw(pos, line.text, FontStyle{}, COLOR_WHITE);
        else
            brief::DrawKeyRuns(pos, line.runs, font, [](const Rect& r, unsigned c) { DrawRectangle(r, c); });
        pos.y += static_cast<int>(lineHeight());
    }
}
