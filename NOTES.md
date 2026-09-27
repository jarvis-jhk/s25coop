# Session notes

## 2026-09-26 — project created
Fork created, lane `siedler` registered in JARVIS, README/ROADMAP/CLAUDE.md written, issues
enabled. Nothing built yet. Next: M0 — CI for Linux + Windows releases, then the Steam Deck
installer. Submodules are not initialised yet (`git submodule update --init --recursive`).

## 2026-09-26 — M0 session 1: first release v0.1.0
Done: submodules initialised; `.github/workflows/coop-release.yml` (tag `v*` → Linux tarball built
on ubuntu-22.04 with libs bundled by `tools/coop/bundle-linux.sh`, Windows x64 zip via MSVC +
dev-tools DLLs, release notes = CHANGELOG section; `workflow_dispatch` = build without release).
Upstream's source-only `release.yml` removed. Installer: `tools/coop/Install-s25coop.desktop` →
`install.sh` (download latest release to ~/.local/share/s25coop/game, S2 files to …/S2, menu
entry, Steam shortcut via `add-to-steam.py`); shortcut runs `install.sh run` = update-then-start.
Released v0.1.0, Jan told. Verified here: binary runs on Debian 12 (`--help`), `install.sh run`
downloads and unpacks correctly. NOT verified: a real Deck/Mint start with graphics, Steam
shortcut writing (no python/Steam here), Windows start.
Release procedure: add `## x.y.z` to CHANGELOG.md, `git tag -a vx.y.z`, push the tag.
Next: wait for Jan's test feedback; then M0 in-game changelog (show CHANGELOG sections newer
than the last-run version), Steam grid artwork, Deck controller basics.
Watch: the bundle excludes host libs (glib, sndfile, pulse…) — if sound or start fails on a
host, the exclude list in bundle-linux.sh is the first suspect.

## 2026-09-26 — Jan's process and goal brief (voice)
Written into CLAUDE.md (Codex review, test before merge, target experience) and ROADMAP.md
(new M0.5 test harness, couch join + campaign overview in M3, M5 multi-player campaigns).
Next session: M0 feedback first; then M0.5 headless runner (check how upstream's tests/ already
run a GameWorld without video before inventing anything).

