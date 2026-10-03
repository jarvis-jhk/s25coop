<!--
Copyright (C) 2026 s25coop contributors

SPDX-License-Identifier: GPL-2.0-or-later
-->

# Graphical controller hints

The per-view in-game hint bar draws Xbox-coloured A/B/X/Y button badges, with neutral
labelled badges for shoulders, D-pad directions, Start/Back and stick clicks.
Left-stick motion keeps its own translated label; it never becomes the L3 click.
Labels remain readable without relying on colour. These are code-drawn shapes with no
external artwork or additional licence dependency.

The bar measures badge padding with the actual UI font, keeps inputs with their action
where they fit, and wraps between actions. A group too wide for the view falls back to
ordinary wrapped text, retaining every input and action. Body text, seat colours, ring
avoidance and input routing keep the same contracts. Layout and emitter tests establish
geometry and actual font draw calls; real Deck appearance remains hardware acceptance.

This slice renders inputs already advertised by the contextual hint source.
Camera/zoom axes (including triggers), device-specific PlayStation/Switch artwork and
hints outside the in-game brief bar remain follow-up work.
