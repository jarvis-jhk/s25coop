// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dskFrontEndCampaigns.h"
#include "Loader.h"
#include "RttrConfig.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlText.h"
#include "driver/KeyEvent.h"
#include "dskFrontEndMissions.h"
#include "files.h"
#include "helpers/format.hpp"
#include "mygettext/mygettext.h"
#include "ogl/glFont.h"
#include "libsiedler2/libsiedler2.h"
#include <algorithm>

dskFrontEndCampaigns::dskFrontEndCampaigns(std::shared_ptr<boost::filesystem::path> selection)
    : dskFrontEndPage(_("Campaigns")), selection_(std::move(selection))
{
    for(unsigned i = 0; i < artwork_.size(); ++i)
        AddItem(ID_FirstCampaign + i, "");
    AddTextButton(ID_Previous, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, _("Previous"), NormalFont);
    AddTextButton(ID_Next, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, _("Next"), NormalFont);
    AddTextButton(ID_Refresh, DrawPoint(0, 0), Extent(1, 1), TextureColor::Grey, _("Refresh"), NormalFont);
    AddText(ID_Status, DrawPoint(0, 0), "", COLOR_YELLOW, FontStyle{}, NormalFont);
    Refresh();
}

void dskFrontEndCampaigns::Refresh()
{
    catalog_ = frontend::ScanCampaigns(
      {RTTRCONFIG.ExpandPath(s25::folders::campaignsBuiltin), RTTRCONFIG.ExpandPath(s25::folders::campaignsUser)});
    first_ = 0;
    keyboardChoice_ = 0;
    for(unsigned i = 0; i < catalog_.entries.size(); ++i)
    {
        if(catalog_.entries[i].folder == *selection_)
        {
            first_ = i / 6 * 6;
            keyboardChoice_ = i;
        }
    }
    FillPage();
}

const glArchivItem_Bitmap* dskFrontEndCampaigns::GetArtwork(const unsigned tile) const
{
    return tile < artwork_.size() && !artwork_[tile].empty() ?
             dynamic_cast<const glArchivItem_Bitmap*>(artwork_[tile][0]) :
             nullptr;
}

void dskFrontEndCampaigns::FillPage()
{
    for(unsigned i = 0; i < artwork_.size(); ++i)
    {
        artwork_[i].clear();
        if(first_ + i >= catalog_.entries.size())
            continue;
        const auto& campaign = catalog_.entries[first_ + i].description;
        const auto image = campaign.selectionMapData ?
                             std::optional<ImageResource>(campaign.selectionMapData->background) :
                             (campaign.image ? std::optional<ImageResource>(*campaign.image) : std::nullopt);
        if(image)
        {
            // Optional artwork cannot discard a working campaign or reuse an earlier cached picture.
            if(libsiedler2::Load(RTTRCONFIG.ExpandPath(image->filePath), artwork_[i]) == 0
               && image->index < artwork_[i].size())
            {
                auto chosen = artwork_[i].release(image->index);
                artwork_[i].clear();
                artwork_[i].push(std::move(chosen));
            } else
                artwork_[i].clear();
        }
    }
    Layout();
}

void dskFrontEndCampaigns::OnLayout()
{
    auto* status = GetCtrl<ctrlText>(ID_Status);
    if(!status)
        return;
    const auto area = GetFrame().content;
    layoutUnavailable_ = area.getSize().x < 480 || area.getSize().y < 250;
    const unsigned shown = static_cast<unsigned>(std::min<size_t>(6, catalog_.entries.size() - first_));
    for(unsigned i = 0; i < 6; ++i)
        GetCtrl<Window>(ID_FirstCampaign + i)->SetVisible(!layoutUnavailable_ && i < shown);
    for(unsigned id = ID_Previous; id <= ID_Refresh; ++id)
        GetCtrl<Window>(id)->SetVisible(!layoutUnavailable_);
    status->SetPos(area.getOrigin());
    status->setMaxWidth(area.getSize().x);
    if(layoutUnavailable_)
    {
        status->SetText(_("Reduce the UI scale or enlarge the window."));
        return;
    }
    status->SetText(
      helpers::format(_("%1% campaigns; %2% unavailable"), catalog_.entries.size(), catalog_.failures.size()));
    const Rect tiles(DrawPoint(area.left, area.top + 28), Extent(area.getSize().x, area.getSize().y - 80));
    const auto grid = frontend::LayoutTiles(tiles, shown, Extent(4, 3), Extent(320, 230));
    columns_ = std::max(1u, grid.columns);
    for(unsigned i = 0; i < grid.tiles.size(); ++i)
    {
        auto* tile = GetCtrl<Window>(ID_FirstCampaign + i);
        tile->SetPos(grid.tiles[i].getOrigin());
        tile->Resize(grid.tiles[i].getSize());
    }
    const unsigned width = (area.getSize().x - 2 * frontend::itemGap) / 3;
    for(unsigned id = ID_Previous; id <= ID_Refresh; ++id)
    {
        GetCtrl<Window>(id)->SetPos(
          DrawPoint(area.left + static_cast<int>((id - ID_Previous) * (width + frontend::itemGap)), area.bottom - 36));
        GetCtrl<Window>(id)->Resize(Extent(width, 36));
    }
    GetCtrl<ctrlButton>(ID_Previous)->SetEnabled(first_ > 0);
    GetCtrl<ctrlButton>(ID_Next)->SetEnabled(first_ + 6 < catalog_.entries.size());
}

