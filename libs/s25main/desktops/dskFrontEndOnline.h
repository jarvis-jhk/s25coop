// Copyright (C) 2026 s25coop contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "dskFrontEndPage.h"

class dskFrontEndOnline : public dskFrontEndPage
{
public:
    dskFrontEndOnline();
    enum ControlIds
    {
        ID_Join = ID_FIRST_FREE,
        ID_Lan,
        ID_Internet,
        ID_Host
    };

private:
    void OnChoose(unsigned id) override;
};
