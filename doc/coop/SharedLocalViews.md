<!--
Copyright (C) 2026 s25coop contributors

SPDX-License-Identifier: GPL-2.0-or-later
-->

# Several local views on ONE player (couch coop)

Status: design, 2026-09-30. ROADMAP M3 step c.

Goal: two to four people on one machine (Steam Deck on a TV, several controllers) play a
single-player campaign mission together as ONE tribe. Each gets their own splitscreen view and
gamepad; all their orders act for the same player.

## What the splitscreen merge gives us

derneuere's splitscreen (merged 2026-09-30) gives every local player a `PlayerView`, and extra
local players take over AI slots (Dummy AI, commands through `LocalPlayerGCFactory`). Input,
windows and pad seats are already keyed by **view index**:

- `WindowManager` owners, `IngameWindow::ownerIdx_`, `ViewScope`, action/road window owners
- `PadRouter` slot = view index, `ViewportLayout`, the mouse view

Game-level state is keyed by **player id**, which is exactly right for a shared tribe:

- commands: `OnWindowOwnerChanged` → `GameClient::SetWindowOwnerPlayer(view.GetPlayerId())`
  → `LocalPlayerCommands` (map player id → commands); the pad path uses
  `GetGCFactory(playerId)`. Two views on one id share one factory and one command stream.
- visual settings (`std::array<VisualSettings, MAX_PLAYERS>`), post box, notifications.

## What blocks it today

Only explicit guards that refuse a repeated player id:

- `dskGameInterface::CreateViews`: drops a repeated id (`!helpers::contains(playerIds, id)`).
- `GameClient::ValidateAdditionalLocalPlayers`: refuses the main id and duplicates.
- `GameClient::SetupLocalPlayers`: needs `ps == AI` and an id not yet local; a failure stops the
  game start with `ClientError::LocalPlayerSetup`.
- `GameClient::ApplyAdditionalLocalPlayers`: turns the slot into a Dummy AI "Local player N" —
  must never happen to the main (human) slot.
- `GameClient::RefreshAdditionalLocalPlayers` rebuilds the list from a map: duplicates vanish,
  and `CreateViews` reads that list.
- `QuickStartGame`: builds distinct slots.
- `dskGameLobby` seat panel: `LocalSeat` keyed by player id, dedup, `savedPs`/`savedAi` restore
  per slot (two seats on one slot would clobber each other).

## Plan

1. `GameClient`: separate **extra views** from **extra player slots**. A new list of view
   player ids may repeat ids and contain the main id. `SetupLocalPlayers` registers only the
   distinct ids that are not the main player (Dummy AI, as today); the rest reuse the existing
   factory. `CreateViews` reads the view list, without the dedup.
2. `ValidateAdditionalLocalPlayers`: count views against `MAX_VIEWPORTS`; allow the main id and
   repeats as views; `ApplyAdditionalLocalPlayers` skips them.
3. Command line first (`--local-players 2 --share-player` or similar) so a test and a hand test
   can drive it before any lobby UI.
4. Seat identity: the focus ring and brief stripe use `PLAYER_COLORS[view index]` (as the menu's
   ring does), because two views on one player would otherwise look identical.
5. Lobby: a seat can join "together with seat 1" (the host's tribe); no state save/restore for
   such seats. In a campaign lobby that is the ONLY kind of seat offered — a campaign's AI
   opponents must never become local players (Codex review of the merge).
6. Polish: `GameWorldViewer::RoadConstructionEnded` removes the preview of every view of that
   player (a visual glitch when two people build roads at once); refresh open economy windows
   when the shared visual settings change.

Done for 4 and the road half of 6 (2026-10-02): `dskGameInterface::SeatColor` gives a shared view
`PLAYER_COLORS[view index]` for its focus ring and brief stripe; unshared views keep their player's
colour. `OnRoadNote` cancels another view's preview of the same player when a constructed road
runs through it or puts its new end flag inside it (that view's build could only be refused, and
the note would have punched holes into its overlay). Tests: `tests/s25Main/splitscreen/testSharedViews.cpp`.
Seats are either all shared or all distinct today (lobby toggle, CLI), so a shared seat colour
cannot collide with an unshared local player's colour; revisit if mixed setups are ever allowed.
Open: refreshing a second view's open economy window when the shared settings change.

Tests: a Test_splitscreen case with two views on player 0 — both place a building, both land
in player 0's world, one command stream; a campaign mission with two shared views in the
headless harness if feasible.
