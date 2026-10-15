// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "dskFrontEndPage.h"
#include "frontend/CampaignCatalog.h"
#include "gameTypes/ServerType.h"
#include "libsiedler2/Archiv.h"
#include <array>
#include <memory>

/// Six artwork cards at a time; the selection identity survives reconstruction through the page trail.
class dskFrontEndCampaigns : public dskFrontEndPage
{
public:
    explicit dskFrontEndCampaigns(std::shared_ptr<boost::filesystem::path> selection,
                                  ServerType type = ServerType::Local);
    const frontend::CampaignCatalog& GetCatalog() const { return catalog_; }
    unsigned GetFirstCampaign() const { return first_; }
    const glArchivItem_Bitmap* GetArtwork(unsigned tile) const;
    bool Msg_KeyDown(const KeyEvent& ke) override;
    enum ControlIds
    {
        ID_FirstCampaign = ID_FIRST_FREE,
        ID_Previous = ID_FirstCampaign + 6,
        ID_Next,
        ID_Refresh,
        ID_Status
    };

protected:
    void OnChoose(unsigned id) override;
    void OnLayout() override;
    void Draw_() override;

private:
    ServerType type_;
    void Refresh();
    void FillPage();
    frontend::CampaignCatalog catalog_;
    std::shared_ptr<boost::filesystem::path> selection_;
    unsigned first_ = 0;
    unsigned keyboardChoice_ = 0;
    unsigned columns_ = 1;
    bool keyboardActive_ = false;
    bool layoutUnavailable_ = false;
    // Loader cache ids use filename stems. Own archives keep equal-stem artwork from different campaigns independent.
    std::array<libsiedler2::Archiv, 6> artwork_;
};
