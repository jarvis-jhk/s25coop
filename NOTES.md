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

## 2026-09-27 — quota filler: replay regression (M0.5)
- `ai-battle --check-replay <rpl>` (HeadlessGame::PlayReplay): unpacks map+Lua from the replay, runs no AIs, executes the
  recorded commands and compares each recorded AsyncChecksum (taken before the frame's first command, as in Run) with the
  replayed game; exit 3 on async or if the game ends before the recorded last GF / leaves commands unread (Codex review
  caught the early-end case). `--random_init` overrides the seed. Prints `Final state:` like --test.
- ctest `CoopHeadless_ReplayInSync` (tests/coop/checkReplay.cmake): records the scripted-input game, checks it in sync with
  the same final state, and checks that a wrong seed is reported async. Manually also in sync: MISS200 walkthrough
  (18 checksums) and a 30000-GF AIJH fight (669 checksums).
- ROADMAP M0.5 CI item closed: CoopHeadless_* run in CI already and failed workflows reach the lane via the webhook.
Next: M0.5 leftovers are optional (MISS200 event 99/victory; GameClient-side replay). Suggest moving to M1 (campaign
status / unlocking: upstream PR #1681 by kubaau + ottml's enable_next_missions) or M2 engine groundwork (several clients
→ one player slot), both now testable headless. Upstream PRs #1985/#1986: still no review.

## 2026-09-28 — quota filler: campaign progress (M1, 0.1.3)
- Took over upstream PR #1681 (kubaau/campaign_status, merged with authorship) and fixed: world scripts used 1-based
  chapter ids (Europe marked Africa conquered), world luaFolder "<RTTR_RTTR>/CAMPAIGNS/WORLD" reverted to "" (case
  breaks on Linux), a config without [campaigns] was reset to defaults (every existing install!), save string padded
  with NUL bytes instead of '0', "chapter 0" on the victory screen, IsWinnerHuman bug (Flamefire), victory screen
  now reset at Game::Start, settings saved in ShowMenu when progress changed, default chapters always playable.
- onHumanWinner now fires also without a GUI, and only when the announced winner (team or player) has a human.
- Tests: testCampaignSettings (roundtrip, old config, defaults floor, no gaps, every shipped mission completes its own
  index), testLua HumanWinner* (CheckObjective via Start(true) + SetStatisticValue), local ctest `CoopCampaign_<c>_<map>`
  (tests/coop/headless/missionComplete.lua finishes each mission via event 99 / onHumanWinner; ai-battle --test prints
  `Campaign progress: <uid>=<code>`). 83 local tests pass (Test_drivers local-only failure as always).
- Codex review: winner attribution (fixed + tests). Rejected: "MISS209 enables chapter 10" (it does not; it calls
  SetCampaignCompleted). Open design point for M2/M5: progress is written to every client's local SETTINGS
  whatever that client's player did — right for coop (all share the win), wrong for versus campaign maps.
- Upstream #1681 is kubaau's draft; our fixes were offered back as a comment on that PR (2026-09-28 01:00).
Next: M1 "mission unlocking and presentation as close to the original" (check the victory screen/unlock UI against the
original; Hirotaro's screenshot in #1681), or M2 groundwork. Upstream #1985 approved by Flow86, #1986 no review.
- CI on 07bf3205a was red (fixed in the next commit): Windows Test_integration — the settings test saved into a user data
  folder that does not exist on a fresh runner (Save() does not create it; the test now does); StyleAndFormatting —
  kubaau's nested ternary is formatted differently by clang-format 10; Clang-Tidy — CampaignID taken by value.
  Lesson: a test that writes CONFIG.INI must create the folder; run a style check on taken-over code, not only ours.
- CI green on every job at c02b10d67; released v0.1.3 (tarball checked: `s25client --version` = v0.1.3, CHANGELOG
  inside). Jan told. The 02:00 regular work session request was folded into this session.
- Upstream notifications on Return-To-The-Roots/s25client arrive via JARVIS's github-inbox → adhoc; the file
  /app/agent/data/pr-watch/s25client-upstream.md tells adhoc to forward them to this lane.
Next: M1 presentation vs. the original (victory screen, locked mission buttons, world map markers) — needs a GUI look;
maybe a UI test with mock drivers that opens dskCampaignMissionSelection with S2 data locally and checks locked buttons.

## 2026-09-28 — quota filler: M2 design + network test harness (step 0)
- No open issues, CI green. Started M2 (several clients → one player). Design: doc/coop/SharedPlayerSlot.md. Members are
  a connection role on the server (not a player id); the server seals each world player's command set once, as leader
  cmds + every member cmd that arrived since, BEFORE nwfInfo.addPlayerCmds and the broadcast → world, replays and saves
  unchanged. Codex reviewed the design (15 points folded in: no fake ids, merge before addPlayerCmds, tagged checksums,
  refuse orders of a desynced member, leader hand-over is atomic). ROADMAP M2 item 1 split into steps 0–6.
- Step 0 done: `extras/coop-net` — `coop-net host --port P --maxGF N --map M --ai aijh` / `coop-net join --port P
  --maxGF N --wait-for <host --out>`; real GameServer+GameClient, no video (GameClient::DrawWaitCursor now skips the moon
  without a video driver; the harness calls GameLoaded/OnGameStart itself like dskGameLoader/dskGameInterface do, and
  unpauses). The server compares all checksums every NWF; the host only reports success once its GF is
  (cmdDelay+1)·NWF past maxGF, so the last checksums were compared (Codex review finding). Exit 0 ok / 1 setup / 3 async /
  4 error. `--desync-at GF` reseeds RANDOM in one process to prove detection.
- ctest (tests/coop/checkNetGame.cmake, execute_process runs both processes at once, each with its own HOME; `--log`
  keeps stdout off the pipe): CoopNet_TwoClientsInSync (1000 GF on data/RTTR/MAPS/NEW/KARTE06-fix.swd, ~12 s),
  CoopNet_DesyncDetected (GF 300), CoopNet_DesyncAtEndDetected (GF 999). Not on Windows: shared user folder, and
  GameServer::SendAsyncLog opens a MessageBox on async.
- Gotchas: RTTRCONFIG.Init() chdirs — make paths absolute before it. A map with a .lua beside it gets that script
  (LuaFunctions.SWD's is a unit-test script that errors). The game starts paused until OnGameStart().
- CI on acd6ecfcd: clang builds failed on an unused variable (GCC does not warn — build with clang locally or read
  twice), markdownlint wants a blank line after the licence comment. Fixed in the next commit.
Next: M2 step 1 (server merge) + step 2 (client member mode) — extend coop-net with `join --member-of 0` and scripted
orders (reuse extras/ai-battle/TestInput) so a test proves a member's building appears in the leader's world in sync.

## 2026-09-28 — quota filler: M2 steps 1+2 (members)
- Server: `libs/s25main/network/GameServerCoop.cpp` — members are `CoopMember`s (own list, own socket polling, a
  whitelist handler `CoopMemberHandler`; anything else kicks the member). Joining: the member answers whatever
  Player_Id it gets (free slot or none) with the new `GameMessage_Coop_JoinMember(leader)` (NMS 0x0701); a reserved
  slot is handed back. Then the stock handshake; map only after version AND password passed (Codex finding). Its
  commands are buffered per leader and appended to the leader's set in OnGameMessage(GameCommand) before addPlayerCmds
  and the broadcast. Members off by default: `GAMESERVER.SetAllowCoopMembers(true)` (harness: host `--members N`);
  caps: 16 member connections, 1000 buffered orders per leader. Leader kicked → its members are closed (step 5 later).
- Client: `GameClient::Connect(..., coopMemberOf)`; GetPlayerId() = leader. Bug found by testing: nobody waits for a
  member, so under load it fell > 2·cmdDelay NWFs behind and NWFInfo dropped command sets ("He might be cheating") →
  silent desync. Fix: `NWFInfo::setUnboundedCmds` for members + catch-up (run GFs without waiting while more than
  cmdDelay NWFs are pending).
- Harness: `coop-net join --member-of 0 --build-at GF` (orders a woodcutter near the HQ), `--stall-at GF` (2 s pause),
  `--trace N` (checksum every N GF, to find where two processes diverge), `--connect-delay S`; every process writes
  `State at GF <max>: checksum …, woodcutters …` into its --out file. ctest (tests/coop/checkNetMembers.cmake):
  CoopNet_MemberOrders, CoopNet_MemberAndSecondPlayer (member first takes and returns free slot 1),
  CoopNet_MemberFallsBehind (fails without the fix), CoopNet_MemberDesyncDetected. 7 CoopNet tests × 10 loops green.
- Codex review: 4 findings, all fixed (password bypass, no opt-in, unbounded order buffer, unbounded pending
  connections). Not tested automatically: refusal when members are not allowed (checked by hand).
Next: M2 step 3 (join flow and lobby: the host allows members per slot, the lobby lists them, a GUI "join player X";
campaign default = the human slot), then step 4 (campaign as network game, MISS200 walkthrough split over two clients).
- GitHub event (same session): fork CI "failed" on fix/ai-battle-log-dir after Flamefire rebased upstream PR #1986 onto
  upstream master — all tests passed, only the Codecov upload failed (the branch carries upstream's workflow, tokenless
  upload is refused off the default branch). Upstream CI on that commit is green; nothing to do. #1985 is merged upstream.
- Clang-Tidy on the member commit: bugprone-exception-escape on CoopMember's implicit move-assignment (vector erase
  moves elements). Members are now `std::unique_ptr<CoopMember>`, copy deleted. Lesson: a new struct held by value in
  a vector that gets erase_if'd needs a nothrow move — or hold it by pointer.

## 2026-09-28 — Jan's requests: GOG installer, Intro, Steam artwork, default display (0.1.4)
- Intro: RttR's Intro button was always disabled (`dskIntro` was an empty page). Now `SmackerVideo`
  (libs/s25main, wraps vendored libsmacker in external/libsmacker, LGPL-2.1) decodes `<RTTR_GAME>/VIDEO/INTRO.SMK`
  (case-insensitive lookup); `dskIntro` plays it scaled to 4:3 (320x200 VGA), audio as one WAV effect
  (louder of music/effects volume), music paused meanwhile, any key/click skips, decode error ends it. Button
  enabled only when the file exists. Test_sounds/SmackerVideoTests (full decode of the real intro when
  RTTR_COOP_S2_DIR is set). Seen in the real client under Xvfb (data/siedler/introtest/run.sh; Xvfb needs
  `SDL_VIDEO_X11_FORCE_EGL=1`, GLX fails with GLXBadContextTag).
- Videos in the original: SETTLER2.EXE (DOS launcher) names INTRO.SMK and CREDITS.SMK, but no disc ships
  CREDITS.SMK (checked: 1996 VVV CD, Gold CD, eXoDOS zip, 7z copy, GOG). The campaign itself plays no video,
  so the intro is the only one. S2.EXE shows DATA/CREDITS.LST pictures instead (RttR's Credits page).
- Installer: `tools/coop/s2-extract.py` (stdlib only) looks ONLY in the Downloads folder (xdg-user-dir +
  ~/Downloads, one level): GOG `setup_the_settlers_2_gold_*.exe` via bundled static innoextract 1.9 (release
  workflow downloads it, sha256 pinned; GOG installer is Inno 5.6.2), ISO/bin/img/mdf/nrg via its own
  ISO9660 reader (sector layouts 2048/2352/2336/2448, Nero offset), zip (disc image preferred over an
  installed copy, which lacks VIDEO), .7z via 7z/7za/7zz/bsdtar. Copies DATA+GFX+VIDEO, swapped in as a whole.
  Nothing found → `s2-help.html` opened (what, why, filenames, GOG link) and "look again" loop. `install.sh run`
  and s25coop.sh also run it, so a user can drop the file in Downloads and just start the game.
  Tested on: GOG exe (7 s), The Settlers II Gold.zip (identical to our S2 copy), Settlers_II_The_Gold_Edition_1997.zip,
  siedler2.zip (installed copy, no VIDEO), siedler2gold.7z, The Settlers II.img, Die Siedler II.img.
  All 19 official missions pass the headless smoke run on the GOG data (GOG maps differ from the CD: patched).
  Test fixtures: /app/agent/data/siedler/dl (archive.org) and gogdl (Jan's GOG installer, local only).
- Steam: entry named "The Settlers II: Coop" with SteamGridDB artwork (Gold Edition, game 5247477; grid p/wide,
  hero, logo, icon), URLs in add-to-steam.py. Existing "s25coop" entries: `install.sh run` adds the pictures
  (no Steam restart); re-running the installer renames them (closes Steam; skips if Steam will not close).
  install.sh now replaces itself by rename (it may be the running script).
- Display: default is a borderless window at the desktop resolution (Settings::LoadDefaults; existing
  CONFIG.INI keeps its mode).
- Codex review: 7 points, all fixed (ISO name traversal, zip member path, malformed ISO aborting the search,
  non-atomic swap, per-user Steam rename, Steam not closing, decode error freezing the video).
- ROADMAP: self-updating AppImage added to M3, after splitscreen+gamepad (Jan).

## 2026-09-28 — CI red on de98b41 (coverage gate)
gcc-10 coverage job: checkTestCoverage rejected testSmackerVideo.cpp — the PlaysOriginalIntro body
only runs with RTTR_COOP_S2_DIR (game data), which CI never has. Wrapped the body in
LCOV_EXCL_START/STOP (37cdb2f). Rule: any test that needs the original game data gets that exclusion.

## 2026-09-28 — CI: UBSan overflow in the lag pause
Unit tests on master failed only on clang-12 + sanitizers: CoopNet_Member{Orders,FallsBehind,DesyncDetected}
aborted the member in upstream's `GameClient.cpp` lag pause, `rand() * 4 * gf_length` (int overflow before
the multiply reaches the int64 duration). Our member tests are the first to hit a lagging NWF under UBSan.
Fixed by multiplying the duration first; CoopNet tests pass locally. Candidate for a tiny upstream PR.

## 2026-09-28 — quota filler: M2 step 3, lobby protocol
- No issues, master CI green at 4b2fb3d. Step 3 split into protocol / lobby GUI / campaign default (ROADMAP).
- Protocol done: NMS_COOP_MEMBERS (server → all: allowed + {id, leader, name}), NMS_COOP_ALLOW_MEMBERS and
  NMS_COOP_KICK_MEMBER (host only), and NMS_COOP_JOIN_MEMBER from an ACTIVE lobby player = switch to member (server
  swaps both queues into the new CoopMember, frees the slot via Player_Kicked(NoCause); refusal answers NO_PLAYER_ID and
  the client stays a player). Client: GetCoopMembers/AreCoopMembersAllowed/JoinCoopMember/IsCoopSwitchPending,
  CI_CoopMembersChanged; host: GameLobbyController::SetCoopMembersAllowed/KickCoopMember. Details in
  doc/coop/SharedPlayerSlot.md (Status).
- Harness: host --members-via-lobby, --open-slots N, --kick-members; join --switch-to-member P, --switch-now,
  --expect-kick; result files carry "Members at start/at the end". ctest CoopNet_LobbySwitch (member built the
  woodcutter, states equal), CoopNet_LobbyRefused (plays on as player 1), CoopNet_LobbyKick (host log must say
  "kicked by the host"). Message round-trip: Test_simple GameMessages/CoopSerialization.
- Codex review: fixed 0xFF leader sentinel + CLI range, kick test accepted any disconnect. Rejected: "recvQueue swap
  while executeMsgs loops" (it pops before running), "member of the host can send host messages" (member handler
  whitelist refuses them).
Next: lobby GUI (dskGameLobby): members under their player row, a "play together" button on occupied human rows when
  allowed, host checkbox "allow co-players" + kick; a UI test with mock drivers like testCoopChangelogWindow. Then the
  campaign default and step 4 (campaign as network game).
- CI on a4f59bc: StyleAndFormatting (clang-format 10 packs the GENERATE_CALLBACK list differently) and Clang-Tidy
  (performance-inefficient-string-concatenation inside the run loop, bugprone-exception-escape from option reads in
  main) — fixed in the next commit; player-index options now validated by a po notifier inside the try.

## 2026-09-29 — quota filler: M2 step 3, lobby GUI
- No issues, master CI green at f56218f. Lobby GUI done (dskGameLobby co-player row above the chat, helpers in
  libs/s25main/coop/CoopLobby.*): host "Allow co-players" checkbox (400,430) + member list + Remove; players choose
  "Play X's tribe" + Join; members see "You play together with X", no Start/Ready, no changes to the row
  (`IsOwnRow`); rows show "Jan +2". Chat area starts 25 px lower. Selection survives rebuilds (every player change).
- Tests: UI/CoopMembersHostView, UI/CoopMembersPlayerView, UI/CoopLobbyHelpers (inject the server broadcast with
  `GameMessage_Coop_Members(...).run(&GAMECLIENT, 0)`). The member view is not unit-tested (IsCoopMember needs a
  connection) — checked by hand.
- By hand with two real s25client under Xvfb (data/siedler/lobbytest: start.sh/start2.sh, click.sh, type.py for
  keys, shot.sh; Direct IP → Create Game → map → lobby; second client Join Game 127.0.0.1): allow, join, switch,
  Remove, start the game (free slots must be closed or AI), member orders a woodcutter site → visible on the host,
  no async. First real GUI coop game.
- Bugs found by the new ctest CoopNet_LobbySwap (host swaps slot 0 with a dummy AI while a member is there): server
  kept members on the old slot (fixed: GameServer::SwapCoopMembers, orders buffer swaps too), and members got kicked for
  their SwapConfirm (now whitelisted).
- Codex review: co-player list went stale on join/leave/swap/data changes → rebuilt there, choice preserved.
- Upstream RttR asks "Submit debug data?" at first start (sends to upstream's server) — consider switching it off or
  routing it to our fault path (M0 Windows fault hook item).
- CI on bb44a17 red: coverage gate (two never-taken `return nullptr;` in the new UI test helpers) → find_if.
- Same session, second part: Join Game window checkbox "Play the host's tribe together" (COOP_LEADER_HOST 0xFE, the
  server resolves the host's slot), explicit refusal → "Co-players are not allowed." instead of "Lost connection".
  Hand test under Xvfb: 2-player map with an AI in slot 1 (full), refused while not allowed, joined once allowed,
  game started, no async. ctest CoopNet_LobbyJoinHost. Harness race fixed (joiners wait for host.txt.connected,
  `--after`): a joiner could take slot 0 before the host's own client. Codex review: the joinhost test could pass
  without exercising the resolution (host announced before its early swap) → fixed + asserted in the host log.
- Third part: step 4 entry — "Campaign together..." button in the network map selection (dskSelectMap, 590,445) →
  dskCampaignSelection(csi) → mission → network lobby with co-players allowed (StartServer). Back from the campaign
  list returns to dskSelectMap for network csi. By hand: Roman mission 1 over the network with a co-player; both get
  the diary; closing it on the host resumed the game ("The game was resumed"), the co-player's flag showed on the host.
  UI test UI/CampaignTogetherOnlyOverTheNetwork. Codex review: nothing.
- Our local S2 copy (archive.org) has Portuguese mission names ("Lá Vamos Nós"); Jan's GOG copy will not.
- Fourth part: headless proof. coop-net `--lua`; MissionStatement without a video driver logs instead of crashing
  (IngameWindow needs loaded graphics). Local ctest CoopNetCampaign_roman_MISS200 (checkNetLobby.cmake mode campaign,
  GAME_DIR/LUA inputs, no AIs): co-player joins the host's player, woodcutter built, in sync. Codex review: nothing.
Next: release 0.1.5 once CI is green (announce to Jan: coop campaign, needs a second PC/Deck). Then step 4 leftovers:
victory/campaign progress on a co-player's client. Read, not tested: Game::CheckObjective fires EventHumanWinner in
the deterministic game on every client and SetCampaignChapterCompleted writes the local SETTINGS, so a co-player's
own machine should record the progress too — prove it with coop-net + a test script (coop-net has no --test-script
yet; port it from ai-battle). Then step 5 robustness.

## 2026-09-29 — quota filler: 0.1.5 released, co-player campaign progress
- No issues, master CI green at cb627a2. 0.1.4 had never been tagged: the release workflow now puts every CHANGELOG
  section since the last published release into the notes (`gh api releases/latest`), so v0.1.5 carries 0.1.4 too.
  Tagged v0.1.5 at e689fd5.
- coop-net `--test-script` (both processes): loaded into the map's Lua state at game start, onTestEnd(gf) called at
  maxGF between the same two GFs everywhere, then "Campaign progress: …" in the result file; exit 2 on a Lua error or
  a script without onTestEnd (Codex review). Local ctest CoopNetCampaignWin_roman_MISS200 (missionComplete.lua):
  host AND co-player record roman=2, in sync. Caveat: checksums are only compared through maxGF, i.e. up to the
  moment the win fires, not after it.
- Diary pause settled by reading: GameClient::SetPause only acts on the host, so a co-player's close just closes their
  window; the host's close resumes everyone (seen by hand earlier). Step 4 done except the campaign default in step 3.
- Step 5a: member checksums. No protocol change — every client sends exactly one command set per NWF (the first
  one right after loading), so the n-th of a member and the n-th of its leader belong to the same GF.
  GameServer::CompareCoopChecksums keeps the leader's until all members compared (max 20000 NWFs), removes a member
  that differs ("out of sync") or lags past that ("too far behind"). CoopNet_MemberDesyncDetected now expects the
  removal (member desynced at GF 300, removed at its NWF 64; host plays on). All 14 CoopNet ctests + unit tests pass
  (Test_drivers local-only as always). Codex review: claimed a loading-phase off-by-one — rejected, the leader's
  loading set passes the same addPlayerCmds path and the in-sync member tests would fail on any offset.
- Step 5b: NMS_COOP_REMOVED(reason) sent synchronously right before the server closes a member (kicked by the host,
  its player left, out of sync, too far behind); ClientError::Coop{Kicked,LeaderLeft,OutOfSync,TooFarBehind}, shown
  by every screen's CI_Error. The member's ServerLost first runs what it already received (reason and EOF usually
  arrive in the same read); ServerLost returns once stopped, so the reason is not overwritten. coop-net prints the
  error text; CoopNet_MemberDesyncDetected and CoopNet_LobbyKick assert on it. Codex review: nothing.
- Step 5c (leader hand-over) deferred, reasons in ROADMAP (host leaving ends the game anyway; campaign leader = host).
- Step 6 save/resume: coop-net --save <file> (host, at maxGF) and --savegame <file> (host continues it; slots are
  left as saved). checkNetResume.cmake + local ctest CoopNetCampaignResume_roman_MISS200: MISS200 with a co-player
  saved at GF 1500, resumed, co-player rejoins the host's player, second woodcutter, missionComplete at GF 3000 →
  both "woodcutters 2", in sync, both roman=2. Worked on the first try: upstream frees the saved human slot and our
  COOP_LEADER_HOST join finds the host. GUI: iwLoad over the network allows co-players when the save has exactly one
  human (coop::lobby::isSingleHumanSave, unit-tested in UI/CoopLobbyHelpers). GUI hand test under Xvfb (lobbytest:
  save copied into home/.s25rttr/SAVE; Map selection "Load game..." at 683,541, row 200,162, load 609,459): lobby
  showed "Allow co-players" ticked, the second client joined with the co-player box, "root +1", game started, the
  co-player saw the saved woodcutter site, no async. Note: `pkill -f bin/s25client` kills the calling shell — use -x.
  CHANGELOG 0.1.6 section written (not released). Codex review: nothing.
- CI: StyleAndFormatting failed on cb60a18 (nested ternary in coop-net: clang-format 10 vs 14 lay it out
  differently → lambda, e1ec5cd). Everything else green incl. Clang-Tidy, Windows, macOS, sanitizers. Tagged v0.1.6.
  Rule: avoid nested ternaries, local clang-format 14 does not catch CI's 10.
Next: wait for Jan's test of 0.1.5/0.1.6 on Deck/Mint; meanwhile M3 (splitscreen/gamepad: derneuere's branch) or M1 presentation, whichever Jan's
feedback points at. Jan's Deck/Mint feedback on 0.1.5 still open. Jan's Deck/Mint feedback still open.

## 2026-09-30 — work session: M3 a, derneuere's splitscreen merged
- No issues. Master CI for e1ec5cd had been cancelled by the NOTES push (again) — re-run, green.
- Merged derneuere/s25client `splitscreen-gamepad` (13 commits, ~40k lines + language catalogues moved from the
  external/languages submodule into data/RTTR/languages) via PR #2 (branch merge/splitscreen; `merge/*` branches do not
  trigger CI, a PR does). 5 conflicts (both sides kept; our IsBuildingEnabled null guard kept).
- Adaptations: `WindowManager::Close(id, owner)` / `CloseAll(id)` on newer upstream code; our co-player lobby ids moved
  to the END of the dskGameLobby enum (the splitscreen tests hard-code the ids before them — keep new ids at the end);
  ctrlEdit takes a space as Char event (upstream 3d758c8cd); iwMsgbox cannot be closed with pad B (CloseBehavior::Custom)
  — A presses its harmless button; -Werror/-Wunused-lambda-capture and 43 clang-tidy findings in the new code; 58 files
  reformatted with clang-format 10.
- clang-format 10 now available locally: /app/agent/data/siedler/cf10w/clang_format/data/bin/clang-format (PyPI wheel).
  Run it on every changed file before pushing — no more "CI formatting" commits.
- CI-only failure found: windows alive at a test's end die in the fixture destructor after the body's `bool alive`
  flags → stack smashing (gcc Debug) / access violation (MSVC). Our Release build never shows it. Lessons: tests that
  pass a stack flag into a window must keep the flag alive past the fixture; ASan does not run stably in this container
  (ASLR, `setarch -R` not permitted → endless DEADLYSIGNAL; one run filled 11 GB of log — always cap the output).
  Upstream's TestEventManager is deleted through EventManager without a virtual dtor (ASan new-delete-type-mismatch);
  upstream bug, not ours.
- Local tests: `TMPDIR=/app/agent/data/siedler/tmp` is needed, boost copy_file fails across filesystems into /tmp.
- Tested: full local ctest (82 incl. Test_splitscreen 153 cases, all Coop*), and by hand under Xvfb 1280x800:
  `s25client --map …/GreenPlains.SWD --local-players 2` → lobby "Local player 2" → two views side by side, a click in
  the right view opened player 2's action window and placed a yellow building site. Screens: data/siedler/sstest/.
- Codex review of the overlap: splitscreen is local-only (seat panel only in single-player lobbies), so it does not
  meet coop members; the `--local-players` CLI can still reach odd combinations (member client with extra slots, local
  seats on a campaign's AI slots) → M3 b/c.
- M3 c design written: doc/coop/SharedLocalViews.md (several local views on ONE player; only a few explicit guards
  block it — commands, windows and pads are already keyed right).
- Released v0.1.7 (splitscreen) — package checked: binary starts, rttr-de.mo and CHANGELOG inside.
- Branch feature/shared-views, merged to master after CI was green (one extra round: a pad-brief test demands the German
  ware word in purpose sentences — derneuere's German used English ware names; run the FULL ctest after .po changes):
  1. `GameClient::SetSharedLocalViews(n)`: n extra views on the MAIN player (no slot, no registration; commands go by
     player id anyway). CreateViews appends them without the duplicate check. CLI `--local-players 2 --share-player`.
     Test SplitscreenGameTests/TwoSharedViewsControlOnePlayer; by hand under Xvfb a site placed in the right view
     showed in both views.
  2. Lobby seat panel checkbox "Play one tribe together" (at 400,340, id at the enum end): seats become shared views,
     no slot touched; toggling stands everybody up first. Campaign (`GameClient::SetHostingCampaign`, set in
     dskCampaignMissionSelection::StartServer, cleared by Stop) and one-player maps: always together, box read-only.
     Tests MenuPadSeatTests/ATogetherSeatSharesTheHostsTribe, CampaignSeatsAreAlwaysTogether. Codex: 2 points, both
     rejected (router slot on stand-up is the existing behaviour; the lobby cannot change the map).
  3. Translations: rttr.pot lacked all our texts, so msgmerge turned new German entries into obsolete ones. Recipe for
     new texts: `xgettext --from-code=UTF-8 -k_ -kgettext_noop -k__ -C --no-wrap -f <list of libs/extras sources>`,
     `msgcomm --unique` against rttr.pot, append the new entries to rttr.pot verbatim (msgcat rewraps everything),
     build (msgmerge updates all 28 .po), German via `msgmerge -C <translated compendium>` with the build's flags.
     88 texts added, all German (Codex).
- The panel is only visible with a pad plugged in; Xvfb has no pad, so step 2 is covered by the tests only.
- Released v0.1.8 (couch coop in the lobby, German texts).
Next: M3 c
step 3 (seat colour per view in focus ring and brief stripe; road preview of shared views). Jan's Deck/Mint feedback
on 0.1.5–0.1.7 still open.
