<!--
Copyright (C) 2026 s25coop contributors

SPDX-License-Identifier: GPL-2.0-or-later
-->

# Single-mission scenarios

A scenario is a registered campaign with exactly one mission. It appears in
Home → Maps & scenarios → Category → Scenarios, and in the Campaigns grid.
Choosing it opens its mission page with description, preview and saved progress;
Continue starts the normal local party flow. Every joined controller shares the
human tribe. Map tribe count includes AI opponents, not controllers.

Put a folder under the user campaign directory (`campaigns` inside the game's
user-data directory; normally `~/.s25rttr/campaigns` on Linux or
`~/Return To The Roots/campaigns` on Windows), with:

```text
MyScenario/
    campaign.lua
    mission.SWD
    mission.lua
```

`campaign.lua` uses the existing campaign format, for example:

```lua
function getRequiredLuaVersion() return 1 end
campaign = {
    uid = 'my-scenario-v1', version = '1',
    author = 'Scenario author', name = 'My scenario',
    shortDescription = 'A single cooperative mission',
    longDescription = 'Describe the starting situation and objective here.',
    maxHumanPlayers = 1, difficulty = 'easy',
    chaptersEnabled = {0}, maps = {'mission.SWD'}
}
```

Use a unique, stable UID. An adjacent Lua file on an ordinary map does not
register a scenario, create an identity or unlock a mission. The existing
campaign format supports one human tribe; separate human tribes and online
front-end hosting are separate roadmap work. Multi-mission campaigns stay in
Campaigns and are omitted from the Scenarios category.

The matching `mission.lua` uses the game's existing map Lua API. Mission
scripts own their goals and completion. For example:

```lua
function getRequiredLuaVersion() return 1 end
function onStart(firstStart)
    rttr:SetMissionGoal(0, 'Reach the objective together.')
end
-- In the actual victory condition, not simply at game start:
-- rttr:SetCampaignChapterCompleted('my-scenario-v1', 0)
-- rttr:SetCampaignCompleted('my-scenario-v1')
```

Browsing and starting never mark a mission complete. Available, Locked and
Completed use the same campaign settings as Campaigns; completed missions remain
playable. A campaign without a UID retains the existing all-playable behavior
and cannot track its own completion through an inferred filename identity.

Discovery validates campaign metadata, map headers and required regular script
files without executing mission scripts or loading map layers. The selected
preview checks layers; the mission page rechecks identity and progress at start.
If registration becomes a different campaign or gains more missions, Refresh is
required. Missing/corrupt files are isolated; Refresh recovers repairs. Back from
the mission page restores category, tribe filter, selected scenario and focus.
