<!--
Copyright (C) 2026 s25coop contributors

SPDX-License-Identifier: GPL-2.0-or-later
-->

# Pre-game menu redesign for controllers

Status: superseded in its ORDER by [FrontEnd](FrontEnd.md) (2026-10-06); the ideas below are
slices of that plan. Originally accepted 2026-10-03 (Jan: "Pack es so
auf die Roadmap"). Builds on concept B (campaign hub). Player cards come first.
Keep mouse/keyboard working in every slice. The bounded slices are listed
under "Roadmap slices" below; ROADMAP.md (M3) points here.

## Requirements from Jan

- A live "shell map" behind the main menu with ambient sound and music,
  like OpenTTD's title game or Factorio's menu simulations.
- Players choose their own settings (colour, nation, team) comfortably by
  controller, like other modern games.
- Someone can change the game rules, including addons, without blocking the
  others from changing their own settings.
- Addon GUI grouped into understandable categories.

## Proposal

1. Lobby player cards, one per local controller / remote player. D-pad
   up/down picks a row on the own card, left/right cycles the value. Colour is
   a swatch strip skipping taken colours; in a shared tribe every seat also
   gets a cursor colour. Locked campaign values show a lock and the reason.
   Builds on the existing local seats in `dskGameLobby`.
2. Rules drawer: opens over the editing player's card only. Edits are staged
   and applied once. Today every `GameMessage_GGSChange` cancels the countdown
   and `dskGameLobby` un-readies the local player on each change, i.e. on every
   single controller step. Instead: one change per apply; ready players keep
   their own choices and confirm the new rules with Y. Online: host edits,
   others read and may send a suggestion. Splitscreen: any seat may open it,
   one at a time, with a visible "X is editing the rules".
3. Addon categories by player effect instead of the technical
   `AddonGroup` bits: Comfort (QoL), New content, World & economy, Combat,
   Easier ("cheats", marked in saves and victory), Developer (hidden).
   Value-dependent addons mark easier/harder by value. Presets: Classic,
   Comfort (recommended), Relaxed, Challenge, Custom; "changed only" filter.
   UI only, no protocol change.
4. Profiles (name, colour, nation, controller mapping) and a controller
   on-screen keyboard.
5. Shell map: first decouple it from the `GAMECLIENT` singleton the lobby
   uses (or stop it cleanly on lobby entry); then play a recorded AI replay
   with a camera script (deterministic, cheap, reviewable) rather than a live
   AI game; then sound/camera; then one scene per nation/campaign. Deck: 30 fps
   cap in the menu, option live/calm/still, still image on low battery and
   without original data.
6. Campaign hub (concept B) as the new start page on top.

## Roadmap slices (2026-10-03)

Order as approved by Jan. Each slice is one PR: claim its scopes first, prove it
with physical-input Debug tests (mouse/keyboard unchanged) and all exact-head CI
green before integration. A later slice may start once the earlier one it builds
on is integrated.

Player cards

- 1a Pure card model per seat: rows (colour, nation, team, shared tribe), value
  cycling, taken-colour skipping, campaign locks with reason. Unit tests only.
- 1b Cards in `dskGameLobby` for local seats: D-pad row/value, A/B, focus per
  controller; replaces the per-seat controls for controller users.
- 1c Remote players' cards (read-only) and per-seat cursor colours in a shared
  tribe; online and splitscreen regression with real loopback server. The lobby
  retains its editable rows and chat: a Player cards button opens a live,
  read-only roster with four cards per page, including each co-player. Local
  shared seats preview the colours of their actual compacted game-view order;
  a departing seat updates the remaining previews. These are view/cursor colours,
  independent of the shared tribe colour, not a new saved or network setting.

Rules drawer

- 2a Staged rule edits: one `GameMessage_GGSChange` per apply instead of per
  step; ready players keep their own choices and re-confirm new rules with Y.
  Countdown/ready regressions.
- 2b Drawer UI over the editing player's card; one editor at a time with a
  visible "X is editing the rules"; others keep editing their cards.
- 2c Online: non-hosts read the rules and may send a suggestion to the host.

Addon categories

- 3a Category table (Comfort, New content, World & economy, Combat, Easier,
  Developer) with a test that every addon has exactly one category and
  value-dependent easier/harder marks.
- 3b Category tabs plus "changed only" filter in the addon window. UI only.
- 3c Presets Classic / Comfort (recommended) / Relaxed / Challenge / Custom;
  "Easier" marked in saves and the victory screen.

Profiles

- 4a Profiles (name, colour, nation, controller mapping) stored and picked on
  the card.
- 4b Controller on-screen keyboard for names and chat.

Shell map

- 5a Decouple the menu world from the lobby's `GAMECLIENT` (or stop it cleanly
  on lobby entry); tests that lobby/game start are unaffected.
- 5b Play a recorded AI replay with a camera script behind the main menu.
- 5c Ambient sound and music; Deck: 30 fps cap, live/calm/still option, still
  image on low battery or without original data.
- 5d One scene per nation/campaign.

Campaign hub

- 6 Campaign hub (concept B) as the new start page on top of the above.