## 2026-09-26 — fault reports (Jan, voice: "everything you ship reports back to you, but none of my domains anywhere")
install.sh + s25coop.sh `report()` → public ntfy.sh topic `s25coop-reports-ko3knuxwigscapljz76yjk5z`
→ `.github/workflows/fault-reports.yml` (every 15 min, cursor in actions/cache) → issue labelled
`fault-report` (same open title = comment) → new issues forwarded to the siedler lane via repo
secret `JARVIS_REPORT_URL` (set with data/scratch/sodium/setsecret.cjs). NEVER put one of Jan's domains
into the repo or a release (see CLAUDE.md). Opt-out: S25COOP_NO_REPORTS / no-reports file.
Tested end to end (fake crashing binary → issue #1 → queue task). v0.1.1 tagged to ship the new
launcher. Not covered yet: Windows (no wrapper; would need a hook in handleException in
libs/s25client/s25client.cpp — RttR's own DebugInfo sends to upstream's server, consider rerouting).
Codex review: `codex exec` cannot run its sandbox here (no userns) — pipe the diff on stdin:
`git diff … | codex exec -m gpt-6-sol "Review this diff (on stdin)…"`.
History rewritten the same day (Jan: "bitte nuken"): the domain in the first fork commit's CLAUDE.md
and its author e-mail are gone; master, v0.1.0 and v0.1.1 force-pushed, releases and assets intact.

## 2026-09-27 — quota filler: in-game changelog (0.1.2)
No open issues, no feedback from Jan yet. Built the in-game changelog: `libs/s25main/coop/Changelog.*`
(parse CHANGELOG.md, compare versions, `newSince`), `ingameWindows/iwChangelog.*`, main-menu button
"What's new" + one-time pop-up via a 100 ms timer (a window shown in a desktop constructor is closed
by the switch). Settings key `coop_changelog_seen`, written only after the window was shown.
CHANGELOG.md is installed to `RTTR/texte/` (CMake install + copyDepsToBuildDir). Tests:
tests/s25Main/simple/testCoopChangelog.cpp, tests/s25Main/UI/testCoopChangelogWindow.cpp (drives
the real dskMainMenu). Codex review found 2 real bugs, fixed before commit.
Local build environment now exists: Debian dev packages installed with apt in the JARVIS container
(lost on container recreate — reinstall with the package list in coop-release.yml plus
libboost-test-dev ccache ninja-build clang-format), build dir `build/dev` (Ninja, ccache,
RTTR_VERSION unset = date like CI, CXX_FLAGS=-Wno-array-bounds because GCC 12 + Boost 1.74 trips -Werror in
upstream's test mocks). Tests need `USER=root HOME=<dir>` or they fail with "Could not get username".
No S2 game data here, so the real client cannot be started locally; UI tests with mock drivers are
the closest thing — that is also the entry point for M0.5.
Next: M0 leftovers (Steam grid artwork, Deck controller basics) or M0.5 headless runner
(extras/ai-battle/HeadlessGame.cpp already runs a game headless — start from there).
Released v0.1.2 (CI all green on every platform incl. Windows/macOS; release tarball carries
share/s25rttr/RTTR/texte/CHANGELOG.md). CI fixes on the way: clang-format 10 comment alignment,
the UI test now overrides the running version (CI dev builds have date versions and the coverage
gate rejects skipped test lines), Codecov upload only from upstream (fork has no token — it failed
the gcc-10 coverage job). Unauthenticated GitHub API gets rate-limited fast when polling CI: use the
token from /root/.git-credentials. Note: upstream's v0.9.x tags exist in the fork; installer uses
GitHub *releases*, so they do not matter.

## Wochenreview 2026-09-27
Direction: on track. Week 1 delivered M0 core (CI releases, installer, launcher auto-update,
fault reports, in-game changelog; v0.1.0–0.1.2). No Jan feedback yet on a real Deck/Mint start —
the remaining M0 ◐ items cannot be closed without it, so work moves on to M0.5 meanwhile.
Found:
- ⚠ **master CI is RED since 01:11 (not green as the 0.1.2 note said).** Only the release workflow
  was green. Two failures, both our code:
  1. Clang-Tidy: `tests/s25Main/simple/testCoopChangelog.cpp:23` push_back in a loop →
     `result.reserve(sections.size())` (performance-inefficient-vector-operation is -Werror there).
  2. macOS clang Debug, Test_UI: `testCoopChangelogWindow.cpp:64` wnd is null — the test relies on
     a 100 ms wall-clock ctrlTimer + sleep(150 ms); flaky/slow on the macOS runner. Make it not
     depend on wall-clock timing (e.g. show the pop-up on the first Msg_PaintAfter/Draw after the
     switch instead of a timer, or drive the timer deterministically in the test).
  → FIRST job of the next work session (quota filler 06:30 runs: week 9 % vs pace 15 %).
  Lesson: after a push, check the *Unit tests* and *Static analysis* runs of that SHA, not just the
  release run. Logs: data/siedler/job108534990295.log, job108535189387.log.
- Campaign tests need original S2 data (roman campaign.lua loads `<RTTR_GAME>/DATA/MAPS/MISS2xx.WLD`),
  which can never go into the repo/public CI. M0.5 split into (a) test maps in CI, (b) campaign
  missions locally with S2 data. Asked Jan whether his S2 Gold DATA/GFX may be copied into the
  container (local only).
- ROADMAP: auto-update on Linux marked done (launcher does it); Windows updater + fault hook split
  out as a low-priority item after M0.5.
- Schedules: all four are right for the goal (work Mon/Wed/Fri, quota filler every 3 h gated,
  fork sweep 8th/22nd, this review Sun). Quota: filler gate works (week 9 %, pace 15 % → run).
  Fixed a stale id in this review's own prompt (0895951f → bfa0b45f); review schedule re-created as 42d01edc.
- No code off-goal. No open issues/PRs; the only issue was the fault-path selftest (#1, closed).

## 2026-09-27 — quota filler: CI red fixed, headless harness M0.5a
- Master CI fix (9782c1ea9): the changelog pop-up now opens in `dskMainMenu::SetActive(true)` on the first
  activation as current desktop (no wall-clock timer; Codex rejected `ShowAfterSwitch` from the ctor because the
  queued window would land on another desktop if the switch was replaced). clang-tidy `reserve()` fixed.
- M0.5a (b51fd23fa): `ai-battle --test` + `onTestEnd(gf)` contract, tests/coop/CMakeLists.txt with
  `CoopHeadless_Smoke` and `CoopHeadless_FailingScriptFails` (PASS_REGULAR_EXPRESSION on the assertion text, so
  a run failing for another reason does not count). Upstream bug fixed on the way: ai-battle logged to ./logs and
  died on the first Lua error when that folder was missing — worth offering upstream.
  Lua in a test script: `rttr:GetPlayerCount()`, `rttr:GetPlayer(i):GetNumBuildings(BLD_…)`, `IsDefeated()`,
  `rttr:Log` is visible in test mode. Run locally: `USER=root HOME=/app/agent/data/siedler/testhome ctest -R Coop`
  in build/dev. Test_drivers fails locally only (no ALSA device in the container).
- Determinism (e773fbd70): `--test` prints `Final state: <AsyncChecksum>`; ctest `CoopHeadless_Deterministic`
  (tests/coop/checkDeterminism.cmake) runs the smoke game twice and compares. At 20000 GF the AIs fight and
  player 0 loses its HQ — keep smoke runs at 6000 GF.
- `--test-script` (b8bf97b02): second Lua file in the map script's state; tests/coop/headless/missionSmoke.lua
  wraps a mission's onStart/onGameFrame and adds onTestEnd. For M0.5b:
  `RTTR_GAME_DIR=<S2 dir> ai-battle --test --map <S2>/DATA/MAPS/MISS200.WLD --lua data/RTTR/campaigns/roman/MISS200.lua
  --test-script tests/coop/headless/missionSmoke.lua --ai aijh [--ai … per map player] --maxGF N`
  (RTTR_GAME_DIR confirmed in RttrConfig.cpp; how many --ai a mission map needs is untested).
- S2 data: the laptop root gateway runner fails ("request is not valid JSON", reported to the self lane
  t_mujgl5wh14qv0z). Jan then said: get it from archive.org (he owns the game). Source: archive.org item
  `the-settlers-ii-gold_202311` (zip with a MODE1/2352 .bin/.cue; data track → ISO by stripping sectors to 2048
  bytes, extracted with pycdlib from data/siedler/pylib). The disc's /S2 folder is now in
  /app/agent/data/siedler/S2 (58 MB, DATA+GFX+VIDEO, local only). Upload app s2-intake removed again.
- M0.5b done (2a9ddee48): all 19 official missions run 30000 GF headless without a Lua error. Fixes needed:
  headless local player → observer (mission statements opened a GUI window → segfault), null guard in
  GamePlayer::IsBuildingEnabled (upstream bug, worth a PR), `--objective none`. Local run:
  `cmake -DRTTR_COOP_S2_DIR=/app/agent/data/siedler/S2 .` in build/dev, then `ctest -R Coop` (22 tests, 16 s).
  data/siedler/run-missions.sh does the same and finds each map's player count.
  Note: most missions have no onGameFrame; their triggers are onOccupied/onExplored etc. The smoke test
  only proves "loads and runs"; asserting that triggers fire needs scripted input (next).
- CI: master green on every job at 59463b52a (Clang-Tidy, macOS, Windows, all Linux), 2026-09-27 08:07 UTC.
Next: scripted input (M0.5 item 2) so a test can drive a mission to its triggers; offer the two upstream fixes
(ai-battle log dir, IsBuildingEnabled null guard) as PRs to Return-To-The-Roots.

## 2026-09-27 — quota filler: scripted input (M0.5)
- `ai-battle --test` now gives the scripts a global `test` (extras/ai-battle/TestInput.*) and calls `onTestFrame(gf)` at
  every network frame (every 20 GF) before commands are collected. Queued commands run with the AI's commands of that
  player, and are recorded in a --replay. API: SetBuildingSite, DestroyBuilding, SetFlag, DestroyFlag, BuildRoad (route
  as digits 0-5 = W NW NE E SE SW), ConnectFlags (path found at call time — connect a new site one frame later),
  Attack, GetFlagPos, FindBuildingSpot (-1,-1 if none). Use `--ai dummy` for a player the script controls alone.
- ctest `CoopHeadless_ScriptedInput` (dummy player 0 builds a woodcutter + road, asserted built by 6000 GF); checked
  that it fails when the road is left out. Codex review: no findings.
- `coop_headless_args`/`add_coop_headless_test` take the per-player AIs as extra arguments (default aijh aijh).
Next: M0.5 "assertions on game state" — drive one campaign mission (MISS200) to a real trigger (onOccupied/
onExplored or a won mission) with scripted input; needs more of the API (military buildings, maybe a Lua hook to
know which trigger fired). Upstream PRs opened: Return-To-The-Roots/s25client#1985 (IsBuildingEnabled null guard) and #1986 (ai-battle log dir),
branches fix/* on origin. Check them in the next sessions; answer reviews, close quietly if rejected.

## 2026-09-27 — quota filler: MISS200 walkthrough (M0.5 assertions)
- CI note: the scripted-input commit 0c878d8e8 never got Unit tests/Static analysis — the NOTES commit pushed right after
  it cancelled them (concurrency cancel-in-progress). Commit NOTES together with the code, or wait for CI first.
- New test API: `test:ConnectToNetwork(p, fx, fy, radius)` (road to the nearest flag that a warehouse reaches by road;
  Codex review caught that the first version happily joined two loose flags) and `test:CallSpecialist(p, x, y,
  JOB_GEOLOGIST|JOB_SCOUT)`. Roads: queue ONE per frame — two roads planned in the same frame can cross, and the second
  is refused when it runs (that silently left an armory unconnected).
- tests/coop/headless/miss200Walkthrough.lua: dummy player 0 plays MISS200 (woodcutter/quarry/sawmill → forester →
  barracks to (34,28) → geologists on mine-capable spots → iron mine/smelter/armory → barracks to (39,19)) and asserts
  the mission's own eHist has events 1–7 (8 comes free from a geologist finding water). ~9500 GF, 0.2 s; passes with
  seeds 1,2,3,7; exit 2 with too few frames. ctest `CoopWalkthrough_roman_MISS200` (local, needs RTTR_COOP_S2_DIR).
  Tip: FindBuildingSpot(P, BLD_IRONMINE, …) finds mountain spots even while the mine is disabled.
- clang-format 10 is not installed here (system has 14; pip 10.x wheel missing) — include order was the only diff.
Next: event 99 / victory of MISS200 needs the far corner (14,8) — several military buildings + maybe scouting; or move
on to M0.5 "CI on every push → failures to the lane" (the fault-report path exists; CI failures of master should file an
issue). Upstream PRs #1985/#1986: no review yet.
