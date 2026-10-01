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
- ◐ Steam Deck installer (written, untested on a Deck): one `.desktop` file to open in Desktop Mode (pattern:
  ArnoldSmith86/minecraft-splitscreen) — downloads the release, takes `DATA`+`GFX`+`VIDEO` out of the
  GOG installer or an archive.org CD copy in ~/Downloads (no disk-wide search; help page otherwise,
  Jan 2026-09-28), adds a Game Mode shortcut named "The Settlers II: Coop" with SteamGridDB artwork.
- ☑ Intro: the main menu's Intro button plays the original `VIDEO/INTRO.SMK` (Smacker via libsmacker)
  with sound. The original campaign plays no other video (checked 2026-09-28, see NOTES).
- ☑ Default display: borderless window at the desktop resolution (Jan 2026-09-28).
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
- ◐ Assertions on game state: buildings, wares, mission flags, and that campaign Lua triggers
  (onGameFrame, onOccupied, victory/defeat, message boxes) fired. Done: smoke test per mission (M0.5b);
  MISS200 walkthrough — a script plays mission 1 through its tutorial goals, explores and occupies
  the arc at 14,8 through real buildings/roads and asserts events 1–7, 16 and 99 in order. Victory
  records chapter 0 (`roman=2`); disabling the arc's real victory trigger must fail the walkthrough
  (ctest `CoopWalkthrough_roman_MISS200`, local; CD and GOG maps tested, 2026-09-30).
  Walkthroughs for further missions only where a coop change needs them.
- ☑ Run it in CI on every push; failures reported to the JARVIS lane: the `CoopHeadless_*` tests run with the unit
  tests on every push, and a failed workflow reaches the lane through the repo's GitHub webhook. The mission tests
  need original S2 data and stay local.
- ☑ Replay-based regression: same seed twice → same final checksum (ctest `CoopHeadless_Deterministic`);
  `ai-battle --check-replay <rpl>` replays a recording without AIs, compares every recorded checksum and the final
  state, exit 3 on async (ctest `CoopHeadless_ReplayInSync`, incl. a wrong-seed run that must be caught). Since
  2026-09-27. The splitscreen regression `SecondLocalPlayerCommandsTakeTheFullNetworkRoundtrip` also records
  and replays through the real GameClient with the mock video driver, checking every recorded checksum,
  error/async callbacks and replay completion. A negative checksum control on that path remains open.
  2026-09-27. The existing splitscreen network-roundtrip test also replays through the real GameClient/mock video.
  `ClientReplayChecksumTests` now adds a clean re-encoded recording with identical final GF/checksum and a
  corrupted-checksum control: one desync callback, paused playback and no normal completion. Disabling
  the production checksum verifier fails that regression (Sol companion, 2026-09-30).

## M1 — The official campaign, done properly (single group)

- ☑ Campaign status: remember finished missions, unlock the next, mark conquered continents.
  Upstream PR #1681 (kubaau) merged with its review points fixed; since 0.1.3. Tested: unit tests +
  ctest `CoopCampaign_*` (every official mission, finished headless, records exactly its own chapter).
  Not yet seen in a real GUI (victory screen, locked buttons) — needs Jan's Deck/Mint.
- ☐ Mission unlocking and presentation as close to the original as possible (Roman campaign
  MISS200–209, World campaign).
- ☐ Original videos/intros where the original had them, if they exist in the S2 data.

## M2 — Coop: many players, one tribe

