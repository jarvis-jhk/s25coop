<!--
Copyright (C) 2026 s25coop contributors

SPDX-License-Identifier: GPL-2.0-or-later
-->

# Pre-game menu redesign for controllers

Status: proposal, 2026-10-03 (Jan's feedback on the menu concepts report).
Builds on concept B (campaign hub). Not yet ROADMAP slices; split each step
into bounded slices before implementing, and keep mouse/keyboard working.

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
