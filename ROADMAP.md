# Roadmap

The order work is done in. Each item is sized to finish in one or a few work sessions;
anything bigger is split before it is started. Status: ☐ open · ◐ in progress · ☑ done.

## M0 — A version you can install and keep updated (first!)

- ☐ CI: GitHub Actions builds Linux x86_64 (portable tarball/AppImage) and Windows on every
  tag, publishes a GitHub Release with notes.
- ☐ Versioning + `CHANGELOG.md`, written for players, one entry per release.
- ☐ In-game changelog: after an update the game shows what changed since the last run
  (menu entry to show it again).
- ☐ Auto-update: the installed game checks the GitHub releases of this repo and updates itself
  (reuse/adapt RttR's `s25update` where possible).
- ☐ Steam Deck installer: one `.desktop` file to open in Desktop Mode (pattern:
  ArnoldSmith86/minecraft-splitscreen) — downloads the release, asks for / finds the S2 Gold
  `DATA`+`GFX`, adds a Game Mode shortcut with artwork.
- ☐ Linux (Mint) install path: same release, simple install script / AppImage.

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
- ☐ Detect the Steam Deck and default to a layout and scale made for its 1280×800 screen.

## M4 — More campaigns

In order: Die Rückkehr der Wikinger (complete, Spikeone/RttR_Campaigns), Roman Campaign II
(needs a `campaign.lua`), Oktavianus' Reise, Tyrann, then FANpaign and 2NDpaign (need one-sided
alliances PR #1680 and RTX start positions PR #1683). Licenses/permission are checked before
anything is bundled.

## Continuous

- Every few weeks: review all RttR forks and branches, merge what is safe, note the rest here.
- Keep in sync with upstream master; offer generally useful changes back upstream.
