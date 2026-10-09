<!--
Copyright (C) 2026 s25coop contributors

SPDX-License-Identifier: GPL-2.0-or-later
-->

# Front end from scratch

Status: on the roadmap as the top priority, 2026-10-06. Jan: "be WAY more aggressive with the
menu overhaul! Basically do everything from starting the binary to being in game from scratch."
This replaces the slice order of [MenuRedesign](MenuRedesign.md); its ideas (player cards, rules
drawer, addon categories, profiles, living background, campaign hub) are slices of this plan.

## Goal

Four people on a sofa with a Deck on the TV: start the game, everyone presses A, pick a campaign
or a save, choose colours, start — without a mouse, without a floating window, without reading
upstream's developer-style dialogs. A player at a desk with mouse and keyboard gets the same
screens and can click everything.

## Principles

- Full-screen pages, no floating windows. One focus per page; D-pad/left stick move it, A
  activates, B goes back one page, LB/RB switch tabs, Menu opens the page's actions.
- Every page has the same frame: header (page title, joined-player strip), content, footer help
  line (button glyph + action, the same component as the in-game brief).
- Built from the game's own art (wood, stone, buttons, fonts from the original data), scaled for
  800×600 and the Deck's 1280×800 (first-run scale from PR #41).
- The logic stays where it is (GameLobby, GameClient, campaign and save loading); the new pages
  replace desktops, not game code. The old desktops stay reachable via Options → "Classic
  menus" until every route has parity (F12).
- Each slice is one PR with physical-input tests (pad and mouse), claims first, exact-head CI.

## Screen map

```text
binary → splash/intro (any button skips)
       → Title: "Press A" / click; each controller's A joins (P1–P4 strip)
       → Home
           Continue        → newest save → Party
           Campaigns       → campaign grid → mission picker → Party
           Maps & scenarios→ map browser (preview, player count) → Party
           Load game       → save browser → Party
           Play online     → Host (Campaigns / Maps / Load, open to network) → Party
                             Join (LAN list, direct IP, online lobby) → Party
           Options         → grouped settings
           What's new, Credits, Quit
Party (lobby, local and online in one): player cards, rules drawer, ready/start
       → Loading (title, tip, controller layout) → game
```

## Slices

F1 framework · F2 title/join · F3 home · F4 campaigns · F5 maps & scenarios · F6 load game ·
F7 party · F8 online · F9 options · F10 loading · F11 living background · F12 retire old
desktops. Sizes, owners and order: ROADMAP.md (M3 "Front end from scratch"). Pieces with no
dependency on F1 (on-screen keyboard, addon category table and tabs, profiles) can start at once.

## Framework (F1, as built 2026-10-07)

- `desktops/dskFrontEndPage` is the base of every new page: header (back button, title, joined-player
  strip in the slot colours), content as tiles (`UseTiles(aspect, maxTile)`) or a list
  (`UseList(rowHeight, maxWidth)`), items added with `AddItem(id, label)` in ascending id order, and a
  footer help line. Subclasses react in `OnChoose(id)`; mouse click and pad A both arrive there.
- Layout is in actual render units (`frontend/PageLayout`, pure and unit-tested): 800×600 and the Deck's
  1024×640 each get their own arrangement, tiles keep their aspect, content is capped at 1100 wide.
  A page also re-lays out when it becomes active, because a resize while it waited for its switch only
  reaches the old desktop.
- Back stack: `Open(factory)` pushes the current page's factory and chosen item into the new page's
  trail; B, Escape and the back button rebuild the previous page and focus that item again. A root page
  can handle B itself (`OnBackAtRoot` + `HasBackAction`). A second press in the frame of a queued switch
  is ignored (`WindowManager::IsSwitchPending`). Factories must capture values, never a page's `this`.
- Footer: `frontend/PageKeys` asks the same questions the press decides on (CanActivate, PeekStep) and
  draws with the in-game brief's badges (`brief::DrawKeyRuns`); only shown while a controller is in hand.
- Upstream desktops opened from a page return through `frontend/MenuRoutes` (`MainMenu()`,
  `SinglePlayerMenu()`): to the home page when it was shown last, to `dskMainMenu` when "Classic menus"
  was.
- Title (F2, `dskTitle`): every controller's first press there gives it a slot (the page offers
  `MenuPadInput::MaxSlots`) and that press is its join - so "everybody presses A once" is literal. The
  joined controllers are the `Party` (input/Party, kept by MenuPadInput, unplugging leaves it); the player
  strip on every page shows the party. A/Start by a member or a click on Start opens Home; B leaves the
  party; B on Home returns to the title. The F7 prerequisite now seats that party in a local lobby
  without another A press, in join order.
  Campaigns and loaded saves share the human tribe; ordinary maps use open individual tribes. If too
  few tribes are open, the lobby asks for "Play one tribe together"; choosing it seats the pending
  party. Closed or network-occupied tribes are never reopened. Classic menus keep their old join
  behavior. This is the existing lobby with party carry-over, not the full-screen F7 page yet.
- Home (F3) does not list Play replay, Readme and Intro yet; they stay reachable through "Classic menus"
  (the old main menu has "New menus" to come back). F12 must not retire the old menu before they have a place.
- Tests: `tests/s25Main/UI/testFrontEndLayout.cpp`, `tests/s25Main/splitscreen/testFrontEndPage.cpp`
  (`TestPage` there is the minimal example of a page), `testFrontEndHome.cpp`.

## Loading saves with more players

A save is loaded into the Party page like any other start. Its human tribe can be played by
every joined player together (the shared-views mode from M3c); a save from an ordinary
single-player game is no exception (Jan, 2026-10-06). The saved AI tribes stay AIs unless a
player takes one over explicitly.

## Not now

Pixel-art Xbox button glyphs and the full/buttons-only/off help footer are on the roadmap but
wait for F1's shared footer (Jan, 2026-10-06: "Do neither of them now").

## Save browser (F6a)

Home → Load game opens `dskFrontEndLoad`. It uses the full-screen page frame and
returns to the selected Home tile with one Back / B / Escape. The scrollable list
starts at the newest save (full timestamp, path tie-break), with map, date, game
time, saved human/AI tribe counts and names alongside. D-pad browses the list;
A, Enter or double-click loads, and Load/Refresh are also clickable. Refresh
keeps the selected file when it still exists, then falls back to the newest.
Continue selects and starts its exact newest-save target on page activation;
a deleted or incompatible target shows an error without starting another save.

The catalog reads headers and settings only. It never loads a world for preview,
changes the RNG or edits the save; malformed metadata is counted as unavailable.
The current save format has no thumbnail: “No preview stored” is an explicit
fallback, with actual snapshot thumbnails deferred to F6b. Failure stays on this
page with an acknowledgement. A second activation in the same input batch
cannot start another connection over the existing connecting modal.

The existing client/server and connecting window lead to the current lobby;
F7's full-screen replacement is separate. The title party is carried into local
saved games through the existing shared-view bridge. Physical regressions load
a current solo world re-encoded with upstream 4.0 player metadata (no portrait/start-goods
fields or local seats) with two and four controllers, preserve AI/locked tribes and
resume the simulation. The loading screen is the test's sole bypass because
original S2 rendering resources are absent. This does not establish online
co-player resume, AI takeover or Steam Deck hardware acceptance. Classic Load
keeps its original dialog until F12.

Actual pre-coop upstream-produced world/save compatibility still needs a
provenance-backed fixture in F6b; rewritten player metadata does not establish
that compatibility.
