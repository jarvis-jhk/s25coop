// Copyright (C) 2024 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "CampaignSettings.h"
#include "Settings.h"
#include "gameData/CampaignDescription.h"

void CampaignSettings::readSaveData(const CampaignID& campaignId, const std::string& saveString)
{
    int i = 0;
    for(auto c : saveString)
        setChapterStatus(campaignId, i++,
                         c == '1' ? ChapterStatus::Enabled :
                         c == '2' ? ChapterStatus::Completed :
                                    ChapterStatus::Disabled);
}

std::map<CampaignID, std::string> CampaignSettings::createSaveData() const
{
    std::map<CampaignID, std::string> result;
    for(const auto& state : states_)
        result[state.first] = toSaveString(state.second);
    return result;
}

bool CampaignSettings::isChapterPlayable(const CampaignDescription& campaignDesc, ChapterID chapter)
{
    // backwards compatibility - if campaign uid not specified, then all chapters are playable
    if(campaignDesc.uid.empty())
        return true;

    const auto& state = getCampaignState(campaignDesc);
    const auto it = state.find(chapter);
    return it != state.cend() && it->second != ChapterStatus::Disabled;
}

std::vector<MissionStatus> CampaignSettings::getMissionsStatus(const CampaignDescription& campaignDesc)
{
    const auto numMaps = campaignDesc.getNumMaps();
    std::vector<MissionStatus> result(numMaps);

    // backwards compatibility - if campaign uid not specified, then all chapters are playable
    if(campaignDesc.uid.empty())
    {
        for(auto& status : result)
            status.playable = true;
        return result;
    }

    auto& state = getCampaignState(campaignDesc);
    for(auto i = 0u; i < numMaps; ++i)
    {
        result[i].playable = state[i] != ChapterStatus::Disabled;
        result[i].conquered = state[i] == ChapterStatus::Completed;
    }
    return result;
}

bool CampaignSettings::shouldShowVictoryScreen() const
{
    return chapterCompleted_ || campaignCompleted_;
}

void CampaignSettings::enableChapter(CampaignID campaignUid, ChapterID chapter)
{
    if(states_[campaignUid][chapter] == ChapterStatus::Disabled)
        setChapterStatus(campaignUid, chapter, ChapterStatus::Enabled);
}

void CampaignSettings::setChapterCompleted(CampaignID campaignUid, ChapterID chapter)
{
    setChapterStatus(campaignUid, chapter, ChapterStatus::Completed);
    chapterCompleted_ = chapter;
}

void CampaignSettings::setCampaignCompleted(CampaignID campaignUid)
{
    campaignCompleted_ = campaignUid;
}

void CampaignSettings::resetCompletionStatus()
{
    chapterCompleted_ = {};
    campaignCompleted_ = {};
}

CampaignSettings::CampaignState& CampaignSettings::getCampaignState(const CampaignDescription& campaignDesc)
{
    // The default chapters are always playable, also when a script already changed the state of other chapters
    // before this campaign's description was seen (e.g. a mission continued from a savegame on a fresh profile)
    auto& state = states_[campaignDesc.uid];
    for(auto chapter : campaignDesc.chaptersEnabled)
    {
        if(state[chapter] == ChapterStatus::Disabled)
            state[chapter] = ChapterStatus::Enabled;
    }
    return state;
}

std::string CampaignSettings::toSaveString(const CampaignState& state) const
{
    if(state.empty())
        return "";

    std::string result;

    result.resize(state.crbegin()->first + 1, '0');
    for(const auto& status : state)
    {
        const auto ss = status.second;
        result[status.first] = ss == ChapterStatus::Enabled ? '1' : ss == ChapterStatus::Completed ? '2' : '0';
    }
    return result;
}

void CampaignSettings::setChapterStatus(CampaignID campaignUid, ChapterID chapter, ChapterStatus status)
{
    states_[campaignUid][chapter] = status;
}
