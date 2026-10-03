<!--
Copyright (C) 2026 s25coop contributors

SPDX-License-Identifier: GPL-2.0-or-later
-->

# In-game controller mapping

Audit of `dskGameInterface`, `FocusPath`, `PadRouter` and `PlayerBrief`, 2026-10-03.
This describes the implemented legacy UI. The future controller panel retains its own
[design and rollout gates](ControllerPanel.md). Menu/lobby input and real Deck hardware
remain separate acceptance work.

## Fixed display shortcut

**Click the left stick (L3) to toggle building spots for your own view.** The same click
works in the world, during road planning, in a ring, inside a window or dropdown, and
behind a modal confirmation. It never selects or confirms anything in those controls.
The hint bar calls the click `L3 Build aid`; moving the left stick remains a distinct input.

Visible spots (including cursor-only aid) switch off; an off display switches to all spots.
An explicit off choice prevents the automatic cursor aid from reappearing when opening
a build menu. The existing system-menu button still cycles off / cursor / all. Its label
refreshes after the shortcut, without moving its selected sector.

In Just watch, requesting building spots explicitly leaves that mode and shows all spots.
Names/output return to their saved settings. B remains the independent way to leave Just
watch and restore the exact prior display without choosing a new building-aid setting.
Only the main view persists its display choice, as before; other seats do not alter the
shared preferences. The shortcut does not issue game commands or spend resources.

## Input precedence and modes

1. L3 handles its own press/release before all contextual routing.
2. An open ring consumes every other button for that seat.
3. Just watch consumes other buttons, with B restoring the display.
4. Back toggles the system ring when allowed; Y can enter a newer top window.
5. Active window focus consumes all remaining buttons.
6. Remaining presses reach world / road actions. Releases have no world action.

World buttons: A opens the object window, starts a road on an owned flag, or opens an
available action ring. X places a flag. RB opens the action ring, including a flag's
geologist/scout/demolition choices. LB starts a waterway at a suitable owned water flag.
Y enters the top window. B closes an eligible top window. Back toggles the system ring.
Invalid A/RB actions use the existing rejection feedback. D-pad has no world action.

Road planning: A extends or shortens the preview to the pointer when a valid path exists.
X commits a valid road; B removes the last piece or cancels an empty preview. Y can enter
an eligible open window. Back/LB/RB have no road action. L3 does not alter the road preview.

Ring: left-stick movement aims at a sector; D-pad moves by a sector, only when there is a
choice. A activates it. LB/RB switch available pages or tabs, with no wraparound shortcut
invented for a single page. B and Back close the ring. X/Y have no ring action.

Window focus: A activates the selected control if supported. D-pad and left-stick motion
first adjust a value when that control accepts it, otherwise move focus. LB/RB move to
previous/next focus stations, without wrapping. A opens a dropdown, directions browse,
A confirms, and B cancels its preview; otherwise B releases focus. Back remains conditional
on the system-menu gate. Y can enter a newer top window. L3 leaves pending input untouched.

Motion: the left stick moves the world pointer (and pushes the camera at the viewport edge)
outside focus/ring/watch modes. The right stick pans the owner's camera; LT zooms out and
RT zooms in. Camera and zoom stay usable while watching. Stick clicks are button events,
not axis movement. Pressed-state filtering permits one L3 toggle per press, without repeats
while held; disconnected devices cannot send a shortcut to their former seat.

## Unused controls and follow-up scope

Start only takes an available pad in hand; it remains harmless after assignment. Guide
remains reserved for the platform because drivers can intercept it. R3 has no in-game
action. X/Y and the shoulders retain the contextual gaps listed above. Avoid assigning
destructive global actions merely to fill those gaps: current A/X/road distinctions are
part of the tested confirmation policy.

Further useful mappings need separate policy and physical-input tests: a stable camera/HQ
shortcut, optional names/output shortcuts, and help exposing camera/zoom alongside the
contextual hint bar. No such binding or coloured glyph rendering is implemented here.
Any change to those routes needs fresh shared-input/view claims; L3 is the bounded first
implementation from the Deck mapping feedback.
