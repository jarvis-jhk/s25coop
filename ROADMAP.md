# Roadmap

The order work is done in. Each item is sized to finish in one or a few work sessions;
anything bigger is split before it is started. Status: ☐ open · ◐ in progress · ☑ done.

## M0 — A version you can install and keep updated (first!)

- ☑ CI: GitHub Actions builds Linux x86_64 (portable tarball/AppImage) and Windows on every
  tag, publishes a GitHub Release with notes.
- ☑ Versioning + `CHANGELOG.md`, written for players, one entry per release.
- ☑ In-game changelog: after an update the game shows what changed since the last run
  (menu entry "What's new" to show it again). Since 0.1.2.
- ☑ Auto-update on Linux/Deck: the launcher (`install.sh run`) updates from GitHub releases before
  every start. An in-game updater is not needed there.
- ☐ Windows: launcher/updater and a fault-report hook (`handleException` in s25client.cpp). Low
  priority — Jan tests on Deck and Mint; do it after M0.5 unless someone asks.
- ◐ Steam Deck installer (written, untested on a Deck; no grid artwork yet): one `.desktop` file to open in Desktop Mode (pattern:
  ArnoldSmith86/minecraft-splitscreen) — downloads the release, asks for / finds the S2 Gold
  `DATA`+`GFX`, adds a Game Mode shortcut with artwork.
- ◐ Linux (Mint) install path (same installer): same release, simple install script / AppImage.
- The ◐ items above can only be closed by a real start on Jan's Deck/Mint. Until his feedback
  arrives, do not polish them further; work on M0.5 instead (review 2026-09-27).

## M0.5 — Test harness (before merging anything big)

- ☐ Headless mode, split (start from extras/ai-battle/HeadlessGame.cpp):
  - ☑ a) `ai-battle --test --map … --lua test.lua`: fixed seed, N game frames at full speed without
    video/audio; the script's `onTestEnd(gf)` asserts, exit 2 on any Lua error. ctest `CoopHeadless_*`
    (tests/coop/), runs in CI with the unit tests. Since 2026-09-27.
  - ☑ b) every official campaign mission (roman 200–209, world 9 maps) loads with its script and runs
    30000 frames without a Lua error: ctest `CoopMission_*`, local only (`RTTR_COOP_S2_DIR`, original S2
    data never in the repo or public CI). Since 2026-09-27.
- ☑ Scripted input: a test script issues player commands (build, attack, ...) at given game
  frames, through the same command path as network players. `onTestFrame(gf)` + global `test`
  (extras/ai-battle/TestInput.h), ctest `CoopHeadless_ScriptedInput`. Since 2026-09-27.
- ☐ Assertions on game state: buildings, wares, mission flags, and that campaign Lua triggers
  (onGameFrame, onOccupied, victory/defeat, message boxes) fired. One smoke test per campaign
  mission: loads, scripting runs N minutes without Lua errors.
- ☐ Run it in CI on every push; failures reported to the JARVIS lane.
- ◐ Replay-based regression: record a short replay, assert it still replays in sync. First part done:
  same seed twice → same final checksum (ctest `CoopHeadless_Deterministic`).

## M1 — The official campaign, done properly (single group)

- ☐ Campaign status: remember finished missions, unlock the next, mark conquered continents
  (take over upstream PR #1681 by kubaau, and ottml's `enable_next_missions`).
- ☐ Mission unlocking and presentation as close to the original as possible (Roman campaign
  MISS200–209, World campaign).
- ☐ Original videos/intros where the original had them, if they exist in the S2 data.

## M2 — Coop: many players, one tribe

- ☐ Engine: several clients control ONE player slot (commands from any of them act for that
  player; host decides who is in the group). Nobody upstream or in any fork does this yet.
- ☐ Campaign missions hosted as network games instead of local-only, with the shared slot.
- ☐ Save and resume a coop campaign with the same group; deterministic loading
  (upstream `save-rng-state` / PR #1970).

## M3 — Splitscreen, controller and Steam Deck UI

- ☐ Merge derneuere's `splitscreen-gamepad` (local 1–4 players, gamepads, radial build menu,
  couch lobby), adapted so local players can share the one campaign player.
- ☐ Full controller navigation of the main menu and all dialogs; mouse/keyboard unchanged.
- ☐ Couch join screen: in the main menu each gamepad presses A to join; the screen shows how many
  players (and which controller is who). That count drives everything after it.
- ☐ Campaign overview with artwork of every working campaign and single scenario.
- ☐ Detect the Steam Deck and default to a layout and scale made for its 1280×800 screen.

## M4 — More campaigns

In order: Die Rückkehr der Wikinger (complete, Spikeone/RttR_Campaigns), Roman Campaign II
(needs a `campaign.lua`), Oktavianus' Reise, Tyrann, then FANpaign and 2NDpaign (need one-sided
alliances PR #1680 and RTX start positions PR #1683). Licenses/permission are checked before
anything is bundled.

## M5 — Multi-player campaigns and scenarios

- ☐ Maps/campaigns made for several players: one slot per real player, own colour and tribe;
  only when the map has a single human side does everyone share one tribe.
- ☐ Long-term goal: every campaign and scenario ever released for S2 or RttR playable here.

## Continuous

- Every few weeks: review all RttR forks and branches, merge what is safe, note the rest here.
- Keep in sync with upstream master; offer generally useful changes back upstream.
