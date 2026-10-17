<!--
Copyright (C) 2026 s25coop contributors

SPDX-License-Identifier: GPL-2.0-or-later
-->

# Graphical controller hints

The per-view in-game hint bar draws Xbox-coloured A/B/X/Y button badges, with neutral
labelled badges for shoulders, D-pad directions, Start/Back and stick clicks.
Stick motion keeps its own translated label; it never becomes the L3/R3 click.
The right-stick camera badge and LT zoom-out / RT zoom-in badges follow the existing
legacy input routing in the world, roads, rings, focused windows and Just watch.
A zoom direction disappears at its target-zoom limit and returns after zooming away;
the limit uses the commanded target, not the still-interpolating visual zoom.
Labels remain readable without relying on colour.

Since the pixel-art slice the inputs are drawn as Xbox-style button images in the game's
look (`data/RTTR/assets/base/pad_glyphs`, 8-bit palette BMPs, index 0 transparent). They
are drawn from code by `tools/coop/padglyphs.py` - run it after changing the artwork; the
file order is `brief::PadGlyph` in `input/KeyGlyph.h`. The D-pad has one image per set of
lit arms, so all D-pad directions of one action share a single image. No image is taller
than 13 px. The folder is optional: if it is missing, or an image is, that input keeps the
code-drawn badge described above, so nothing depends on external artwork or a licence.
Both the in-game bar and the front-end footer draw through `brief::DrawKeyRuns`.

The bar measures badge padding with the actual UI font, keeps inputs with their action
where they fit, and wraps between actions. A group too wide for the view falls back to
ordinary wrapped text, retaining every input and action. Body text, seat colours, ring
avoidance and input routing keep the same contracts. Layout and emitter tests establish
geometry and actual font draw calls; real Deck appearance remains hardware acceptance.

The camera/zoom follow-up extends the typed hint source without changing a binding.
Mouse-only and disconnected views still have no controller brief. Device-specific
PlayStation/Switch artwork, hints outside the in-game brief bar and the future panel
remain follow-up work; the panel has its own motion-suppression policy.
