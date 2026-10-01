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

## 2026-09-30 — Sol companion: MISS200 walkthrough reaches real victory
- Branch `sol/miss200-victory-walkthrough`, separate checkout/build, no primary-checkout or master changes.
- `miss200Walkthrough.lua` now waits for tutorial event 7, then builds and connects successive barracks towards
  the arc. Each expansion waits for a completed barracks AND territorial gain (a soldier entering); no resources,
  mission flags or direct `MissionEvent` calls are injected. The real world occupies (14,8), the mission explores
  it (16) and wins (99). Required events 1–7, 16 and 99 must appear in order; optional geologist events may interleave.
- `checkWalkthrough.cmake` requires exit 0 and exactly `Campaign progress: roman=2` followed by `TEST PASSED`.
  Its negative control disables only the mission's arc victory branch and requires exit 2 specifically for the
  missing event 99. Local S2-data gate preserved; no original map data is added to the repository.
- Built `ai-battle` in the companion's own `build/dev` (GCC 12 Release, Ninja, ccache, `-j 2`). Lua 5.2 syntax and
  static validation passed. The new positive/negative test passed on both the existing CD map and the distinct
  GOG 1.5.1 map. CD: arc occupied at GF 27554, seven barracks, normal mission resources, `roman=2`.
  All seven relevant existing tests passed: five `CoopHeadless_*`, MISS200 smoke and campaign-progress tests.
  Contract probes also reject an always-successful binary and a success result without campaign progress.
- Runtime environment here needs `USER=root`; own `RTTR_USERDATA_DIR` and `TMPDIR` keep logs and temporary files
  local to this checkout. Initial setup needed a full build; subsequent slices can reuse that local cache.
- Read-only review on exactly Sol 6.1 found no actionable defects; its suggestion to wait for event 7 before
  expanding was adopted. Opus owns review/integration of the PR. Next independent slices: campaign controller
  Back routing and the controller exit from the victory screen; shared-view presentation stays with Opus.

## 2026-09-30 — Opus: per-player start goods (Jan's request), roadmap M6/M7
- Jan's Lua script (clear HQ, add a few wares) is now a start loadout: `StartWares::Minimal` and `MinimalPlus` (the
  script's "Cheater" bonus), appended to the enum (old settings keep their values), in `nobHQ::getStartInventory`.
- Per player: `BasePlayerInfo::startWares` (std::optional, empty = game setting; serialized as 0 / value+1).
  BasePlayerInfo version 2; Savegame 4.2, Replay 8.4, each maps its minor to the player-info version
  (`SavedFile::GetPlayerInfoVersion`). New lobby message `NMS_PLAYER_START_WARES` (server/client/coop member
  ignore), `GameLobbyController::SetStartWares`, Lua `player:SetStartWares(SWR_MINIMAL)`, SWR_MINIMAL(PLUS).
- Lobby: "Goods" button per row, cycles Default → Minimal → Minimal+ → Very low → … → A lot. Host may set it for
  any row, a player for its own; locked by the map script's `general`. Network lobby: columns right of the name
  move 50 px left (name 130 px) to fit; single player keeps upstream widths. German texts added.
- Tested: Test_integration StartWaresSuite (loadout, per-player HQ, net/serialize round trip), Serialization
  (savegame + replay keep the field), Test_lua SetStartWares; new ctest CoopNet_StartWaresPerPlayer (joiner picks
  minimal via `coop-net --start-wares`, both processes report p1 with 2 boards/4 stones/0 helpers, p0 normal, in
  sync to GF 1000); full ctest. By hand under Xvfb: SP + network lobby, in-game stock exactly the script's wares.
- Codex (gpt-6.1-sol): 1 point rejected — server does not enforce the Lua lock on start goods; the server has no Lua
  state and nation/team/colour locks are UI-only upstream too (matters only against a modified client).
- ROADMAP: M6 (S3/S4 rules as add-ons, presets Classic/Comfort/Age of Gods/Dark Tribe, AI artwork) and M7 (AI HD
  graphics remake, original always selectable, free asset set) added at Jan's request.
Next: M3 c step 3. Sol PR #4 (campaign controller routes) reviewed, merged: pad suite green after merge.

## 2026-09-30 — Sol companion: campaign controller return routes
- PR #4, branch `sol/campaign-controller-navigation`: chooser B now follows the existing Back action,
  returning local campaigns to Singleplayer and network campaigns to Create game with their original context.
- Chapter/campaign victory screens accept controller input: focused Continue for A, B/Start via ShowMenu.
  Pickup and navigation leave the screen visible; mouse/keyboard exits and recorded progress are preserved.
- Driver-event regressions exercise Direct-IP, LAN and lobby chooser entry/return and all three victory exits
  for chapter and whole-campaign completion. No desktop-handler/focus injection or original S2 files needed.
- Both new bug regressions fail on the old implementation; the mouse/keyboard countercheck passes there.
  All seven campaign UI cases and the full Test_splitscreen ctest pass with the fixes (full suite: 55.16 s).
  GCC12 Release/Werror, checkout-local build/dev and userdata/temp paths, Ninja/ccache, at most two jobs;
  clang-format10 and diff checks pass. Read-only gpt-6.1-sol diff review: no actionable findings; sandbox
  source reads were unavailable. No primary-checkout/build changes, master push or self-merge.
- Documentation scopes were initially held by per-player-start-wares; acquired after its completion to
  add these notes/status on the same PR. Opus owns review/integration. Next useful independent work:
  controller Back from map selection and controller skip from the intro; the broad M3 navigation item stays open.

Next: M3 c step 3; Sol's campaign-controller-navigation PR to review.
- Integrated Sol PR #12 (online-lobby controller B) as 2a2891fde: all 17 CI jobs green on ea7f175ff; own
  Debug rerun of the six MenuPadLobbyReturn cases passes (1630 assertions). Change mirrors dskMultiPlayer B.


## 2026-09-30 — Sol companion: controller Back from Create game
- Branch `sol/map-selection-controller-back`, based on origin/master be40e1c42 (per-player start goods).
  dskSelectMap handles B through its existing GoBack action, retaining Local/Direct-IP/LAN/lobby routing
  and the disconnected-lobby fallback. Other controller buttons retain their navigation behavior.
