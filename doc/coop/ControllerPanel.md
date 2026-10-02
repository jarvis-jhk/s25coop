<!--
Copyright (C) 2026 s25coop contributors

SPDX-License-Identifier: GPL-2.0-or-later
-->

# Controller side panel

Status: proposed design, 2026-10-02. ROADMAP M3, Deck feedback. This branch
contains design only. Push this document and the bounded ROADMAP slices before
implementing any panel code. Opus owns integration; each implementation slice
needs fresh file claims, runtime proof and its own reviewed PR.

## Player experience

Back opens a fixed panel on the right of the controller's own view. The world
remains visible on the left. LB/RB switch between Buildings, Stock, Statistics,
Post and Settings. A selects or confirms; B cancels the innermost interaction
and then returns toward the world. The panel displays useful counts, history
and messages directly rather than opening a stack of floating windows.

Mouse and keyboard still open and operate the existing movable windows. Merely
connecting a controller does not change those windows or activate the panel.
World building and road actions keep their existing input until a panel has
focus. Opening a panel does not pause the game or send a game command.

The first release of the shell remains opt-in through a controller UI option;
the existing system/radial menu stays the default until the implemented pages
cover its actions. Once equivalent behavior is verified, controller Back may
use the panel by default. Keep an explicit legacy option for rollback. This
is a migration plan, not permission to ship inaccessible placeholder actions.

## Existing code constraints

- `desktops/PlayerView.h` owns each view's focus, ring and world presentation;
  view index identifies the seat. A shared tribe can have several view indices
  with one player id. Panel visibility, selected tab, scroll and navigation
  history belong to the view, while game data and commands belong to the player.
- `dskGameInterface::OnPadButton` already routes ring input before ordinary
  focus, and handles Back/Y specially. Do not append another unconditional
  shoulder handler: world LB undoes road steps, RB opens world actions, rings
  use both shoulders for pages and focused windows use them for navigation.
- `iwPadSystemMenu` and `iwMainMenu` expose the current menu destinations.
  Reuse their semantic actions and authorization, not mouse clicks at guessed
  coordinates or a second command implementation.
- `WindowManager` owns floating windows by type and view owner. Existing
  `IngameWindow` layout can clamp against the full render area rather than one
  viewport. Positioning those windows at the right edge alone is insufficient
  to implement a contained, independently owned panel.
- `world/ViewportLayout.cpp` yields two side-by-side cells at landscape sizes;
  three views have two cells above a full-width lower cell, four have a 2x2
  grid. Layout must use each actual cell, not assume equal four-way cells.

The design does not change network protocol, shared-player policy, save format,
world checksums or singleton lifecycle. Existing ntfy fault reporting stays in
place; do not embed private report endpoints in game sources or public docs.

## Layout and information density

All sizes below are GUI view units after existing GUI scaling, not physical
screen pixels. Convert through the established scale once. Let W/H be the
owner's viewport size. Use an 8-unit inset, a 32-unit tab header, a 32-unit
context hint footer and the remaining height for scrollable content. Separate
rows by 4 units; interactive rows are at least 28 units high. Measure translated
text, wrap detail labels and scroll rather than shrink fonts.

At a 1280x800 single-view render area the panel is 448 units wide, at
(824,8), with height 784. Content has 432 units of width inside 8-unit margins.
The world region ends at x=816, leaving an 8-unit gap. Stock presents four
columns where measured icons/counts fit; the selected item has its name and
full count in the detail area. Buildings show name, count and productivity in
rows. Statistics uses the available width for a graph and scrollable legend.
Post shows a list with the selected letter's text in the same page. Settings
shows grouped named actions and the selected control's current value.

For viewports with W>=960 and H>=480, use width clamp(W*0.35, 336, 480).
Reserve at least 480 units of world width including the gap/insets. The 800x600
single-view layout is the explicit reduced-width side-panel tier: for
800<=W<960 and H>=480, use width 320, leaving at least 456 world units.
At 800x600 the panel rectangle is (472,8,320,584) and the world region ends
at x=464. For W<800 or H<480, use a compact overlay with width min(320,W-16), height H-16; it
covers only that owner's view and clearly indicates that the world is behind
it. This avoids shrinking a four-way 640x400 view into an unreadable world
strip. Compact pages use a single column or two count columns, with list/detail
as navigable subpages; every action remains reachable through scrolling.

