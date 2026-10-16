<!--
Copyright (C) 2026 s25coop contributors

SPDX-License-Identifier: GPL-2.0-or-later
-->

# Savegames from before s25coop

`upstream-94af63d6-gf3000.sav` was written by **unmodified upstream Return to the Roots**
(Return-To-The-Roots/s25client commit 94af63d624bff515b79792d44c1fd735a5e81228, the upstream commit s25coop is
based on). It proves that s25coop still loads saves players made before the fork (savegame format 4.1, no
s25coop player fields).

How it was made: the workflow `.github/workflows/upstream-save-fixture.yml` checks out that upstream commit,
adds only the test driver in `tools/upstream-save` and runs it. The driver replays upstream's own recorded game
`tests/testData/200kGFs.rpl` (map "Big Slaughter v2", one human tribe, seven hard AIs) with checksum checks up
to game frame 3000 and saves it with upstream's `Savegame`, as `GameClient::SaveToFile` does. Nothing of
s25coop's game code is involved. `upstream-94af63d6-gf3000.provenance.txt` records the run, compiler and SHA-256.

Used by `FrontEndLoadTests/UpstreamSave*` (two and four players on one screen) and
`CoopNet_UpstreamSaveResume` (an online co-player).
