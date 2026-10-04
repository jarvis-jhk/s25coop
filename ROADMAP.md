# Roadmap

The order work is done in. Each item is sized to finish in one or a few work sessions;
anything bigger is split before it is started. Status: ☐ open · ◐ in progress · ☑ done.

**Current priorities (weekly review 2026-10-04).** The core goal is a group playing a campaign
together on one Deck/TV or over the internet. Network coop (M2) and shared local views (M3c 1–3) work.
What stands between that and a good couch experience, in order:
1. Opus: M3c step 4 (a second view's open economy windows refresh) — closes shared local views.
2. Sol: menu redesign 1a (PR #40) ✓ → 1b local player cards ✓ (PR #42) → 1c, then 6 campaign hub.
3. Sol: Deck detection and first-run scale (its ready backlog #0), then controller panel slice 2.
4. Opus: M1 mission presentation (unlock/brief screens like the original) once 1–3 have no open review.
5. Release whenever player-visible changes are on master and its CI is green — tag, push, then CHECK the
   GitHub release exists (0.1.10 was noted as released on 2026-10-03 but its tag was never pushed).

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
    with seat 1" (campaign: only that; 0.1.8, 2026-09-30) · ☑ 3 seat colours per view (focus ring, brief stripe), a road built from one view stops a crossing preview in another (2026-10-02) · ☐ 4 refresh a second view's open economy windows when the shared settings change.
- ◐ Full controller navigation of the main menu and all dialogs; mouse/keyboard unchanged.
  Covered with physical-input regressions (Sol PRs #4–#39; details in NOTES.md and the PRs): Back/B on
  every main-menu path (single/multiplayer, Direct-IP, LAN, online lobby, Create game, campaign chooser,
  Options, Credits, Intro), dropdown A-open/confirm/B-cancel, tables, long text scrolling, in-game Save,
  addon settings, every economy window (distribution, transport, tools, military, build order), statistics,
  merchandise, Stock, Building overview, Post, outline map, music player. Replays keep these read-only.
  Review 2026-10-04: these exercise the LEGACY floating windows, which the controller panel (below) and the
  menu redesign will replace for controller users. No further "exercise legacy window X" slices unless a real
  bug is reported; open paths are covered by the panel and menu-redesign slices instead.
- ◐ Jan's Steam Deck feedback (2026-10-01, voice; not urgent, handed to Sol as separate requests):
  - ◐ Steam, application menu and window title use "The Settlers II: Coop". Existing Steam entries
    migrate automatically on reinstall or desktop launch with Steam closed; Game Mode leaves
    live shortcuts untouched. Integrated (PR #30), released in 0.1.10. Confirm on the Deck.
  - ◐ Controller hints as Xbox button glyphs: per-view in-game brief badges now colour
    A/B/X/Y and label shoulders, D-pad directions, Start/Back and stick inputs, with font-metric
    wrapping and complete text fallback for oversized groups (PR #36).
    Existing contextual bindings stay authoritative. Typed right-stick and LT/RT camera/zoom
    hints now cover legacy world/road/ring/window/watch modes and actual target-zoom limits
    (PR #38; both released in 0.1.10). Other UI surfaces and
    device-specific artwork remain separate slices. Deck appearance is unverified.
    Rendering scope and fallback policy: [Controller hints](doc/coop/ControllerHints.md).
  - ◐ Fixed L3 building-position shortcut plus current in-game mapping audit in
    [ControllerMapping](doc/coop/ControllerMapping.md); eight physical/live cases and full
    583-case Debug suite pass locally. All17 exact-head CI passed; Opus integrated PR #35.
    Further bindings and panel integration remain separate gated slices.
  - ☑ Lobby "play as a team / together" checkbox is drawn over other controls; fix the layout (PR #31, 0.1.10).
  - ☑ Dropdowns under D-pad/stick: focus must not change the value; A opens, D-pad picks, A confirms, B cancels (PR #34, 0.1.10).
  - ☐ In-game with a controller: instead of floating, movable, overlapping windows, a fixed side panel
    with tabs switched by the shoulder buttons that shows as much information as possible.
    Mouse/keyboard keep the windows. Design: [ControllerPanel](doc/coop/ControllerPanel.md).
    Design and these bounded slices must be pushed before panel implementation:
    - ☑ 1 pure per-view geometry/navigation model and unit tests (no input routing):
      PR #33 integrated from exact tested head b212278737; all 17 CI checks pass.
      Rendering, physical routing and usable Stock remain slice 2 work.
    - ☐ 2 opt-in shell plus readonly Stock; physical entry/tab/Back/Help/modal tests,
      basic resize/disconnect/mixed-device exits and shared-view live refresh.
    - ☐ 3 Buildings/productivity: actual counts, targets and the owner's camera.
    - ☐ 4 general/merchandise Statistics: history, physical ranges and replay browsing.
    - ☐ 5 Post: filters, incoming replacements, detail/diary/location and delete confirmation.
    - ☐ 6 view-only Settings: building positions, names/output and watch-only per view.
    - ☐ 7 editable economy Settings: distribution, then transport/tools, then military/build
      order in separate policy PRs; live broadcasts, Apply/Cancel and replay guards.
    - ☐ 8 parity/default rollout: complete mixed-device/shared-view regression matrix and
      remaining legacy destinations; default only after parity and packaged Deck checks.
    Each slice needs fresh claims for shared input/view/page scopes, physical Debug proof
    and all exact-head CI green; use no panel code in this design-only checkpoint.
- ☐ Couch join screen: in the main menu each gamepad presses A to join; the screen shows how many
  players (and which controller is who). That count drives everything after it. Review 2026-10-04: this
  is delivered by menu-redesign slices 1a/1b (player cards, one per controller) and 6 (campaign hub);
  do not build a separate join screen.
- ◐ Campaign overview with artwork of every working campaign and single scenario: optional
  missing/damaged images have a consistent text fallback and do not block other previews;
  controller selection and continuation are exercised with generated campaign data.
- ☐ Controller-first pre-game menu (Jan, 2026-10-03; design and slices: doc/coop/MenuRedesign.md). One PR per
  slice, physical-input tests, mouse/keyboard unchanged:
  1a pure player-card model ✓ (PR #40, model only; shell wiring is 1b) ·
  1b local cards in the lobby ✓ (PR #42, integrated 2026-10-04) ·
  1c live read-only player/co-player cards and shared-seat cursor preview ◐ (Sol: local Debug green; exact-head CI required) ·
  2a staged rule edits with re-confirm · 2b rules drawer, one editor at a time · 2c online rule suggestions ·
  3a addon category table · 3b category tabs and "changed only" filter · 3c rule presets ·
  4a player profiles · 4b on-screen keyboard · 5a menu world decoupled from the lobby client ·
  5b AI replay behind the main menu · 5c ambient sound, Deck fps/battery options · 5d scene per nation/campaign ·
  6 campaign hub as the new start page.
- ◐ Detect the Steam Deck and default to a layout and scale made for its 1280×800 screen.
  Sol first-run display slice: exact Steam flag or Valve LCD/OLED DMI identity, persisted automatic
  640-unit reference height (125% at1280×800), stored settings and TV/fixed-scale precedence.
  See doc/coop/SteamDeckUi.md; integrated 2026-10-04 (PR #41, CI green at 8129b51ba). Controller layout and packaged
  Deck appearance remain separate menu/panel/hardware work.
- ☐ Self-updating AppImage release (Jan 2026-09-28: nice to have, but only after splitscreen and
  gamepad support are done).

## M3.5 — Linux handhelds via PortMaster (Jan, 2026-10-01)

Automatic builds that install through [PortMaster](https://portmaster.games/), first target Jan's
Anbernic RG 35XX H (Allwinner H700, aarch64, Mali-G31 with OpenGL ES only, 640×480, stock OS or
muOS/Knulli/ROCKNIX). Only after the M3 controller UI, which a handheld without mouse depends on.

- ☐ 0 Feasibility on the target: RttR draws with desktop OpenGL; check whether PortMaster's gl4es
  carries it on the H700 at playable speed, or whether a GLES renderer path is needed. Decide before
  building anything else; write the result to doc/.
- ☐ 1 CI job: aarch64 build (PortMaster's build-environment image / compatible glibc), packaged as a
  PortMaster port (`port.json`, launcher `.sh` using `control.txt`, gl4es if needed), attached to every release.
- ☐ 2 640×480 layout: menus and in-game windows usable at that size (RttR's minimum is 800×600), Deck-style
  scale detection extended to small screens.
- ☐ 3 Game data: the port ships no S2 files; the launcher finds `DATA`/`GFX` the player copied into the port
  folder (or a GOG installer, like the Deck installer) and shows a clear help screen otherwise.
- ☐ 4 Test on Jan's RG 35XX H, then offer the port to the PortMaster repository.

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

## M8 — A really good computer opponent (Jan, 2026-10-02)

The RttR AI (`libs/s25main/ai/aijh`) is beatable and has only Easy/Medium/Hard. Goal: an AI that plays well,
found by measurement rather than guesswork, and then sensible difficulty levels on top of it. Starts after M3;
every step is finished, measured and pushed before the next.

- ☐ 0 Arena: `ai-battle` runs AI A against AI B on every suitable map at once (headless, fixed seeds, both
  starting positions swapped), in parallel up to the CPU budget, and writes one table: wins, losses, time to win,
  score at the time limit, plus asyncs/crashes. Reproducible from a single command, results kept in the repo
  so every later change is compared to the same baseline.
- ☐ AI must handle every game setting (addons, nation, map, starting resources) and actually use economy
  addons such as wine (Jan, 2026-10-03; doc/ai/Goals.md): step 0 gets a settings matrix, ignored addons are
  step-2 weaknesses.
- ☐ 1 Baseline and weaknesses: current aijh against itself and against each level; watch replays of the losses
  and write down the concrete weaknesses (economy stalls, soldiers, expansion, defence, ships) in doc/.
- ☐ 2 Iterate: one weakness per step, each change must beat the previous version in the arena with a
  statistically clear margin over all maps before it is merged. Parameters the AI already has are tuned
  automatically first (search over the weights, arena as the fitness function).
- ☐ 3 Learned components only if 2 runs out: e.g. a trained evaluation for build-site choice or attack timing,
  trained in the headless arena. Must stay deterministic (same seed → same game) so network games and replays
  stay in sync, and cheap enough for a Steam Deck.
- ☐ 4 Difficulty levels made from the strong AI (handicaps such as reaction time, economy bonus/malus,
  attack restraint), each level measured in the arena so neighbouring levels are clearly apart.
  Old levels stay selectable for old saves and replays.

## Continuous

- Every few weeks: review all RttR forks and branches, merge what is safe, note the rest here.
- Keep in sync with upstream master; offer generally useful changes back upstream.