Tab labels scroll horizontally to keep the selected one fully visible, with
previous/next hints at their ends. Do not force all five translated labels to
fit in 304 units. Graphs remain readable with fewer tick labels and a selected
value detail, not smaller fonts. At extreme supported GUI scales, clamp the
outer rectangle, allow scrolling and show text fallbacks for missing icons.

Panel bounds, content clip, tabs and footer must stay inside the owner's
viewport at 800x600, 1280x720, 1280x800 and 1920x1080, with 1-4 views and
existing supported scale settings. A three-view bottom panel may use its larger
cell. A live resize recalculates geometry, preserves selected objects by stable
identity where still present, and clamps focus/scroll to visible valid controls.

## Input precedence and focus

Exactly one controller interaction owns an event per view. Apply this order:

1. Owner modal, or a global modal that applies to everyone.
2. Active panel child interaction: expanded dropdown, confirmation, text edit
   or detail subpage. It receives B and directional input before the shell.
3. Active panel shell and its current page.
4. Existing ring, legacy focused window and world routing.

Opening is allowed only from the world or a compatible legacy menu with no
pending interaction. During a road preview or action/build ring, Back keeps
its current behavior and does not transfer hidden work into a panel. These
states must be unwound explicitly before entry. When opening, close the
view's legacy system-menu ring through its existing release path, clear its
focus, then establish panel focus. Do not leave two owners of the same input.

While the shell is active, LB/RB cycle tabs with wrap. While a dropdown/edit/
confirmation or detail subpage is active, those buttons are consumed there;
they never navigate the shell or reach a world/ring action. A detail subpage
keeps its navigation history on shoulder input; B returns to the parent page
before tab navigation becomes available again. Test both shoulders in a detail
subpage, asserting unchanged tab/history and no world action. A dropdown follows open/browse/commit/
cancel semantics: focus movement cannot change a game setting. Within pages
use D-pad or left stick to navigate and A to activate. Multi-page content uses
visible page controls or directional navigation rather than reusing shoulders.

B first cancels uncommitted input, then returns from detail, then closes the
panel. Back requests panel close through the same cancellation/dirty-state
policy; it cannot bypass a confirmation. Tab switches retain selections but
cannot silently commit/discard pending settings. Use the existing page's
Apply/Cancel semantics, and request a keep/discard decision when necessary.
Y must not move focus into a floating window behind an active panel. Start
retains existing controller assignment behavior; the panel does not introduce
a global pause shortcut. Right stick/trigger world motion is suppressed while
the panel or its modal owns input so browsing cannot pan/zoom the hidden map.

A world-position action, such as a letter's location or selecting a building,
closes or suspends the panel and centers only that view's camera using the
existing action. Reopening restores that tab and selection. Help is a child
page or owner modal with B returning to the same panel state. Destructive
commands require the existing explicit confirmation; holding a button must
not repeat a delete/confirm edge.

## Mixed devices, ownership and lifetime

A panel is controller-owned. Mouse movement alone leaves it open and cannot
change its tab. A deliberate mouse/keyboard action targeting this same view
requests an exit through the ordinary dirty-state policy before the action is
processed; no click falls through an unresolved modal. The initiating action
is consumed on a blocked/confirmed transition, and the UI shows a clear way to
cancel. Input in another view leaves the panel intact. Classic mouse menus
must retain their existing geometry, drag, focus and command behavior.

One view owns at most one panel. A panel's child confirmation/Help cannot steal
another view's focus. Shared views read the same player's actual state but
keep independent tabs/selections. Simultaneous commands still pass through the
owner-to-player command factory and existing network authority checks. Never
cache a `GamePlayer*` or a selected object pointer past its documented lifetime;
resolve stable ids and clear selection if an object/message disappears.

On pad disconnect, release panel focus and controller capture. Cancel purely
local uncommitted edits; already queued commands remain queued. Do not leave a
modal that blocks recovery with the mouse. Reconnect enters from the world,
with the last tab remembered for this game session. Desktop destruction clears
all panel/focus references before destroying game/view state. Replays use the
same browsing pages but disable editing at the callback boundary, including
wheel/bar/slider paths; read-only browsing must not set a false dirty flag.

## Page contracts and migration order