- Driver-event tests cover B and mouse Back in four contexts, B closing the regular Load game window
  before leaving the desktop, and B retaining the custom map-settings dialog that requires confirmation.
  The initial overlay test assumed a regular close behavior for the map dialog; its actual custom/modal
  constructor invalidated that assumption, so the tests explicitly distinguish the two kinds of window.
- BReturnsToTheOriginalMenu fails on the old implementation; mouse Back and unrelated-button controls
  pass there. All five final MenuPadMapReturnTests cases pass with the fix. GCC12 Release/Werror in the
  companion build/dev, Ninja/ccache and at most two jobs; clang-format10 and diff checks pass. Final
  read-only gpt-6.1-sol review of the corrected tests/input-routing source: no actionable findings.
- Existing SecondLocalPlayerCommandsTakeTheFullNetworkRoundtrip already records and replays through
  the real GameClient with the mock video driver, checking replay async/error callbacks and completion.
  The older M0.5 roadmap sentence and duplicate Sol backlog task were stale; both corrected. A negative
  checksum control for that real-client replay path is a separate useful ready slice.
- Full Test_splitscreen ctest also passes (54.07 seconds).
- Opus owns review and integration; no primary checkout/build changes, master push or self-merge.

## 2026-09-30 — Sol companion: real-client replay checksum negative control
- Branch `sol/gameclient-replay-checksum-negative-control`, based on origin/master d0be9ed1a.
  New ClientReplayChecksumTests records a real loopback game with an actual military-setting order,
  copies its players/settings/seed/map/commands through Replay's serializer, and changes exactly one
  valid object-count checksum field in one copy (the nonzero RNG marker remains valid).
- Clean re-encoded control reaches the same GF and full AsyncChecksum as the recorded game and applies
  the military-setting order. The corrupted copy produces exactly one CI_ReplayAsync, pauses after its
  offending GF, clears the skip target, does not report normal completion and stays paused across three
  real frame intervals and further client pumps. No original S2 files, direct game-state injection,
  replay-error callback injection or byte-offset guesses; compressed recordings use the normal reader.
- Regression passes (2.45 s). A mutation that disables the production checksum condition fails specifically
  on the missing replay async callback (exit 201); original GameClientGF_Replay.cpp was then restored
  byte-for-byte and rebuilt. The checksum verifier is unchanged in the final branch.
- Full-suite execution exposed a stopped-replay lifecycle bug: Stop cleared replayinfo but retained
  replayMode, and a subsequent controller view crashed in IsReplayFOWDisabled. Stop now clears the mode
  after releasing metadata; tests explicitly stop both pristine and corrupted playback and check that
  replay mode is false and the FOW query is safe before the next client/view operation.
- New fixture initializes the mock GUI driver before LocalGameFixture constructs its settings-dependent
  GameManager. As the first registered test, reversing that initialization creates default driver names
  before the mock driver exists and causes a later Options return test to request a driver restart.
- Read-only gpt-6.1-sol review found the fixed 50-pump persistence check could theoretically run before a
  future frame deadline. Adopted: pump through three real GetGFLength intervals, then check GF/callbacks.
  GCC12 Release/Werror, own build/dev and userdata/tmp, Ninja/ccache, at most two jobs; clang-format10
  and diff checks pass. Opus owns review/integration; no primary checkout/build edits or master push/merge.
- Final combined replay/controller/Options checks and the full Test_splitscreen ctest pass (56.05 s).
  Final gpt-6.1-sol fixture/lifecycle review found no actionable issues.

## 2026-09-30 — Sol companion: controller Back from Options
- Branch `sol/options-controller-back`, based on origin/master af5da6dfe. B calls the existing Back
  action, so settings persistence, local/proxy port validation and video/audio driver warnings remain
  on one code path. Start remains inert; shared input routing is unchanged.
- Five new mock-video-driver regressions exercise B from another focused control with a keyboard-edited
  player name saved and reloaded from temporary userdata, open portrait dropdown cancellation without
  committing its tentative selection, and the Music player overlay closing before Options can leave.
  Invalid local AND proxy ports retain their custom confirmation dialog; B cannot dismiss it. After A
  acknowledges the error, physical-keyboard correction followed by B saves and leaves successfully.
- With the new tests but the old implementation, four cases fail precisely at Options return or port
  validation; existing three cases and Start countercheck pass. All eight MenuPadOptionsTests cases
  pass with the fix. No direct desktop-handler, focus-state or controller activation injection.
- GCC12 Release/Werror, own build/dev and temporary userdata, Ninja/ccache and at most two jobs.
  clang-format10 and diff checks pass. Exact gpt-6.1-sol read-only review of diff plus input/Back context:
  no findings. Opus owns review/integration; no master push, self-merge or primary checkout changes.
- Full Test_splitscreen ctest passes (all 8 Options cases included; 56.93 s).

## 2026-09-30 — Sol companion: initial and cleared table selection
- Branch `sol/table-initial-selection`, based on origin/master fbe3a4ab9. ctrlTable now starts with
  std::nullopt instead of the engaged unsigned -1 sentinel. Both keyboard/controller arrow paths
  share MoveSelection: first Up or Down selects row zero in a nonempty unselected table, including
  after DeleteAllItems/reload; empty tables remain unselected. Already selected movement is unchanged.
- Six new driver-event/WindowManager keyboard+mouse regressions cover initial selection/activation,
  Up/Down entry with both input devices, clear/refill, empty-to-one-row lifecycle with pad focus acquired
  normally, mixed-input navigation and clamping over 40 rows, scroll visibility, and mouse selection
  followed by keyboard/controller movement and A activation. No direct selection/focus/activation
  injection. The custom desktop records the real table callbacks.
- Four new cases fail on the old implementation at the initial-selection, first-Up, clear/refill and
  empty-table assertions. Existing pure-control activation test's sentinel assertion was corrected;
  obsolete sentinel comments removed from that file and existing campaign driver tests.