- ☐ Engine: several clients control ONE player slot (commands from any of them act for that
  player; host decides who is in the group). Nobody upstream or in any fork does this yet.
  Design: doc/coop/SharedPlayerSlot.md (member connections; the server merges their orders into the
  leader's command set; reviewed by Codex 2026-09-28). Steps:
  - ☑ 0 network test harness: `coop-net host/join` (extras/coop-net), ctest `CoopNet_*` (Linux/macOS CI). 2026-09-28
  - ☑ 1 server merge · ☑ 2 client member mode: `coop-net join --member-of 0`, ctest `CoopNet_Member*` (a member's
    order lands in the leader's world; every process ends in the same state; a member that stalls 2 s catches up;
    a diverged member is caught by the test). 2026-09-28
  - ☑ 3 join flow and lobby: ☑ protocol (member list, host allow/kick, switch from a lobby slot to member; ctest
    `CoopNet_Lobby*`, 2026-09-28) · ☑ lobby GUI (co-player row: host checkbox + list + Remove, players Join,
    "Jan +2" in the rows; UI tests + a real two-client game by hand, 2026-09-29) · ☑ joining a full lobby as
    co-player of the host (Join Game checkbox, ctest `CoopNet_LobbyJoinHost`, 2026-09-29) · ☑ campaign default (members
    allowed when a campaign is hosted: dskCampaignMissionSelection::StartServer, 2026-09-29)
  - ☑ 4 campaign as network game: ☑ "Campaign together..." in Create game, co-players allowed by default (seen
    working by hand with Roman mission 1, 2026-09-29) · ☑ headless proof: MISS200 with its script as a
    network game, a co-player's order lands (local ctest `CoopNetCampaign_roman_MISS200`, 2026-09-29) ·
    ☑ victory/progress on a co-player's machine (the mission's final event fires in every process; host and
    co-player both record the chapter; local ctest `CoopNetCampaignWin_roman_MISS200`, 2026-09-29) · ☑ diary pause
    (the host's close resumes everyone — seen by hand; a co-player's close only closes their own window, because
    GameClient::SetPause is host-only, same as in any upstream network game)
  - ◐ 5 robustness: ☑ member checksums compared with the leader's on the server, a diverged member is removed,
    one more than 20000 NWFs behind too (ctest `CoopNet_MemberDesyncDetected`, 2026-09-29) · ☑ the removed member
    is told why (host removed you / your player left / async / too far behind) instead of "connection lost" · ☐ leader leaves → first member takes over the slot. Deferred (2026-09-29): the server runs in the host's process,
    so when the host leaves the game ends anyway, and in a campaign the leader IS the host; hand-over would only help
    a non-host leader in a multi-player map (M5). Host migration is a separate, much bigger topic.
- ☑ Campaign missions hosted as network games instead of local-only, with the shared slot (steps 3+4 above).
- ◐ Save and resume a coop campaign with the same group: ☑ works with upstream's network load (Create game → Load):
  the saved human slot is free again, the co-player rejoins "the host's tribe", both stay in sync, the mission can
  still be won (local ctest `CoopNetCampaignResume_roman_MISS200`, 2026-09-29); loading a save with one human over
  the network allows co-players at once. ☑ seen in the real GUI (two clients under Xvfb: Load → lobby with co-players already allowed → join
  with "Play the host's tribe together" → both in the resumed game, no async; 2026-09-29) · ☐ deterministic loading of a save made in a
  single-player game (upstream `save-rng-state` / PR #1970) — only if a test shows an async after loading.

## M3 — Splitscreen, controller and Steam Deck UI

- ◐ Merge derneuere's `splitscreen-gamepad` (local 1–4 players, gamepads, radial build menu,
  couch lobby), adapted so local players can share the one campaign player. Split (2026-09-30; the branch is
  13 commits, ~40k lines of code plus the language catalogues moved into the tree):
  - ☑ a) merged as is (PR #2, 2026-09-30): all our tests and its Test_splitscreen pass locally and in CI,
    two local players seen in the real client under Xvfb (`--local-players 2`, a building placed from the second view).
  - ☑ b) coop member + additional distinct local player slots: forbidden by the pure-local-only guard.
    Real host/member Debug regressions prove refusal before loading/starting, removal of the member and continued
    host play; an ordinary member in the same topology finishes in sync (Sol companion, 2026-09-30).
  - ☐ c) local players share the ONE campaign player (several views, one player id) instead of taking AI slots.
    Design: doc/coop/SharedLocalViews.md. Steps: ☑ 1 GameClient views ≠ slots + CLI `--share-player` + test · ☑ 2 lobby seat "together
    with seat 1" (campaign: only that; 0.1.8, 2026-09-30) · ☐ 3 seat colours per view, road-preview/settings polish.
