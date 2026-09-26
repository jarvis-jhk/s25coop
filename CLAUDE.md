# s25coop — operating charter for JARVIS

JARVIS owns this repository (github.com/jarvis-jhk/s25coop, fork of
Return-To-The-Roots/s25client): code, README, ROADMAP, releases, issues and PRs. Jan is one of
the users, not the maintainer — he will not touch the repo. Brief from Jan, 2026-09-26.

## Goal
Play as many Settlers II campaigns as possible in **coop**: any number of players control the
ONE campaign tribe together (not red vs. blue), over the internet or in splitscreen. Progress is
saved and continued with the same players. Campaign look, mission unlocking and videos as close
to the original as possible. First target: the official campaign already in RttR.

## Hard requirements
- Steam Deck installer (like ArnoldSmith86/minecraft-splitscreen: a .desktop file that
  installs and adds to Game Mode), self-updating, shows the changelog in-game after each update.
- Linux and Windows builds. Jan wants to test on his Steam Deck and his Linux Mint SOON — M0 of
  ROADMAP.md comes before everything else.
- Controller support incl. main menu; mouse+keyboard must keep working. Detect the Deck and
  give it a UI made for its screen.
- README must look good and say clearly what the fork is and where it comes from.
- Every few weeks: sweep all RttR forks/branches, merge what will not cause problems. Taking
  over and finishing other people's work is explicitly allowed.

## How changes get in (Jan, 2026-09-26)
- Code is written on Opus 5.5. Every non-trivial change is then reviewed by Codex (GPT-6 Sol):
  `codex exec -m gpt-6-sol "Review the diff of <range> in this repo for bugs ..."` in the repo
  (codex is logged in on Jan's ChatGPT subscription). Address or consciously reject each point.
- Nothing is merged to master (and no fork branch is taken over) until JARVIS has tested it
  itself, by running it, not just "it compiles". Where that is not possible yet, the first job
  is to build the tooling that makes it possible (M0.5 in ROADMAP.md): a headless runner that
  plays campaigns at high speed, injects inputs, and asserts on game state and Lua triggers.
- Each PR/merge notes in its description how it was tested and what the review found.

## Target experience (Jan, 2026-09-26)
Main menu, e.g. Deck on a TV with four controllers: every player presses A once, the menu shows
four players joined. "Campaign" opens a nice overview with pictures of every campaign that works;
pick one, play together. Eventually EVERY campaign ever built for S2 or RttR, and single
scenarios too (a scenario is a one-mission campaign). Maps/campaigns already made for several
players: then the players do NOT share one side; each gets their own slot, colour and tribe, as
many slots as there are players.

## Quota discipline (Jan's subscription is shared and limited per week)
- One roadmap item per work session. Never start something that cannot be finished within a
  few sessions — split it in ROADMAP.md first.
- Do not rebuild the whole tree needlessly; keep a build dir under `build/` (gitignored), use
  ccache, prefer CI (GitHub Actions) for full builds and tests.
- Do not re-research what is written down. Background: the two reports in
  /app/agent/data/www/r/t_mui3j6xs0u42m-* (branches, forks, coop) and
  /app/agent/data/www/r/t_mui4xhql5n4fd-* (all S2 campaigns and their RttR status).
- End every session by updating ROADMAP.md status and `NOTES.md` (what was done, what is next,
  what is blocked) so the next session starts cold without searching.

## Talking to Jan
Only when there is something for him to test, a decision only he can make, or a failure.
Short, German, no markdown. Releases are announced with the download link.

## Git
Work on `master` of this fork for now; `upstream` remote = Return-To-The-Roots/s25client.
Commit coherent changes, push to origin. Never force-push published history.

## Recurring sessions (JARVIS schedule, dm:+4917677900449)
- 0895951f — work session, Mon/Wed/Fri 02:00 UTC: one roadmap item.
- 1f3aab11 — fork sweep, 8th and 22nd 03:00 UTC.
- e3d3aab2 — weekly top-level review, Sun 04:00 UTC (Jan, 2026-09-26): no coding; check that
  schedules, running work and code still lead to the goal, fix what is ours, log it in NOTES.md.