- All six new cases pass. Full Test_splitscreen (58.73 s) and full Test_UI (1.55 s) pass together.
  Core and driver tests: GCC12 Release/O3/Werror; UI-only generated compile blocks temporarily used O1
  with Werror because unchanged testComboBox.cpp trips Boost 1.74/Turtle basic_cstring::rfind's
  -Warray-bounds at O3. No source/CMake/third-party workaround committed; build.ninja restored afterward.
  At most two jobs, own build/dev/userdata/temp paths. clang-format10 and diff checks pass. Exact
  gpt-6.1-sol read-only patch/control/fixture review: no actionable findings. Opus owns integration.

## 2026-09-30 — Sol companion: controller skip from Intro
- Branch `sol/intro-controller-skip`, based on origin/master d47547fe0. Intro accepts pad input;
  A/B/Start use its existing idempotent finish path. Missing-video page retains its focused Back button.
  MenuPadInput offers A to the desktop only without a focused control AND without a top window, so a
  controls-free video can accept confirm. Focused-control activation and window ownership are preserved.
  Intro explicitly refuses Start while a window is present, preventing confirmation bypass.
- Seven mock-video-driver cases cover missing-video A/B/Start, pickup swallowing, a multi-button skip
  burst with one transition and no destination click/command, regular overlay-first B, required dialog
  acknowledgement before skipping, mouse/keyboard counterchecks, and controls-free A fallback with
  focused-control/window precedence. The optional seventh case decodes the actual original intro and
  exercises all three controller exits; RTTR_COOP_S2_DIR is supplied locally, originals never committed.
- With the tests and old implementation, six cases fail at pad acceptance/transition/overlay or
  controls-free A fallback; mouse/keyboard countercheck passes. Original movie was also exercised in
  that negative run. Exact gpt-6.1-sol read-only patch/fixture/routing/intro review: no actionable findings.
- All seven cases pass with originals, all six unconditional cases pass without them, and full
  Test_splitscreen with the original-video case enabled passes (59.96 s). GCC12 Release/O3/Werror,
  own build/dev and temporary game path; at most two compiler jobs. clang-format10/diff checks pass.

## 2026-09-30 — Sol companion: controller Back from player menus
- Branch `sol/player-menu-controller-back`, based on origin/master 0eff660d7. Singleplayer and
  Multiplayer handle B through their existing visible Back actions, returning to Main menu. Start
  stays inert and shared input routing is unchanged.
- Six mock-video-driver regressions cover B from every focused action (six Singleplayer buttons,
  four Multiplayer buttons), Play Replay and lobby-login windows closing before the underlying menu,
  the real missing-save error retaining required confirmation, Start/navigation counterchecks, existing
  mouse/controller-A Back routes, and a queued B burst plus a fresh B remaining on Main menu without
  activating an action or Quit. The test checks GLOBALVARS.notdone, the actual flag cleared by Quit,
  immediately and a frame later, and presses B with Main-menu Quit focused. The fixture restores
  the flag afterward. Userdata is temporary; no external lobby connection is attempted.
- Four new cases fail old code at the return-to-Main assertion; existing Back via mouse/A and inert
  Start/navigation counterchecks pass. No direct handler, focus or control-activation injection.
- All six final cases pass, including the direct quit-flag assertions. Full Test_splitscreen passes
  (61.22 s). GCC12 Release/O3/Werror, own build/dev and userdata, Ninja/ccache and at most two jobs.
  clang-format10 and diff checks pass. Read-only exact gpt-6.1-sol reviews: no concrete handler/routing
  bugs; follow-up with Main menu, frame/deferred-switch and pending-button source also finds no issues.
  No primary checkout/build changes, master push or self-merge; Opus owns integration.

## 2026-09-30 — Opus: Sol PRs #4–#10 integrated, master CI repaired, 0.1.9
- Merged Sol PRs #4 (campaign chooser B/victory exits), #5 (map selection B), #6 (Stop ends replay mode),
  #7 (Options B), #8 (tables start unselected; campaign Next guarded), #9 (intro skip), #10 (player menus B).
- Master CI broke several ways that Sol's local Release/GCC 12 runs cannot see — fixed on master:
  `Settings persisted;` in a test ended the SETTINGS singleton (Debug assert at exit, every Linux job);
  optional == value inside BOOST_TEST needs double parentheses on GCC 9 / Boost 1.73; the gcc-10 coverage gate
  needs LCOV_EXCL on unreachable test lines; clang-tidy (exception-escape in fixture destructors, branch-clone,
  qualified-auto vs MSVC iterators in my own lobby code). Sol was told all of it via the report API.
- Local Debug build dir `build/dbg` (Test_splitscreen only) reproduces Debug-only asserts; keep it.
- Released v0.1.9 (per-player start goods incl. Minimal/Minimal+, controller Back/skip everywhere) from 2572261e2
  after every CI job was green.


## 2026-09-30 — Sol companion: controller Back disconnects from the online lobby
- Branch `sol/lobby-controller-back`, initially based on 2572261e2 and brought up to current master
  6c0ba04a6 for documentation. B invokes the existing disconnect/Back action; overlays keep precedence.
- Six regressions use driver pad events and physical keyboard/mouse. All five focusable controls,
  Create's explicit Back, required proxy confirmation, regular server-info B/Escape cancellation,
  mouse/A Back, edited chat through inert Start/navigation and B bursts are covered. The parent stays
  Multiplayer until a new B press; the real LobbyClient is logged out and its TCP connection closed.
- A test peer listens only on 127.0.0.1 with an ephemeral port. The actual singleton authenticates,
  receives both lists and decodes server-info replies through its production network path; no public
  lobby service or direct client/UI handler, focus, activation or selection-state injection is used.
  The wire header is little endian, while Serializer payloads are big endian. Four-byte peer reads
  exercise partial headers/bodies. EOF/reset are both valid shutdowns when list replies are unread;
  at most one protocol Dead is observed. No singleton is constructed; Stop cleanup is explicit in
  tests/entry, and the fixture destructor restores only a saved proxy enum reference.
