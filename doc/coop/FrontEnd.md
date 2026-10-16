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
- Seats (F7a): `frontend/LocalSeats` holds who sits where in a self-hosted local game and writes it into the
  game state (additional local players or shared views, slot states, pad view slots). It shows nothing;
  dskGameLobby draws its cards from it, and the F7b party page does the same.
- Party (F7b, `dskParty`, 2026-10-12): a campaign or scenario started from the new menus opens this page
  instead of the classic lobby (`dskParty::IsFor`, decided in iwConnecting). One big card per seat (four,
  every seat plays the campaign's tribe), "Start game" below them. The title party is seated on arrival;
  a further controller presses A on a free card to join and B to stand up again. Only the host seat's
  controller (slot 0) can start (Start or the button) or leave (B/Escape/back, with a Yes/No question
  focused on No, then Home); the host's card edits colour/nation/team through the server, guests' cards
  are read-only ("Shared tribe: host chooses") and their D-pad never leaves the cards. Lua runs as in the
  lobby: settings init on construction, settings-ready on activation, player-ready on Start. Saves loaded from
  the new menus open it too (2026-10-14): every card is locked ("Saved game"; the server would otherwise
  accept colour/nation/team changes on a save), the party shares the host's tribe, and the slots nobody here
  can take get a Default AI - a further local player's own slot (idle AI "Local player N",
  `LocalSeats::FormerLocalPlayers`) and a past network player's free slot, which the server refuses to start
  with. Saved AIs stay as they are.
  Ordinary maps (F7c, 2026-10-16) open it too (`IsFor` = every self-hosted local start from the new menus). Each
  joined controller gets a tribe of its own (LocalSeats own-slot mode, the guest edits its own card); if the
  party is bigger than the map's free tribes it plays one tribe together without asking. The host switches the
  mode on its card's Tribes row or in the rules: everybody stands up in the old mode and the same controllers sit
  down again in the new one; a switch to own tribes that would leave a player out is refused with a message.
  "Rules" (left of Start) opens a drawer in the page: Tribes, goals, goods at start, exploration, speed, computer
  players, lock teams, shared team view, random start locations, and the classic addon window. D-pad up/down
  walks the rows, left/right or A/click changes the value; B, Escape, the header's Back and Done close it. The
  host keeps its own copy of the rules and sends it whole; the server's echo is adopted only once every send has
  come back, so an older echo cannot undo a newer edit. Computer players (easy/medium/hard/none) are applied at
  Start to the ordinary AI slots nobody sits on, so a free tribe stays joinable until then. Start asks the
  classic lobby's economy/peaceful questions. Lua permissions as in the lobby (`general`, `playerState`,
  `addonsAll`/`addonsSome`, own*/ai* card rows). Online games keep dskGameLobby until F7d.
  Tests: `tests/s25Main/splitscreen/testFrontEndParty.cpp`, saves in `testFrontEndLoad.cpp`, maps in
  `testFrontEndMaps.cpp`.
- Home (F3) does not list Play replay, Readme and Intro yet; they stay reachable through "Classic menus"
  (the old main menu has "New menus" to come back). F12 must not retire the old menu before they have a place.
- Tests: `tests/s25Main/UI/testFrontEndLayout.cpp`, `tests/s25Main/splitscreen/testFrontEndPage.cpp`
  (`TestPage` there is the minimal example of a page), `testFrontEndHome.cpp`.

## Loading saves with more players

A save is loaded into the Party page like any other start. Its human tribe can be played by
every joined player together (the shared-views mode from M3c); a save from an ordinary
single-player game is no exception (Jan, 2026-10-06). The saved AI tribes stay AIs unless a
player takes one over explicitly.

## Campaigns (F4)

Home → Campaigns opens an artwork grid, six campaigns per page. Built-in and user
campaign directories share one catalog, ordered by name then directory. Every mission
needs a supported map header and an accompanying regular Lua file; the catalog loads
the campaign description through the existing Lua interface but never executes mission
scripts or decodes their map layers. Broken campaigns stay isolated and are counted.
Missing optional pictures use a text fallback. The original continent background is used
when supplied; pictures are owned per card so equal filenames in different campaigns
cannot overwrite each other. Keyboard arrows, Return, controller focus/A and mouse
buttons all reach the same mission page. Back reconstructs the selected campaign page
and restores its controller focus; Refresh retains that directory when it still exists.

The mission page lists every chapter with its saved Available / Locked / Completed
state, selected-map minimap and campaign description. Completed chapters remain playable;
campaigns without a UID retain the existing all-chapters-playable policy. Locked rows
can be browsed but cannot start. Pad A, Return, double-click and Continue recheck current
progress, the selected map's full layers and its exact map/script identity against a
fresh campaign description. Reordered/replaced/deleted campaign files require Refresh
rather than silently starting another mission. Missing layers/scripts stay on the page
with an error; Refresh can recover repairs. Browsing and starting do not mark a chapter
completed: the mission script and existing campaign settings remain authoritative.

