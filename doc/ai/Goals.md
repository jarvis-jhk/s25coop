# M8 — computer opponent: requirements

Companion to the M8 section of ROADMAP.md (the arena, iteration, difficulty levels).

## The AI must handle every game setting (Jan, 2026-10-03)

Whatever a host can set up, the AI plays it sensibly — not just the default game:

- **Addons**: all of them. Relevant economy addons must be *used*, not merely tolerated: wine
  (vineyard, winery, temple), leather (skinner, tannery, leather works), charburner, trade,
  ships/sea attack, half-cost military equipment, inexhaustible resources, and so on.
- **Nation**: every tribe, including ones with their own buildings or graphics.
- **Map**: any size and shape, islands/sea maps, maps with little space or few resources.
- **Starting resources and conditions**: very low to very high stock, peace time, exploration
  and other objective/victory settings.

State today: aijh reads only CHARBURNER, HALF_COST_MIL_EQUIP, INEXHAUSTIBLE_FISH,
INEXHAUSTIBLE_GRANITEMINES, INEXHAUSTIBLE_MINES and SEA_ATTACK (`grep AddonId::
libs/s25main/ai/aijh`). Wine, leather, trade and most other addons are ignored, so their buildings
are never built.

## Consequences for the roadmap steps

- **Arena (step 0)**: the map list is crossed with a settings matrix (addon sets, nations,
  starting resources), so a change is measured across settings, not only across maps. A run
  records which settings it used, so a result can be reproduced.
- **Iterate (step 2)**: "the AI ignores addon X" counts as a weakness like any other; each
  economy addon gets its own step, proven in the arena with that addon enabled (it must win
  more often with the addon than an AI that ignores it).
- **Difficulty levels (step 4)**: levels must stay apart under every setting, not just the
  default one.