- Five old-production cases fail specifically at missing Multiplayer return (exit 201), while
  mouse/A Back succeeds. All six fixed cases pass (0.81 s); full Debug Test_splitscreen passes (84.11 s).
  GCC12 Debug/Werror, own build/debug, ccache, at most two compiler jobs. Static validation,
  clang-format10 and diff checks pass. Exact gpt-6.1-sol read-only patch/context review found no
  concrete bugs or flaky assumptions; repository sandbox access was unavailable, so runtime and CI
  are independent checks. Optional seat comparisons use explicit parentheses for GCC9/Boost1.73.
- PR remains a draft until every branch CI job passes. Opus owns integration; no primary checkout,
  build, master push or self-merge. Next useful independent slice: replay-browser controller flow.
- CI follow-up (forwarded by Opus): gcc-10 coverage on 7b6774a reported one unexecuted reachable
  helper fallback at line 222. Every exercised focus target is reached from its known entry point
  with RightShoulder; removed the unused reverse retry instead of marking it LCOV-excluded. No
  production behavior or test cases changed. Rebuilt Debug Test_splitscreen and all six affected
  MenuPadLobbyReturnTests pass again (0.80 s), including singleton shutdown assertions. Formatting,
  static validation and diff checks pass. Prior full Debug suite passed 84.11 s; fresh branch CI,
  including coverage and Clang-Tidy, remains required before handoff.
- Clang-Tidy follow-up: `expectLobby()` only observes the static desktop accessor, real client
  and loopback peer; made it const to satisfy readability-make-member-function-const. Debug
  Test_splitscreen rebuilt with two compiler jobs; all six MenuPadLobbyReturnTests pass with
  1,630 assertions (0.75 s). Static validation and diff checks pass. New-head CI must all pass
  before PR #12 is handed back as tested.
- Reconciled NOTES/ROADMAP with master 31e92b247 after Opus integrated PRs #11/#13/#14;
  retained both documentation sides. Lobby implementation and its tests are byte-identical
  to c6fcad5a2. Reconfigured/rebuilt Debug with two jobs and ran the full combined suite:
  all 426 cases and 36,772 assertions pass, with USER=root and TMPDIR on the data mount.
  Static validation/diff checks pass. All CI is required again on the resulting merge head.
## 2026-09-30 — Sol companion: controller Back from network menus
- Branch `sol/network-menu-controller-back`, based on origin/master 2572261e2. Direct-IP and LAN B
  invoke the existing Back action to Multiplayer; Start and shared input routing are unchanged.
- Eight regressions inject mock-driver pad events and physical keyboard/mouse input. They cover all
  seven focusable desktop controls, Join closing before the desktop leaves, Create retaining its
  explicit Back requirement, required proxy-warning confirmation, inert Start/navigation, mouse/A
  Back, keyboard Escape parity and B bursts staying at Multiplayer until a new press.
- A synthetic listed LAN row makes the table focusable; this checks Back, not discovery/Connect.
  Empty tables intentionally cannot take focus. Shoulder navigation goes both ways, since the
  existing focus list does not wrap. No direct handler, focus or selection-state injection.
  Userdata is temporary; no public lobby connection. The proxy fixture restores an existing enum
  reference, avoiding a singleton lookup in its destructor; no singleton is constructed in tests.
- All eight cases pass in Debug. Rebuilding the four original production files makes five cases
  fail specifically on the missing return, while mouse/A, keyboard and inert-navigation controls
  pass (exit 201, 27 failed assertions). Final production files restored byte-for-byte and rebuilt.
  Full Debug Test_splitscreen passes (80.21 seconds), including the final eight cases.
  After adding explicit parentheses to the optional seat comparison for Boost 1.73, it passes again (79.32 s).
- GCC12 Debug/Werror, own build/debug, ccache and at most two compiler jobs. Static validation,
  clang-format10 and diff checks pass. Exact gpt-6.1-sol read-only final patch/context review found
  no actionable issues; its repository sandbox could not initialize, so runtime verification is
  the independent local run. CMake reconfigured after adding the test (add_testcase's source glob
  does not track new files automatically).
- Branch CI must finish before handoff as tested; Opus owns review/integration. No primary
  checkout/build changes, master push or self-merge. Next independent menu slice: online-lobby Back.

## 2026-09-30 — Sol companion: safe text credits and controller Back
- Branch `sol/credits-controller-back`, based on origin/master 6c0ba04a6. Credits now opts into pad
  input; B follows the existing Close action to Main menu. Start stays inert and shared routing is
  unchanged, including regular-window and required-confirmation precedence.
- Driver-event entry from Main menu exposed a real resource-failure crash: the iterator initialized
  to the empty entries vector's end became invalid when the vector grew, then early resource-load
  failure left it dereferenced in DrawCredit. A local Debug/gdb trace confirmed the null access through
  glFont::Draw. Credits now sets the iterator after the final entry and keeps text browsing available
  without optional world/game graphics; decorative bobs are drawn only after their assets load.
- Seven physical-input regressions cover Main-menu entry/B return, regular window first, custom
  acknowledgement required before B, inert Start/navigation, mouse/A Back, B bursts and Main-menu
  Quit safety. Isolated missing GAME data and missing world data remain browsable through 40 forward
  keyboard and 40 backward mouse page changes (including wrapping), a background left click and B.
  No singleton construction, focus injection or direct desktop-handler calls; runtime resources remain
  local, original game data is neither needed nor committed. Fixtures restore only captured state.
- All seven targeted Debug cases pass. Exact gpt-6.1-sol read-only patch/context review found no
  actionable iterator, fallback, routing or overlay bug; repository inspection was blocked by its
  sandbox, and it did not independently verify rendering or resource-cache isolation. Local tests
  and CI are the runtime gates. Countercheck and complete Debug suite evidence are recorded below.
- Without only the B handler (safe text fallback and pad opt-in retained), five cases fail specifically
  at return-to-Main; Start/navigation and existing mouse/A Back pass. Final seven targeted cases pass,
  and full Debug Test_splitscreen passes (88.34 s). Static validation, clang-format10 and diff checks
  pass. All branch CI jobs remain required before tested handoff; Opus owns integration.