void dskFrontEndCampaigns::OnChoose(const unsigned id)
{
    if(layoutUnavailable_)
        return;
    if(id >= ID_FirstCampaign && id < ID_Previous && first_ + id - ID_FirstCampaign < catalog_.entries.size())
    {
        const auto folder = catalog_.entries[first_ + id - ID_FirstCampaign].folder;
        *selection_ = folder;
        Open([folder] { return std::make_unique<dskFrontEndMissions>(folder); });
    } else if(id == ID_Previous && first_ >= 6)
    {
        first_ -= 6;
        keyboardChoice_ = first_;
        FillPage();
    } else if(id == ID_Next && first_ + 6 < catalog_.entries.size())
    {
        first_ += 6;
        keyboardChoice_ = first_;
        FillPage();
    } else if(id == ID_Refresh)
        Refresh();
}

bool dskFrontEndCampaigns::Msg_KeyDown(const KeyEvent& ke)
{
    if(dskFrontEndPage::Msg_KeyDown(ke))
        return true;
    if(layoutUnavailable_ || catalog_.entries.empty())
        return false;
    if(ke.kt == KeyType::Return)
    {
        Msg_ButtonClick(ID_FirstCampaign + keyboardChoice_ - first_);
        return true;
    }
    if(ke.kt != KeyType::Left && ke.kt != KeyType::Right && ke.kt != KeyType::Up && ke.kt != KeyType::Down)
        return false;
    const int direction = ke.kt == KeyType::Left || ke.kt == KeyType::Up ? -1 : 1;
    const int step = (ke.kt == KeyType::Up || ke.kt == KeyType::Down) ? static_cast<int>(columns_) : 1;
    keyboardActive_ = true;
    keyboardChoice_ = static_cast<unsigned>(std::clamp(static_cast<int>(keyboardChoice_) + direction * step, 0,
                                                       static_cast<int>(catalog_.entries.size()) - 1));
    *selection_ = catalog_.entries[keyboardChoice_].folder;
    const unsigned first = keyboardChoice_ / 6 * 6;
    if(first_ != first)
    {
        first_ = first;
        FillPage();
    }
    return true;
}

void dskFrontEndCampaigns::Draw_()
{
    dskFrontEndPage::Draw_();
    for(unsigned i = 0; i < 6 && first_ + i < catalog_.entries.size(); ++i)
    {
        const auto* tile = GetCtrl<Window>(ID_FirstCampaign + i);
        if(!tile->IsVisible())
            continue;
        const auto& campaign = catalog_.entries[first_ + i].description;
        const auto rect = tile->GetDrawRect();
        if(keyboardActive_ && first_ + i == keyboardChoice_)
        {
            DrawRectangle(Rect(rect.getOrigin(), Extent(rect.getSize().x, 2)), COLOR_YELLOW);
            DrawRectangle(Rect(DrawPoint(rect.left, rect.bottom - 2), Extent(rect.getSize().x, 2)), COLOR_YELLOW);
            DrawRectangle(Rect(rect.getOrigin(), Extent(2, rect.getSize().y)), COLOR_YELLOW);
            DrawRectangle(Rect(DrawPoint(rect.right - 2, rect.top), Extent(2, rect.getSize().y)), COLOR_YELLOW);
        }
        const unsigned width = rect.getSize().x > 16 ? rect.getSize().x - 16 : 1;
        const int mid = (rect.left + rect.right) / 2;
        const unsigned lines = 3 * NormalFont->getHeight() + 12;
        const unsigned height = rect.getSize().y > lines + 16 ? rect.getSize().y - lines - 16 : 1;
        if(auto* image = dynamic_cast<glArchivItem_Bitmap*>(artwork_[i].get(0)))
        {
            const auto size = image->GetSize();
            if(size.x && size.y)
            {
                const double scale =
                  std::min(static_cast<double>(width) / size.x, static_cast<double>(height) / size.y);
                const Extent fit(std::max(1u, static_cast<unsigned>(size.x * scale)),
                                 std::max(1u, static_cast<unsigned>(size.y * scale)));
                image->DrawFull(Rect(DrawPoint(mid - static_cast<int>(fit.x / 2), rect.top + 8), fit));
            }
        } else
            NormalFont->Draw(DrawPoint(mid, rect.top + 8 + static_cast<int>(height / 2)), _("No preview available"),
                             FontStyle::CENTER | FontStyle::VCENTER, COLOR_YELLOW, static_cast<unsigned short>(width));
        int y = rect.bottom - static_cast<int>(lines);
        for(const auto& text :
            {campaign.name, campaign.shortDescription,
             helpers::format(_("%1% missions — %2%"), campaign.getNumMaps(), _(campaign.difficulty))})
        {
            NormalFont->Draw(DrawPoint(mid, y), text, FontStyle::CENTER, COLOR_YELLOW,
                             static_cast<unsigned short>(width));
            y += NormalFont->getHeight() + 2;
        }
    }
}
