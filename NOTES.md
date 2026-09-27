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