## 2026-09-30 — Sol companion: real member cannot claim an extra local slot
- Branch `sol/coop-member-local-slot-policy`, based on origin/master 6c0ba04a6. M3b policy is to reject
  additional distinct player slots on a network member; shared local views remain a separate M3c item.
  The existing GameClient guard is unchanged. coop-net gains `--extra-local-slot`, installed in the
  connection-finished callback before subsequent start messages (Connect clears pending requests), and reports whether loading/started callbacks
  fired when LocalPlayerSetup is returned. Host mode rejects the test option as a setup error.
- Two real localhost host/member CTest cases use the same topology: host slot 0, spare dummy-AI slot 1,
  member of player 0. A normal member finishes GF 1000 in sync. Requesting the otherwise valid slot 1
  exits specifically with LocalPlayerSetup before either loading or starting; the rejected member has
  no game result, the host removes it and finishes GF 1000 with checksums compared through that frame.
  Actual starting member roster and spare-slot start goods are asserted; unrelated failures cannot pass.
- Both Debug cases pass (22.32 s). Negative control removes only the request from a scratch checker:
  both processes then succeed (0;0), and the refusal checker fails at the intended LocalPlayerSetup
  expectation. The checked-in harness/guard are unchanged by that countercheck. Existing real-member
  regressions and branch CI are recorded in the PR. Own build/debug, GCC12/Werror, at most two jobs.
- Read-only exact gpt-6.1-sol review found a possible lobby/start race in initial harness instrumentation;
  fixed by setting the request in the connection-finished callback rather than after Run returns.
  A per-port process lock also covers concurrent invocations from different build directories.
- After the callback fix, both cases pass three times each (65.86 s); negative control again fails at
  the expected refusal assertion (0;0 without the request). Existing MemberOrders, MemberAndSecondPlayer,
  MemberFallsBehind and MemberDesyncDetected all pass (48.78 s). Review confirmed the callback ordering
  fix; port lock wait now covers a complete prior invocation and its CTest timeout allows both runs.
  Static validation, clang-format10 and diff checks pass. All branch CI jobs are required before handoff.

## 2026-09-30 — Opus: Sol PRs #11, #13, #14 integrated
- Reviewed the diffs (Direct-IP/LAN B → existing Back, Credits iterator fix + text-only fallback + B → Close,
  coop-net `--extra-local-slot` member refusal test). No objections; all 17 CI jobs were green on each head.
- Merged into master (NOTES conflicts only, both sides kept). On the merged tree, Debug: full
  Test_splitscreen passes (80.3 s) and all CoopNet_Member* incl. MemberLocalSlots_normal/extra pass.
  Local run needs `USER=root TMPDIR=/app/agent/data/siedler/tmp` (see above).
- PR #12 (lobby Back) still waits for its CI before handoff.

## 2026-09-30 — Sol companion: music playlist changes reach playback
- Branch `sol/music-playlist-controller`, based on master 91676d5c8; separate from Create Game PR #15.
  A successful track Up/Down now marks the playlist changed, so closing applies the new order to
  active playback as well as saving the file. Merely opening/cancelling Add Track or Add Directory
  no longer marks it changed; confirmed input still does so at the existing Msg_Input path.
- Nine cases enter the real music window from Options using driver pad events. They cover isolated
  Up/Down reorders, missing-selection/top/bottom no-ops, selecting a song and removing it, playlist
  dropdown cancellation before window close, physical-text input cancellation/confirmation for
  both track and directory additions, repeat/random controls and saved playlist contents.
  Songs are symbolic built-in ids, exercising the real player queue/current-song state without
  claiming audible playback. A queued dummy .ogg path is never reached or decoded.
- Old production fails Up/Down active-playlist/current-song assertions and cancellation's no-start
  assertion (three cases, six assertions); four initial counterchecks pass. Seven initial fixed
  cases pass, then all nine final cases pass with 245 assertions. New test glob reconfigured and
  the suite is present in the binary. Own GCC12 Debug/Werror build/debug, at most two compiler jobs.
- Read-only exact gpt-6.1-sol review found test-state leakage on early assertions/exceptions. Cleanup
  now runs in a test-body wrapper on success and exception, never in the fixture destructor; an
  exception probe proves the production playlist is restored and covers that helper path. The
  destructor only swaps the captured settings string. No second singleton is constructed.
- Full Debug Test_splitscreen passes all 435 cases and 37,013 assertions. clang-format10, static
  validation and diff checks pass. All current-head branch CI is required before tested handoff;
  Opus owns integration.

## 2026-10-01 — Sol companion: Addon Settings controller policy regressions
- Branch `sol/addon-controller-policies`, from origin/master 91676d5c8. Bounded to a new
  `testMenuPadAddons.cpp`; no product behavior, shared input, presets or M3 view changes.
- Eight physical-input cases enter the real Options and connected local lobby addon windows.
  They cover Apply and persisted configuration, Abort, deliberate custom B/right-click refusal,
  category/reset-scroll with pending edits, dropdown B cancellation, read-only and whitelist rules,
  Default preserving locked non-default values, and exception-safe singleton-state restoration.
- All/AllAndSaveToConfig use actual controller entry. None/WhitelistOnly instantiate the real
  addon window with the connected lobby parent, explicitly isolating policy enforcement from
  campaign Lua policy selection; campaign script routing is not claimed. Apply clears only the
  local settings after its serialized message is queued, then requires the real server broadcast
  to restore them, so the window's immediate local edit is insufficient to pass.
- The fixture uses temporary userdata and references the real Settings/GameClient. Options is
  destroyed before swapping the captured addon configuration back, including a deliberate throw.
  Saved INI assertions parse the exact addon section/key/value. Both shoulder directions traverse
  the visible controls to their endpoints and prove locked controls never receive focus.
- Final own Debug/Werror build (max two jobs): all 8 cases / 465 assertions, then complete
  `Test_splitscreen`: all 434 cases / 37,233 assertions. Deliberate missing-callback/unlocked-policy
  counterchecks are recorded in the PR/checkpoint. Initial read-only gpt-6.1-sol review found setup
  outside the cleanup guard, weak substring persistence and one-direction traversal; all tightened.
  Save/Load preset flows remain a separate bounded task. Branch CI must all pass before handoff;
  Opus owns review and integration. No primary checkout/build, master push or self-merge.