Stock is the first working page: actual wares/people counts, nation shield
mapping, addon visibility and Help. It sends no command. Buildings then shows
counts/productivity and selects the actual first matching building through the
existing per-view navigation action; empty categories are inert. Statistics
adds general and merchandise history, time/player selection and no world edits.
Post preserves current filter/selection through incoming mail, deletion and
same-count replacement, with owned snapshots for destructive tests. Settings
is deliberately split by policy: view-only map display first, then distribution,
transport/tools, military and build order in separate PRs. Save/load/leave,
diplomacy, ship navigation, economic progress and campaign diary remain reachable
via labeled legacy destinations until each has an implemented panel adapter.

Do not create hidden offscreen windows as panel state or call their message
handlers directly. Extract a shared page model/action only when the same
policy can be used by both presentations; acquire its existing window scopes
before that change. The shell owns clipping, tabs and focus, pages own local
navigation, existing game logic owns commands. Refresh reads live player data
and invalidates another shared view's displayed state without rebroadcasting
commands or overwriting that view's pending edits. A semantic change requires
both mouse-window and controller-page regressions.

## Bounded delivery slices

These are ordered dependencies, not permission to edit all listed files at
once. Exact scopes must be claimed after inspecting current master/Opus work.
No panel implementation starts until this design and ROADMAP are pushed.

1. Pure geometry and navigation model: new panel layout/state helper plus its
   unit tests. Cover 1-4 views, odd sizes, scales, tab wrap, nested cancellation
   and resize. No desktop routing or settings changes.
2. Opt-in shell and Stock: own panel component/test, PlayerView and bounded
   dskGameInterface entry/focus dispatch; existing stock adapter scopes only
   after inspection. Physical driver input proves entry, all tabs selectable,
   Stock data, B/Back/Help, modal isolation and unchanged legacy defaults.
   This slice also owns minimum safe resize, disconnect/reconnect, destruction,
   mixed-device exit and shared-view live Stock refresh. These are prerequisites
   for a usable shell, not deferred to rollout. Other tabs have labeled legacy
   destinations until their page slice lands.
3. Buildings and productivity: actual registry targets/counts/output, empty
   types, centered owner camera and correct child window; no build commands.
4. Statistics: general and merchandise pages using actual histories and replay;
   physical ranges/player toggles and unchanged world/inventory/checksum.
5. Post: filters, asynchronous replacement, text/detail/location/diary, Help
   and physical delete confirmation with correct target and cancellation.
6. View settings: building-position, names/productivity and watch-only actions;
   physical navigation proves per-view state, no global simulation effects.
7. Economy settings: distribution first, transport/tools next, military/build
   order next; one policy per PR. Reuse serialized command paths, real server
   broadcasts and replay guards, preserving Apply/Cancel on B/tab/mouse exits.
8. Parity and default rollout: remaining legacy destinations and the complete
   mixed-device/shared-view regression matrix across all implemented pages.
   Reverify the lifecycle behavior already required in slice 2. Switch
   the controller default only after parity checks and a packaged manual
   Deck/splitscreen exercise; retain the legacy option and record hardware
   checks still outstanding rather than claiming mock input as hardware proof.

## Acceptance for every implementation PR

Run affected Debug Test_* in Sol's own cache with at most two compiler jobs;
confirm any new globbed test suite is registered. Fixtures use the existing
singletons with explicit normal/exceptional cleanup and restoration before
fixture destruction. Tests deliver physical driver events, do not substitute
UI callback invocation for input evidence, and verify actual game/player data
rather than matching the page's own speculative copy. Use a generated replay
where a page can edit; assert no browsing commands, correct live command owner
and exact final replay GF/checksum where relevant.

For the shell and lifecycle slices include two shared-player views and two
distinct-player views, owner/global modal precedence, simultaneous tabs, live
resize, disconnect and mouse/keyboard regression paths. Geometry assertions
cover content/labels as well as the panel outer bounds, and retained classic
windows do not receive hidden controller events. A deliberately reverted
production path must fail the relevant executed case at its intended oracle.

Read-only review uses exactly gpt-6.1-sol. All exact-head CI checks and both
Unit tests/Static analysis workflows must succeed before a ready/tested Opus
handoff. This design PR itself changes no runtime code; its validation is
source-path/action auditing, document consistency and diff checks, not a claim
that any panel behavior exists or has passed runtime tests.