- ◐ Full controller navigation of the main menu and all dialogs; mouse/keyboard unchanged.
  Merchandise statistics has five physical-input regressions for all fourteen ware toggles,
  clear, all four time ranges and older peaks, Help/reopen, unchanged history/inventory and
  save-backed real replay completion at the exact final GF/checksum (Sol companion; exact-head
  CI required before handoff). A separate coarse aggregation defect is delegated to the primary lane.
  Goods distribution now rejects replay pad/wheel/bar edits immediately without a false discard
  warning and keeps Help available. Nineteen real-input/real-world tests cover all eight
  wine/leather/charburner policies, all tabs and bounds, Default/reopen, mouse/timer transmission
  and generated replay browsing/completion (Sol companion; exact-head CI evidence tracked in its PR).
  Credits supports controller B to Main menu; its text pages stay usable when optional game graphics
  or world data are unavailable, with physical-input and missing-resource regressions (Sol companion, 2026-09-30).
  Campaign chooser Back now retains local/network context; chapter/campaign victory screens support
  A/B/Start, with driver-event regressions (Sol PR #4). Remaining dialogs and menu paths stay open.
  Options handles controller B through its existing save/validate/Back action; driver-event tests cover
  saved text, cancelled dropdown selection, music overlay and both invalid-port confirmations.
  Table navigation starts at row zero on the first Up/Down, initially and after clearing/reloading;
  keyboard, controller and mouse regressions cover activation, scrolling and empty-table behavior.
  Intro accepts controller A/B/Start to skip, also from its missing-video page, with overlay precedence,
  single-transition/no-input-leak regressions and a locally tested original-movie path.
  Singleplayer/Multiplayer handle controller B through their existing Back actions; all focused actions,
  replay/login overlays, missing-save confirmation and a no-Main-menu-quit countercheck are covered.
  Online-lobby B uses the existing disconnect/Back action; six regressions authenticate the real
  client against a loopback TCP peer and cover all five focusable controls, overlay precedence,
  physical input, edited chat and B bursts (Sol companion, 2026-09-30).
  Direct-IP and the LAN browser handle controller B through their existing Back actions. Eight driver-event
  and physical-input regressions cover all seven focusable controls, window/confirmation precedence,
  mouse/A/keyboard behavior and B bursts stopping at Multiplayer (Sol companion, 2026-09-30).
  Create game/map selection handles controller B through its original Back route, with four-context,
  mouse and regular/custom-window regressions (Sol companion, 2026-09-30). The network Create Game
  form also supports B/Escape/title close cancellation while keeping right-click inert, with
  validation, all pad/keyboard focus targets and real-loopback lobby regressions.
  Long Readme, Help and Changelog scrolling is covered through driver and physical mouse input; scroll
  position now clamps safely when text/visible area changes, preventing out-of-range drawing.
  In-game Save is exercised from the controller system menu through Options in a real local game:
  saved world bytes, row selection/overwrite, keyboard submission, validation, cancellation and
  autosave selection are covered.
  Construction-order list/dropdown and reorder/default buttons now have physical-input,
  real-world command and generated-replay coverage for all eight wine/leather/charburner combinations;
  Default retains every active building, and replay settings stay read-only with current previews.
  Music playlist track reordering now refreshes playback; cancelled additions keep playback intact.
  Controller regressions cover song activation, removal, confirmed/cancelled input, repeat/random
  controls and saved files.
  Addon Settings has eight physical-input regressions for Apply/Abort, category scroll reset,
  dropdown cancellation, read-only and whitelist/default enforcement, exact saved config and
  real local-server settings roundtrip (Sol companion, 2026-10-01). Policy-specific windows are
  tested with the real lobby parent; campaign Lua routing and preset Save/Load remain separate.
  Transport priorities now have seven physical-input/backend/replay regressions with leather on/off:
  selection, all move buttons and boundaries, Default/reopen, mouse selection and Help return,
  every actual ware priority, recorded updates and read-only replay policies (Sol companion, 2026-10-01).
  Other paths remain open.
- ☐ Couch join screen: in the main menu each gamepad presses A to join; the screen shows how many
  players (and which controller is who). That count drives everything after it.
- ◐ Campaign overview with artwork of every working campaign and single scenario: optional
  missing/damaged images have a consistent text fallback and do not block other previews;
  controller selection and continuation are exercised with generated campaign data.
- ☐ Detect the Steam Deck and default to a layout and scale made for its 1280×800 screen.
- ☐ Self-updating AppImage release (Jan 2026-09-28: nice to have, but only after splitscreen and
  gamepad support are done).

## M4 — More campaigns

In order: Die Rückkehr der Wikinger (complete, Spikeone/RttR_Campaigns), Roman Campaign II
(needs a `campaign.lua`), Oktavianus' Reise, Tyrann, then FANpaign and 2NDpaign (need one-sided
alliances PR #1680 and RTX start positions PR #1683). Licenses/permission are checked before
anything is bundled.

## M5 — Multi-player campaigns and scenarios

- ☐ Maps/campaigns made for several players: one slot per real player, own colour and tribe;
  only when the map has a single human side does everyone share one tribe.
- ☐ Long-term goal: every campaign and scenario ever released for S2 or RttR playable here.

## M6 — Settlers III and IV rules as add-ons (Jan, 2026-09-30)

Everything Settlers III and IV add to normal multiplayer play, as RttR add-ons with S2 graphics, so that a
preset eventually plays like S3 or S4. Only after M3; every add-on is a step of its own, playable and tested
(headless runner + AI) before the next one starts.

- ☐ 0 Inventory: every S3 and S4 gameplay feature (economy chains, specialists, military, magic/mana, trade,
  ships, win conditions), each marked "RttR has it / an add-on comes close / new", split into add-on-sized
  slices. Written to doc/, not researched twice.
- ☐ 1 Presets in the add-on window, one click, editable afterwards: **Classic** (S2 as shipped), **Comfort**
  (S2 plus the quality-of-life add-ons), **Age of Gods** (S3 rules), **Dark Tribe** (S4 rules). A preset is data,
  so a new add-on only has to be added to it.
- ☐ 2 Artwork pipeline for new buildings, figures and wares: AI-generated sprites that match S2's palette,
  perspective, player-colour masks and animation frames, packed as an asset override. Prompts and scripts in the
  repo so any piece can be regenerated.
- ☐ 3 S3 slices, one add-on each (first candidates from step 0: priests and mana with a few spells, the
  market/trade by donkey, S3's soldier strength levels). The AI must play each one, or the add-on is off for AIs.
- ☐ 4 S4 slices the same way (candidates: the Dark Tribe as an AI opponent, gardeners reclaiming dark land,
  eyecatchers).

## M7 — HD graphics remake (Jan, 2026-09-30)

All game graphics recreated by AI image generation in an HD-remake style. The original graphics always stay
selectable; eventually the game runs without the original game files at all.

- ☐ 0 Style study: five sprites (HQ, woodcutter, carrier walk cycle, a tree, a terrain tile) in HD, side by side
  with the originals in the real client; choose tool, resolution (2× or 4×) and style before anything big.
- ☐ 1 Engine: load higher-resolution sprites and draw them at the original size, with a setting
  "Graphics: Original / HD". Original is the default until HD is complete.
- ☐ 2 Bulk pipeline: every sprite of the S2 archives, keeping anchor points, frame counts, player-colour
  masks and shadows; reproducible scripts, a checker that compares sprite sizes and anchors to the original.
- ☐ 3 A free set (graphics, then sounds, music, maps) so a player without S2 can play. For that set the art is
  generated from descriptions, not from the original pictures, so it does not count as a copy of Blue Byte's
  work; the legal side is checked before it is shipped.

## Continuous

- Every few weeks: review all RttR forks and branches, merge what is safe, note the rest here.
- Keep in sync with upstream master; offer generally useful changes back upstream.