## 2026-09-30 — Sol companion: cancel Create Game from the controller
- Branch `sol/create-game-controller-cancel`, based on master 91676d5c8 after PR #12 integration.
  Create Game uses the existing NoRightClick cancellation policy: B, Escape, Alt+W and the title
  close button discard the form through the same Close method as visible Back. Right-click stays
  inert; shared input routing and required custom confirmations are unchanged.
- Seven driver-event/physical-input regressions cover both Direct-IP and LAN, all four pad focus
  targets, keyboard focus in each of three text fields, invalid name/port, valid Start into map
  selection, mouse/A Back, title close, Escape/Alt+W, regular/custom modal overlays, inert Start
  and navigation, right-click, and B bursts stopping at the original network desktop.
  The original parent must survive without map-selection transition and the real client remains
  Stopped; tests never construct another singleton or call UI handlers/focus/activation directly.
- Updated the existing network and real-loopback lobby tests' explicit old Custom-close expectations
  in the same slice. The online lobby remains authenticated after form cancellation and its proxy
  warning still needs confirmation. Text fields intentionally cannot take pad focus; their keyboard
  input is tested through physical mouse focus and key events. New CMake source glob reconfigured.
- All 21 affected cases pass (2,278 assertions). Full GCC12 Debug/Werror Test_splitscreen passes
  all 433 cases. At most two compiler jobs, own build/debug, USER=root and same-mount TMPDIR.
  Original production dialog makes six new and three existing cases fail specifically at missing
  cancellation (exit 201, 63 assertions); valid Start still passes. Final source restored and rebuilt.
- clang-format10, static validation, diff checks and agent TypeScript check pass. Exact gpt-6.1-sol
  read-only review requested title-close coverage (added); final review finds no actionable bugs.
  Stopped is corroborated by the same parent and absence of map-selection transition; Close only
  queues removal and saves window state. All branch CI jobs are required before tested handoff.

## 2026-10-01 — Sol companion: safe scrolling in long text windows
- Branch `sol/controller-text-window-scroll`, based on master 91676d5c8. Readme, Help and Changelog
  already support controller scrolling through their nested scrollbar; no focus/router change is
  needed. Six driver-event and physical mouse regressions exercise long texts, both bounds without
  losing focus, inert A/Start, wheel/arrows sharing pad scroll position, short texts with no focus
  but working B, required confirmations, and clear/refill/resize behavior. Readme and Changelog
  are entered through the real Main menu with generated temporary installed files.
- The original resize case SIGSEGVs after the scroll position reaches the bottom of a narrow/small
  area and the text area grows: RecalculateSizes moves the slider but leaves scroll_pos beyond the
  new last valid page, so drawing that page extends past drawLines. Debug/gdb confirms ctrlMultiline::Draw_ -> glFont::Draw -> UTF-8 decode on
  an invalid string. Clamp scroll_pos whenever range/page changes while a scrollbar remains visible;
  the existing hidden-state reset remains unchanged. No parent callbacks added to size/range updates.
- One control regression covers range shrink/page growth, preservation when range grows and all
  visible/hidden transitions. Six targeted Debug cases pass; full Debug Test_splitscreen passes
  all 432 cases. Full Debug Test_UI passes all 137 cases/20,671 assertions; ControlActivation passes
  17 cases/255 assertions. GCC12 Debug/Werror, own build/debug and at most two compiler jobs.
- Exact gpt-6.1-sol read-only review found the resize assertion allowed an unintended jump to zero:
  tightened it to the exact old-bottom/new-maximum clamp and proved the maximum shrinks. No concrete
  product/lifetime/config-isolation issue found. No second singleton; fixture destructor string swap.
  All current-head branch CI is required before tested handoff; Opus owns integration.

## 2026-10-01 — Opus: Sol PRs #15 and #17 integrated
- Reviewed diffs (Create Game now NoRightClick: B/Escape/title close discard like Back; scrollbar clamps
  scroll_pos on range/page change). Merged both, combined CHANGELOG/ROADMAP/NOTES, rebuilt build/dev
  (Release): Test_splitscreen and Test_UI pass on merged master 39a64fa61. Pushed.

## 2026-10-01 — Sol: in-game controller save regressions
- Seven new cases enter Save through the actual Back system ring, Main selection and Options
  controller buttons in a single-view local GameServer/GameClient game. Inputs enter through
  the mock driver and dskGameInterface::UpdateInput; WindowManager handles physical mouse/key
  input and window lifetime. The existing TestableGameInterface suppresses rendering, not input.
  Text fields remain keyboard/mouse-only; pad focus is observed and never assigned by a test.
- Saves are loaded with all game data and compared byte-for-byte against the actual running
  world snapshot and exact GF. Cases cover a trimmed filename, keyboard Enter, pad row selection
  and replacement at a later GF, empty/reserved-name warnings requiring acknowledgement even
  after two B presses, B focus release then close, Escape/right-click close and re-entry, and
  autosave browse/cancel/confirm without writing the pending filename. B cancellation preserves
  an existing valid save. No product code change or overwrite-confirmation policy change.
- Initialization and explicit desktop cleanup wrap the entire test body on normal/exceptional
  exits; an executed throw probe verifies desktop destruction before backend teardown and
  restoration of autosave/debug settings. Temporary userdata, no duplicate singleton. A method
  named setup caused unintended Boost auto-initialization during development; beginGame avoids
  that and keeps the observed desktop and saved backend on the same game instance.
- A suppressed production SaveToFile call fails exactly the two saving cases at actual file
  loading, with the five cancellation/settings/cleanup cases passing. Production is restored
  before final validation. Exact gpt-6.1-sol read-only review tightened the warning test to two
  B presses; final actual-source review reports no concrete findings.
- Own GCC12 Debug/Werror build/debug, at most two compiler jobs: all seven cases pass
  386 assertions; full Test_splitscreen passes all 446 cases and 37,906 assertions. CMake
  reconfigured and new suite presence verified. clang-format10, static validation, diff checks
  and the agent TypeScript gate pass. Branch CI evidence is recorded in the PR/checkpoint;
  all jobs must pass before tested handoff. Opus owns review and integration.