The real local host/connecting/lobby path carries the Title party into shared views of
the campaign's human tribe. Classic and network campaign choosers are unchanged. The
new full-screen F7 party route and F8 network adoption remain primary-owned/separate;
masked continent interaction and original mission-presentation parity belong to M1.
Physical Debug regressions use public test maps and generated campaign metadata. They
verify the selected Lua script's actual mission goal and synchronized two/four-controller
games, while bypassing only the original-resource loader. Packaged Deck acceptance and
original S2 artwork remain distinct from this fixture evidence.

## Not now

Pixel-art Xbox button glyphs and the full/buttons-only/off help footer are on the roadmap but
wait for F1's shared footer (Jan, 2026-10-06: "Do neither of them now").

## Grouped options (F9)

Home → Options opens `dskFrontEndOptions`, with Display / Steam Deck, Sound/Music,
Controls and Language pages. They use the same trail as other front-end pages: Back
returns to the chosen category, then to Home's Options tile. Labels sit above their
controls, so translations and values have separate space at 800×600 and 1280×800.

Choices keep the shared dropdown contract: A opens, D-pad previews, A accepts and
persists, B cancels before leaving the page. Audio sliders preserve the stored byte
volume without rounding it on entry. Accepted display modes and sizes take effect
through the video driver; a rejected mode keeps the working mode and shows an error.
TV/Deck automatic profiles reset an existing fixed scale on enable; turning a profile
off keeps a subsequently chosen fixed percentage. Language changes retain the trail;
returning through it rebuilds the translated ancestors. Resizes cancel display-list
previews and rebuild current size/scale choices; a fixed scale that no longer fits is
reduced and persisted. Scrollable dropdown lists stay above the shared footer.

Advanced settings keeps the upstream page for driver selection, ports/proxies,
portraits, names and addon defaults. Classic menus opens the old main menu. Neither
route is retired in this slice. On-screen keyboard caller adoption and the deferred
help-footer toggle are separate work. Tests are physical input through the mock driver
in `testFrontEndOptions.cpp`; a one-shot mock resize refusal covers recovery, and test
cleanup restores temporary settings and driver state before fixture destruction.

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
New saves include a 160×100 tactical snapshot of the saving client's visible
map: terrain, territory, buildings and roads, with unexplored ground hidden and
remembered fog darkened just like the in-game minimap. It is captured on the CPU
at the saved frame, without a desktop, OpenGL or simulation RNG changes. The
minimap's existing fog policy remembers objects, ownership and roads; ground
terrain/shading retains the same current-world representation as the live minimap.
The selected row alone reads the image; the catalog still only reads metadata.
Old saves and damaged/unknown previews show “No preview stored”. Failure stays on
this page with an acknowledgement. A second activation in the same input batch
cannot start another connection over the existing connecting modal.

The optional image is a fixed 64,016-byte trailer following the existing saved
world, not a new world/player version or a sidecar. Its `S25PRV01` signature,
little-endian 160×100 dimensions, BGRA pixels and FNV-1a integrity check are read
only at the exact world-data boundary. Compressed and legacy uncompressed
payloads are skipped without decompression; malformed lengths, unknown dimensions,
truncated pixels or extra trailing bytes reject the preview. Existing save
readers stop after the world and still load its unchanged bytes. Copying a save
keeps the preview, and overwriting it truncates the previous image. A preview
write failure leaves the successful saved world usable. No loading of old worlds
to invent thumbnails, scene screenshots, original artwork or hardware proof is
implied by this tactical snapshot.

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

## Maps catalog (F5a)

`frontend/MapCatalog` supplies metadata independently of the full-screen Home route.
`ReadMapEntry` reads the SWD/WLD header, converts name/author to UTF-8 and checks
supported nonzero dimensions and tribe count. It annotates an adjacent regular
`.lua` file without loading or executing Lua. This marker does not imply campaign
membership, mission unlocks or a one-mission campaign start policy.

`ScanMaps` takes explicit source folders so the future browser can combine the
existing nine categories and the optional installation WORLDS folder. Discovery
is nonrecursive, accepts case-insensitive SWD/WLD suffixes, reads lexically
identical file paths once, and sorts by displayed name then path. An optional
player filter is an exact map-tribe count; shared controllers are not additional
tribes. Missing folders are empty, and bad headers or unusable folders carry a
path and reason while other sources remain visible. Filtered maps are not errors.
Symlink aliases are not canonicalized into a new identity.

Classic map selection uses the same metadata reader and keeps its category,
error-message, mouse/pad and lazy-preview flow. Header-only browsing is not proof
that map layers are complete: selection still fully loads and validates the map,
then removes a failed target while leaving the other maps available. No map or
save/network format changes. F5b supplies the full-screen controls, Home route
and real party start; F5c defines how scenarios become one-mission campaigns.

## Maps browser (F5b)

