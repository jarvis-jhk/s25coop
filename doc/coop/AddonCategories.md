<!--
Copyright (C) 2026 s25coop contributors

SPDX-License-Identifier: GPL-2.0-or-later
-->

# Addon browsing

MenuRedesign 3a/3b supplies player-facing categories for the existing addon
window and the future front-end party page. This metadata does not replace the
serialized `AddonGroup` bits, change addon ids or implement presets/achievement
policy from slice 3c.

## Categories

Each registered addon has one category in `addons/AddonCategory.cpp`:

- Comfort: interface aids, cosmetic graphics, default orders, finer production
  and troop controls, automatic flags/demolition, durable signs and alliance
  border handling.
- New content: charcoal, wine, leather and trade.
- World & economy: water supply, resource conversion, waterways, ship speed,
  exploration expedition requirements, fire duration and economy-mode duration.
- Combat: catapult limits, demolition restrictions, defender behavior, rank
  strength/limits, sea attacks, battlefield promotions and statistics visibility.
- Easier: inexhaustible mines/fish, refunds, more animals, cheaper recruits,
  peaceful play and extended resource-worker ranges.
- Developer: AI debugging; hidden unless Show developer addons is checked.

All includes every ordinary category. Hidden developer values are retained when
applying or saving presets. The exhaustive registration test fails if a new
addon lacks metadata. An unknown id has no declared category; the browser
places such a future addon behind the developer opt-in until it is classified.

## Values and input

The Easier/Harder column describes clear resource/travel constraints relative
to the registered default: exhaustible water, expedition scouts, waterway
length, ship speed, rebuilding after fires and the resource-relaxing addons.
New mechanics, combat balance and information access remain unranked because
the effect depends on opponents and map. SEA_ATTACK defaults to status 2
(disabled); status 0 allows attacks, and neither is assigned a universal
Easier/Harder label. These labels are advisory, with no save or victory marking.

LB/RB cycles categories and focuses the selected tab. D-pad navigates controls;
A selects tabs or edits values. Mouse users can click every tab and filter.
Developer has a separate opt-in and is skipped by ordinary category cycling.
Changed only compares accepted staged values with actual addon defaults, not
with zero or the settings at window entry. It works in read-only windows too.
Empty categories say No matching addons, and filters reset the scroll position.

An open dropdown retains its unaccepted preview across LB/RB; A accepts and B
cancels it. Filter membership, difficulty labels, Apply and Save use accepted
staged selections. Another controller applying or saving the window therefore
cannot accidentally commit that preview. Category/filter/default/preset changes
preserve unrelated staged edits and the existing read-only/whitelist policies.
Apply still sends the same single settings callback to the existing lobby.

`Window::SwitchPadTab` is an opt-in root hook used by `FocusPath`. Other windows
retain ordinary shoulder focus traversal. Front-end F7 can reuse the category
metadata; this slice retains the current window rather than replacing the
party page, preset rules or the game's networking.