### Sol save-dialog CI coverage repair (2026-10-01)
GCC10 coverage found two unexecuted test lines: an unused second-page ring-navigation fallback
and deleting an existing filename. The only ring target used by this fixture is Main selection
on the first page, so the helper now searches that page directly. The saving regression first
enters a placeholder filename and physically replaces it before saving the actual snapshot,
executing the Backspace path. No LCOV exclusions and no product changes. Fresh own Debug:
all seven affected cases / 392 assertions pass; clang-format10, static validation, diff checks
and agent TypeScript gate pass. All 17 CI jobs on the repaired head remain required.

## 2026-10-01 — Sol: optional campaign artwork fallback
- Campaign selection now resolves an optional preview only when its row is selected. It uses
  a bitmap only after that specific file loaded successfully; equal-stem filenames in the
  global Loader cache cannot turn a failed preview into another campaign's image. Missing,
  corrupt or resource-id-incompatible images show a centered text fallback, without blocking
  the campaign list or previews of other campaigns. Description/Continue remain available.
- Three physical-controller regressions generate nine isolated campaigns with real maps/Lua:
  absent/missing/corrupt images, valid BMP artwork, invalid short/long basename metadata,
  valid/corrupt equal-stem files in different folders, and an actual selection-map preview.
  Cases verify fallback/description state, later valid artwork, controller continuation and
  Back/re-entry, repeated cache-collision switching and map-control creation/removal while
  the fallback stays hidden for a selection map. No original game data or second singleton.
- Read-only exact gpt-6.1-sol review found cache collisions and invalid-name exceptions in
  the first eager-loading approach, plus missing selection-map coverage. All are addressed
  by lazy checked loading and expanded tests. A second actual-source review found Windows
  Lua path escaping: generated paths now use generic_string, with the writer const-qualified.
- Original production fails all three executed artwork cases; fixed source is restored and
  rebuilt. Own GCC12 Debug/Werror: all three cases pass 1,833 assertions; full Test_splitscreen
  passes all 442 cases and 39,353 assertions. Timer waiting always executes a physical frame
  and wait body, so fast hosts do not leave unexercised test lines. New suite presence verified.
  clang-format10, static validation, diff checks and the agent TypeScript gate pass. At most
  two compiler jobs; exact-head CI evidence goes into the PR/checkpoint. All CI jobs must pass
  before tested handoff. Opus owns review and integration.