Home → Maps & scenarios opens `dskFrontEndMaps` in the common full-screen frame.
Back / B / Escape returns to the Maps tile. A scrollable list combines Classic's
nine categories plus the optional installation WORLDS folder. Category and exact
map-tribe-count filters are independent of how many controllers joined on Title.
Refresh keeps the selected file identity if it still matches, otherwise selects
the first map in name/path order. Table headers do not silently reorder selection.

Only the selected map loads its full layers for a minimap preview. Name, author,
tribe count, dimensions and adjacent-script presence are shown alongside it.
Broken headers are counted without hiding other maps. A header-readable map with
bad layers stays in the list with an unavailable-preview message and disabled
Continue; repairing it and refreshing restores the same selection. Browsing never
executes a map script. A script marker does not register a one-mission campaign;
that policy and campaign unlocks remain F5c.

D-pad and keyboard arrows browse rows. A, Enter, double-click and Continue use the
same selected-file start. Dropdowns stage browsing until acceptance: pad A,
keyboard Enter or a mouse selection commits, while B / Escape cancels before
leaving the page. Mixed mouse/pad/keyboard interaction uses those same controls.
Resize cancels an open filter, repositions its scrollable list above the footer,
and preserves the committed filters and map selection. At render sizes too small
for the content, the page keeps Back available and asks to enlarge the window or
reduce the UI scale, rather than creating overflowing controls.

Continue revalidates the file and full layers before hosting. A stale or failed
selection shows an error on this page; an existing connecting modal blocks another
start. The existing local client/server and connecting window lead to the current
lobby. Its joined-party bridge handles individual tribes or an explicit shared
tribe choice when the map has fewer tribes than controllers. The full-screen F7
party page, random-map generation and scenario registration remain separate work;
Classic's original picker is still reachable through Classic menus.

Physical regressions live in `testFrontEndMaps.cpp`: categories, tribe filters,
long lists, lazy/error previews, repaired and replaced files, mixed input, resize,
Home return and real local-game start with two and four joined controllers. The
four-controller case uses a three-tribe map and explicitly chooses one tribe
together in the existing lobby. Only original-resource loading is bypassed in the
start tests. These are mock-driver tests, not Steam Deck hardware acceptance.

## Scenarios (F5c)

Maps & scenarios has a Scenarios category for explicitly registered one-mission
campaigns from the built-in and user campaign directories. It does not infer
campaigns from adjacent map scripts. Discovery, ordering, file validation and
saved progress reuse F4; the tribe-count filter describes map tribes, not shared
controllers. The selected scenario shows its name, author, short description,
preview and Available / Locked / Completed state.

A, Return, double-click or Continue opens the existing mission page with its
full description and start policy. The browser first rechecks that registration
still has one mission and retains the selected UID/map/script identity; changed
registrations require Refresh. Mission start rechecks progress and layers, then
uses the real local host/connecting/lobby and shared Title party. No new save or
network format, synthetic UID, unlock policy or automatic completion is introduced.
Back reconstructs the same category, tribe filter and selected campaign folder.
Ordinary map categories and Classic remain available. A directory missing required
files is isolated and can be repaired with Refresh.

[Scenario packaging and script responsibilities](Scenarios.md). The current
campaign schema supports one human tribe. Multi-human scenarios, F7's party page,
F8 online front-end hosting, packaged S2 rendering and hardware acceptance are
separate work. Physical tests cover two/four joined controllers and their real
mission goal and gameframes, bypassing only original-resource loading.

## Loading (F10)

`dskGameLoader` uses the shared full-screen frame after Home, with the exact
client map title, joined-controller strip, current loading stage, an economy tip
and a short in-game controller legend. The layout uses render units, wraps the
tip/legend and keeps them within the content area at 800×600 and 1280×800.
Insufficient content space hides those blocks; resizing back restores them.
Classic keeps its existing random artwork and accumulating status messages.

The real timed loading sequence is retained: prepare tribes, load game resources,
create the actual game views and their postboxes, then call `GameLoaded` and wait
for the server's start. Mission `onStart` can therefore deliver its initial goal
before the first simulation frames. Waiting is a genuine client/server state,
not an invented percentage or another confirmation screen.

While loading, the desktop takes no controller input, preserving queued device
events for the game. Escape, Enter and clicks cannot leave the loading handshake.
A resource, view or connection failure stops the timer, keeps the failure status
and opens one acknowledgement modal. The controller can confirm it with A; mouse
confirmation also works. Reentrant failures cannot restart the timer or continue
into `GameLoaded`. Return keeps the existing local/lobby/direct-IP policy.

`testFrontEndLoading.cpp` exercises the actual timer and local server/client with
two and four shared views, mission-goal delivery and first gameframes. Its sole
bypass is original S2 resource loading/rendering. These tests do not establish
packaged graphics, native hardware or Steam Deck acceptance. F7 party integration,
F5c scenario registration remain separate work; F4 campaigns are integrated.
