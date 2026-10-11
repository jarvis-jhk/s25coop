// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "dskFrontEndPage.h"
#include "network/ClientInterface.h"
#include "gameData/GameLoader.h"
#include "liblobby/LobbyInterface.h"
#include <memory>

class dskGameInterface;

class dskGameLoader : public dskFrontEndPage, public ClientInterface, public LobbyInterface
{
public:
    explicit dskGameLoader(std::shared_ptr<Game> game);
    ~dskGameLoader() override;

    void LC_Status_Error(const std::string& error) override;
    void CI_GameStarted() override;
    void CI_Error(ClientError ce) override;

    // During loading, the game owns the pending device events. Only the error modal takes menu input.
    bool WantsPadInput() const override { return failed_; }

    enum LoadingIds
    {
        ID_MapTitle = 10,
        ID_Status = 11,
        ID_Timer = 20,
        ID_Tip = 21,
        ID_Controls = 22
    };

protected:
    void OnLayout() override;
    std::vector<brief::KeyHint> FooterKeys(const FocusPath&) const override { return {}; }
    // Keep original-resource loading separate from the client/server synchronization and view creation.
    virtual bool LoadTextures();
    virtual std::unique_ptr<dskGameInterface> CreateGameInterface();

private:
    void Msg_MsgBoxResult(unsigned msgbox_id, MsgboxResult mbr) override;
    void Msg_Timer(unsigned ctrl_id) override;
    void ShowErrorMsg(const std::string& error);
    void AdvanceLoading();
    void SetStatus(const std::string& text);

    const bool frontEnd_;
    bool failed_ = false;
    unsigned position = 0;
    GameLoader loader_;
    std::unique_ptr<dskGameInterface> gameInterface;
};