### Sol campaign-artwork CI coverage repair (2026-10-01)
GCC10 coverage reported the do/while's synthetic `do` line and an unused backwards-focus loop.
Use an ordinary bounded for loop for loading. After returning from each mission chooser, assert
table focus, physically move to a later control, then navigate backwards to the table; that now
executes and proves the intended shoulder path. No LCOV exclusions or product changes.
Fresh affected Debug: 3 cases / 1,881 assertions pass before reconciling integrated master.
After reconciling master abd4cd961 (integrated music/addon PRs #16/#18), own campaign source/tests
are byte-for-byte unchanged. Both documentation sides retained. Fresh Debug affected: 3 cases /
1,881 assertions; complete merged Debug: 459 cases / 40,103 assertions pass. All17 checks must
pass at the resulting merge head before tested handoff.


## 2026-10-01 — Sol: construction-order controls and addon-safe defaults

Claimed `build-order-controller`, starting from master 69888fe49 in Sol's own checkout; no shared
adapter/router, view or primary-checkout edits. Physical Back -> system ring -> Main selection ->
Building sequence enters the real window in a singleton-backed loopback game. Twelve cases cover
all eight wine/leather/charburner combinations, list/preview selection, Up/Down/Top/Bottom and
boundary moves, Default and reopen, custom-order browse/cancel/confirm and both flag directions.
They inspect the full build-order array and flag in the actual world after at least two NWFs,
including cancellation, rather than relying on the visual copy. Lobby setup also restores only
its eagerly changed local copy and awaits the real server broadcast, then checks the running GGS.

Default previously refilled exactly 31 rows, truncating enabled addon buildings. It now iterates
all active entries. Replay's mode dropdown is read-only and its callback also rejects mutation;
list browsing updates only the preview, including after recorded order changes. A generated real
recording proves replay completion without desync, physical mouse/controller reorder rejection,
readonly dropdown focus exclusion and actual recorded world/UI updates. An explicit idempotent
Loader building-placeholder seam supplies distinct textures without original S2 files. Its header
required a one-time wider Debug rebuild, kept at two jobs and in Sol's cache.

The whole body, including initialization, is guarded on normal and exceptional exits; an executed
throw probe proves window/desktop destruction before backend shutdown and settings restoration.
Exact gpt-6.1-sol read-only review found desktop activation replacing the backend observer and
missing replay input attempts: observer restored after activation/every input frame, mouse button
attempts and full shoulder traversal added. Final actual-source review found no concrete issues.

Old production was compiled before the fix: all seven addon-enabled Default cases and the replay
case fail (8/12, 21 assertions); classic Default, all moves, cancellation and cleanup controls pass.
Final production rebuilt: affected Debug 12 cases / 5,515 assertions pass. Complete Debug Test_splitscreen: 451 cases / 43,067 assertions pass.
CMake reconfigured and new suite presence verified. clang-format10, static validation, diff checks
and the agent TypeScript gate pass. Draft checkpoint until every exact-head CI job passes; Opus
owns review/integration, and only fully tested PRs enter completedPRs.
After reconciling master abd4cd961, all four construction-order/placeholder source and test files
are unchanged; both documentation sides are preserved. Fresh affected Debug12 cases / 5,089
assertions and complete merged Debug468 cases / 43,585 assertions pass. The assertion count
varies with GUI/initialization paths; case counts and all actual world/replay assertions pass.
All17 fresh checks on the resulting merge head are required before tested handoff.

## 2026-10-01 — Sol: build-order fixture explicit settings cleanup
- Remove singleton access from BuildOrderPadFixture's destructor. Restore debugMode through
  explicit cleanup on both exits; verify restoration before fixture destruction on normal and
  exceptional paths. Replay's in-body transition tears down only the desktop, retaining the
  temporary debug setting until final cleanup. No product changes or coverage exclusions.
- Own Debug/Werror build-order suite: 12 cases/5,121 assertions pass, including real replay
  input policies and exception cleanup. Formatting, static validation, diff checks and agent
  TypeScript gate pass. Await all17 new-head CI jobs before handoff.

## 2026-10-01 — Sol: music playlist Clang-Tidy repair
- Mark the fixture's tracks() accessor const; this resolves the CI readability warning without
  changing playlist behavior. Merge master 69888fe49, retaining both sides of documentation conflicts.
  The music production source is unchanged; its test differs only by the accessor qualifier.
- Reconfigure CMake to include the integrated Create Game and text-window tests. Own Debug/Werror
  music suite: 9 cases/245 assertions; full merged suite: 448 cases/37,765 assertions.
  Static validation, diff checks and agent TypeScript check pass. Fresh exact-head CI is required
  before tested handoff; PR #16 stays draft until every job, including Clang-Tidy, succeeds.
## 2026-10-01 — Sol: reconcile Addon policy CI checkpoint
- All 17 jobs passed at 9a0ced87f3c1cbb7b62fd9ebbd3423b0d6c59e39, but master integration
  caused documentation conflicts. Merge master 69888fe49 and retain both notes and roadmap entries.
- The Addon test source is byte-identical to the previous tested head. Reconfigure the test glob,
  rebuild own Debug with at most two jobs, and rerun all 8 Addon cases/465 assertions successfully.
  Agent typecheck passes. Await all jobs at the new merge head before handoff.

## 2026-10-01 — Opus: integrated Sol PRs #16 (music playlist) and #18 (addon policies)
- Both handoffs verified: Unit tests + Static analysis green at a3452632a (#16) and 392ed7aa5 (#18).
  #16 production change reviewed: Up/Down mark the playlist changed only on an actual swap; opening
  Add Track/Directory no longer does (confirmed input still sets it in Msg_Input). #18 is test-only.
  Merged both; NOTES/ROADMAP conflicts resolved keeping both sides. Release build of Test_splitscreen
  on the merged master passes; pushed.
After reconciling integrated master abd4cd961, the repaired save test is unchanged, and both
notes/roadmap sides are preserved. Fresh affected Debug7 cases / 392 assertions and complete
merged Debug463 cases / 38,614 assertions pass. All17 fresh merge-head CI jobs remain required.

## 2026-10-01 — Sol: save fixture explicit settings cleanup
- Remove singleton access from SavePadFixture's destructor. Restore debugMode in the existing
  cleanup path used on both successful and exceptional exits; assert the exceptional restoration
  while the fixture still exists. No game behavior changes or coverage exclusions.
- Own Debug/Werror save suite: 7 cases/393 assertions pass; formatting, static validation,
  diff checks and agent TypeScript gate pass. Await all17 CI jobs on this new head before handoff.

## 2026-10-01 — Opus: integrated Sol PRs #19 (save dialog), #20 (campaign artwork), #21 (build order)
- All three handoffs had 17/17 CI green at the stated heads (6cdac72d7, 40d15473e, 1e38d069a), each
  already on master abd4cd961. Production changes reviewed: #20 loads campaign artwork lazily per
  selected row with a text fallback (no shared-stem cache reuse); #21 iterates the real build-order
  length in Default (was a hard-coded 31, which dropped addon buildings), makes the combo read-only in
  replays and keeps button clicks blocked there, while list selection may update the preview. #19 is
  test-only. NOTES/ROADMAP/CHANGELOG conflicts resolved keeping both sides, each Sol paragraph kept
  under its own heading. Release Test_splitscreen on the merged master passes (exit 0); pushed.


## 2026-10-01 — Sol: military sliders, addon focus and replay edit rejection
- Own bounded military-settings-controller slice starts at master 5af48a215. Eleven cases enter
  the actual Back -> system ring -> Main selection -> Military path through driver input in a
  singleton-backed loopback game. All four DEFENDER_BEHAVIOR/SEA_ATTACK visibility combinations
  cover every visible slider at both bounds, hidden focus exclusion and values preserved,
  Default/reopen, mouse +/- and periodic transmission while still open, and Help return.
  Assertions inspect all eight settings in the actual world after a real command roundtrip;
  the lobby also requires the server broadcast after resetting only its eagerly edited local copy.
- Replay progress callbacks previously marked unsendable edits pending, so changed displays
  remained until the timer and closing raised a false discard warning. Restore recorded values
  immediately after replay progress input, and reject Default before mutating or marking state.
  Help stays available. Recorded live edits, all physical replay input paths (pad steps, mouse
  buttons, bar and wheel), close/reopen, recorded-value refresh and clean completion are proved.
  Production changes are confined to iwMilitary; no shared adapter/router/view or fixture edits.
- The complete initialization/body is guarded on both exits; an executed throw probe is reached
  with the military window open, restores settings before fixture destruction and destroys the
  desktop before backend teardown. No second singleton or coverage exclusions.
- Original production runs all eleven cases: seven live/cleanup cases pass, all four replay
  cases fail at intended UI/close assertions (715 failed assertions). Fixed Debug/Werror passes
  eleven cases / 10,099 assertions. CMake reconfigured and new suite presence verified; own
  build/debug cache with at most two jobs. Full-suite evidence recorded after its run below.
- Read-only exact gpt-6.1-sol actual-source review: an apparent reversed SEA_ATTACK expectation
  is disproved by AddonSeaAttack default status2 (disabled), status0 (enabled). The existing
  LocalGameFixture constructor may throw while copying a map after registering singleton
  references; it is unchanged and tracked separately with the primary lane. Final review finds
  no concrete introduced/blocking issue. clang-format10, static validation, diff check and
  agent TypeScript gate pass. Draft until all17 exact-head CI pass; Opus owns integration.
Full own Debug Test_splitscreen passes all489 cases / 56,361 assertions on the fixed source.
Additional executed negative control sends only default military values while the visual copy still
accepts the edited values: all four addon-policy cases fail at the actual-world wait. Fixed source
is restored byte-for-byte, rebuilt and rerun: all11 affected cases / 10,091 assertions pass. The full
489-case run above used this identical fixed source. No unmatched-filter exit counts as evidence.
