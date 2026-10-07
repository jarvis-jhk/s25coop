# Session notes

Older entries (2026-09-26 – 2026-10-02) are in `NOTES-archive.md` — grep it for durable how-tos:
local build env and `build/dev` (2026-09-27), headless harness `ai-battle --test` and `ctest -R Coop`,
S2 data in /app/agent/data/siedler/S2 (`-DRTTR_COOP_S2_DIR`), fault-report path, coop-net tests,
CI lessons (clang-tidy, coverage, GCC/Boost quirks), Sol integration routine, earlier weekly reviews.

## 2026-10-07 — release 0.1.12; Win32 Home-return CI flake

Unit tests run [37574359421](https://github.com/jarvis-jhk/s25coop/actions/runs/37574359421) on master
0142389c9 failed once only in Windows Debug Win32 (`FrontEndHomeTests/EveryUpstreamDesktopComesBackToItsTile`,
testFrontEndHome.cpp:81, shared by six return paths, log does not name which). Isolated case and the full
643-case local Debug run passed; rerun of only the failed job (attempt 2) passed. Recorded as an unconfirmed
intermittent failure, no product fix claimed. If it recurs: add per-path BOOST_TEST_CONTEXT first
(Options validation/driver confirmation and campaign error modal are candidates). Logs in private
data/siedler/master-0142389-{win32,local-full}.log. Both master workflows green at 0142389c9 →
CHANGELOG Unreleased → 0.1.12, tagged v0.1.12 on the CHANGELOG/NOTES-only commit. Claims
master-win32-home-return-ci and frontend-f1-framework released.

v0.1.12 veröffentlicht: https://github.com/jarvis-jhk/s25coop/releases/tag/v0.1.12 — release workflow 37582133234 succeeded; Linux tarball, Windows ZIP and desktop installer uploaded. Unit tests 37582131441 and Static analysis 37582131447 also green on 153d4000b.

## 2026-10-07 — work session: front end F1 (page framework) + F3 (home page)

No open issues or PRs; master CI green at 5f5b4c793. Codex is out of quota until 2026-10-10 02:23 UTC
(`codex exec` answers "usage limit"), so reviews this session were done by a separate read-only Claude
agent instead; re-check with gpt-6.1-sol once the quota is back if something looks doubtful.
F1: `dskFrontEndPage` (header/back/title/player strip, tiles or list, footer help line via the brief's
key badges, back stack of factories with focus restore), pure `frontend/PageLayout` and `frontend/PageKeys`,
`brief::DrawKeyRuns` now shared with the in-game brief, `KeyAction::PageBack`, `WindowManager::IsSwitchPending`.
F3: `dskHome` is the start screen after the splash (tiles: resume last game, campaigns, maps & scenarios,
load game, play online, options, what's new, credits, classic menus, quit; debug-data question and
changelog popup moved along). Upstream desktops return through `frontend/MenuRoutes` (Home if it was
shown last, dskMainMenu after "Classic menus"); default style Classic, so tests and direct starts keep
upstream's flow; MenuPadFixture resets it. dskSinglePlayer's entries are public statics reused by Home.
Found by looking at the real client under Xvfb (data/siedler/frontendtest/run.sh <WxH>, screenshots
there): a desktop queued while the window is resized never gets Msg_ScreenResize (WindowManager only
tells the current one) - at startup Home was laid out for 1024×768 on an 800×600 screen. Pages now
re-lay out in SetActive; regression in LayoutFollowsTheScreenIncludingTheDeck.
Tests: Test_UI FrontEndLayout (6), Test_splitscreen FrontEndPageTests (5) and FrontEndHomeTests (4), all
physical input; negative controls (trail not pushed, focus not restored, no relayout on resize/activate,
routes forced classic) each fail. Debug build/dbg: full Test_splitscreen (638) and Test_UI (146) pass; other
local ctest pass except Test_drivers (no ALSA, as always). Second review pass (agent): no bugs; applied its
risks - menu style set on activation not construction, "New menus" button in the old main menu as the way
back, Home resumes the save it found instead of scanning twice. Known gap: no test resumes a real save
from Home (ResumeSave is upstream's case 3 moved, unchanged).
F2 in the same session: `dskTitle` after the splash (Home's B returns to it) and `input/Party` (joined
controllers in join order, kept by MenuPadInput, unplugging leaves it; the player strip on every page shows
it). The title offers MenuPadInput::MaxSlots slots, so a controller's first press gets it a slot and IS its
join (the pick-up press is swallowed anyway). Tests FrontEndTitleTests (4) + PartyModel; negative controls
(no auto-join, no unplug cleanup) fail. Full Debug Test_splitscreen (643) and Test_UI (146) pass.
First CI on d33ffcc3f failed: clang-format 10 wants a different layout than local clang-format 14 for an
anonymous namespace nested in another one (moved to top level); MSVC "ambiguous GetCtrl" inside a
`[this]` lambda (plain loop now); GCC 16 -Wnoexcept on `vector<Rect>::emplace_back(DrawPoint, Extent)`
(push_back(Rect(...)) now); gcc-10 coverage rejected the unexecuted `catch(...)` restore in a test (the
fixture destructor restores the screen now). **Check formatting with the CI's clang-format 10:**
`/app/agent/data/siedler/cf10/clang_format/data/bin/clang-format` (pip wheel unpacked there). CI's
clang-tidy 18 is at `/app/agent/data/siedler/ct18/clang_tidy/data/bin/clang-tidy`: `ninja -t compdb` in
build/dbg, strip `ccache`, then `clang-tidy -p <db> --extra-arg=-Wno-unknown-warning-option` per file -
minutes instead of the CI job's hour+. Second CI round (72832cba0): gcc-10 coverage flagged a test loop
whose body never ran (focus was already there) - use the shared walk helper instead of local loops.
Not seen yet: a controller on the real screen (Xvfb has no pad), the Deck itself. Under Xvfb the client
sometimes opens a 1024×768 window on an 800×600 screen (random; the old menus are cut off the same way) -
check the FPS counter is visible before trusting an 800×600 screenshot.
Next Opus: F7 party (seat exactly the joined party in the lobby, cards), then F4/F10.

## 2026-10-06 — Jan: front end from scratch; glyphs and help toggle on the roadmap only

Jan (Signal, three messages): be "WAY more aggressive" with the menu overhaul — rebuild everything from
starting the binary to being in game; pixel-art Xbox button images; a toggleable help footer; and load earlier
single-player games with several players. Then: "Do neither of them now. Update the roadmap." So only planning:
ROADMAP priorities rewritten (front end is #1 for Opus and Sol), new M3 item "Front end from scratch" with slices
F1–F12 and design doc/coop/FrontEnd.md (MenuRedesign.md now only supplies ideas, not order); glyphs and the
full/buttons-only/off help footer are roadmap items after F1; single-player saves with several players are an M2
item and part of F6/F7. Sol backlog: 3a, 4b (on-screen keyboard), 3b ready now.
Glyph prototype (not in the repo, deliberately): generator `/app/agent/data/siedler/art/padglyphs/padglyphs.py`
writes 22 8-bit palette BMPs (A/B/X/Y, LB/RB, LT/RT, View, Menu, Guide, 7 D-pad variants, LS/RS motion, L3/R3) and a
preview PNG; Loader folder archives accept them (index 0 transparent; `Archiv::get` returns nullptr when missing, so
text badges can stay the fallback). Open: real NormalFont height (face buttons are 15 px), Deck look.
Next Opus session: F1 framework.

## 2026-10-05 — Fault reports runs 37360066249/37362225980 failed (GitHub outage)

Both scheduled runs on 1a679b1 were cancelled after 15 min with "The job was not acquired by
Runner of type hosted" — no step ran; githubstatus.com showed an open Actions incident
(degraded performance). No code change. The ntfy `since` cache was not advanced and ntfy keeps
12 h, so the next successful run picks up anything posted meanwhile.

Same outage cancelled Static analysis 37364034705 and Unit tests 37364034806 on 6baeadbcc
(all jobs 0 steps, never acquired a runner). Both re-run 19:5x UTC; no code change needed.
Fault reports 37364947335 (19:40, on 6baeadbcc) failed the same way (job cancelled, 0 steps).
Fault reports 37366390859 (19:55, on b7fab06df) too: never acquired a runner, cancelled after 15 min.
Fault reports 37367678420 and 37370047183, Unit tests 37368149814 and Static analysis 37368149799 (on
c1f08a948/6e6a4fdb8) too — incident still open 20:45 UTC. Stopped pushing a NOTES commit per failure (each
push started two more doomed CI runs); one-shot continuation re-runs master CI after the outage.

## 2026-10-05 — work session: M3c step 4 done, 0.1.11 prepared

No open issues or PRs. CHANGELOG `Unreleased` → `0.1.11` (e04cef527: player cards, local lobby cards,
cursor colours, Deck first-run scale). Its CI was cancelled by the next push; the tree differs from
green 5021f66ab (Unit tests 37216628927, Static analysis 37216628880) only in CHANGELOG.md, so v0.1.11
was tagged on e04cef527 and pushed. Release verified: linux tarball, windows zip, desktop file. Jan told.
M3c step 4: after an economy window (distribution, transport, tools, military, build order) has sent
its change, other open windows of that kind on the SAME player re-read the visual settings
(`TransmitSettingsIgwAdapter::RefreshSharedWindows`, new `WindowManager::FindNonModalWindows(id)`,
pure virtual `GetSettingsPlayer()`). A window with its own unsent change is not overwritten; it wins
when sent. iwTransport/iwBuildOrder `UpdateSettings()` now always refill from visual settings (before:
only in replay; the only live caller is the new refresh). iwTools refresh also re-renders order texts.
Tests (testSharedViews.cpp): all five kinds follow, pending edit kept and wins (timer and Close path),
distinct players untouched. Negative control (refresh disabled): 6 failures across all kinds. Debug
build/dbg: full Test_splitscreen (628 cases) and Test_UI (140) pass. Test env needs LoadDummyMapFiles/BuildingFiles and
a leather_bobs pig icon for these windows. Codex gpt-6.1-sol review: no bugs; asked for close-path
coverage (added). Not covered: tool order-count texts (TOOL_ORDERING addon off), network members on
other machines (their visual settings are separate — M2 territory, not this step).
M3c (shared local views) is complete. Next Opus item: M1 mission presentation.

## 2026-10-04 — master CI flake (Windows Debug x64)

Unit tests run 37214226840 on 5ee026574 failed only in Windows Debug x64: `Error copying file (if
different) … libiconv2.dll` in ai-battle's post-build DLL copy — the same parallel-copy race as PR #34
(libogg-0.dll). No code cause; failed job re-run. Second occurrence: if it recurs, make the per-target
DLL copies serial/once (one copy target that others depend on) instead of re-running.

## 2026-10-04 — PR #44 integrated (live read-only lobby player cards)

Sol's iwLobbyPlayerCards (button "Player cards" in dskGameLobby, 4 cards/page, refreshed every paint
from GameLobby + coop members) and shared-seat cursor colours on the local cards via the new
input/LocalViewColor.h helper (also used by dskGameInterface::SeatColor, so lobby preview and in-game
agree). Exact head b586691fc (Unit tests 37211398603, Static analysis 37211398538, 17 checks green);
branch contained master, so the merge tree is byte-identical to the tested head. Opus reviewed the diff:
read-only cards block mouse/activate/step, button sits in the free strip y60-78 above the rows. No
blocker. CHANGELOG entries added.

## 2026-10-04 — PR #43 integrated (member host-message authorization tests)

Test-only M2 hardening from Sol: coop-net `--member-host-probe state|swap|settings` sends a
host-management message over a real authenticated member socket; the server must remove the member
("unexpected message"), the host's serialized lobby config must be unchanged and it must reach GF1000.
A `normal` control checks a member's woodcutter order converges. Port 13728. Exact head 4c43f3af6
(Unit tests 37198914973, Static analysis 37198914972 green). Opus reviewed the diff (harness-only, no
product change) and re-ran after merge in build/dbg: all 19 CoopNet_* pass (210 s). No CHANGELOG entry.

## 2026-10-04 — PR #42 integrated (MenuRedesign 1b, local lobby player cards)

Opus reviewed Sol's lobby cards at exact head 1afdb7478 (Unit tests 37194713080, Static analysis
37194713142 green; Sol's gpt-6.1-sol reviews clear). Each local controller now edits its own card
(D-pad up/down row, left/right value: colour, nation, team; seat mode host-only), authority is re-read at
send time, guests cannot touch host modals, and slot remaps swallow queued edges. Behaviour change:
the host pad now starts on its own card, so A there does nothing; shoulders reach Start, Start button
still starts. Merged with --no-ff; own Debug build of Test_splitscreen + Test_UI passed (451 s).
Not seen on a Deck/TV yet; card layout (4 seats, 108 px high, checkbox at y=560) only checked in code.
Next per ROADMAP: 1c (remote cards), then 6 (campaign hub).

## 2026-10-04 — PR #41 integrated (first-run Steam Deck display scale)

Opus + read-only gpt-6.1-sol review (no blockers) of Sol's `deck::Detect` (Steam `SteamDeck=0/1`
flag, else DMI Valve + Jupiter/Galileo) and `steam_deck_ui` in `[video]`: fresh profiles on a Deck get
reference height 640 (125% at 1280×800); configs without the key load false, TV mode still wins.
Merged at exact head 8129b51ba (Unit tests 37180413516, Static analysis 37180413489 green).
CHANGELOG has it under Unreleased; it goes out with the next release (alone it only affects fresh
installs, so not worth a release of its own). Not yet seen on real Deck hardware.

## 2026-10-04 — PR #40 integrated (MenuRedesign 1a, player-card model)

Opus reviewed Sol's `LobbyPlayerCardModel` (libs/s25main/input) and merged it at exact head 5c8ba7878
(Unit tests 37176328989, Static analysis 37176328988 green). Model only: rows Color/Nation/Team/SharedTribe,
bounded free-colour search, lock reasons per row, proposals never mutate the snapshot. Nothing uses it yet —
next is 1b (local cards in the lobby): on accept revalidate authority and apply only the selected row's field.
Not player-visible, so no CHANGELOG entry and no release.

## 2026-10-03 — splitscreen savegames resume with the same players (Jan, voice, 2026-10-02)
Bug: the lobby hid the seat panel for every savegame (`AreLocalSeatsAvailable` excluded them), so a loaded
splitscreen game had no way to seat the controllers again; own-slot co-players stayed idle Dummy AIs.
Fix (dskGameLobby.cpp): savegames get the seat panel; slots that are `AI::Type::Dummy` AND named
"Local player N" (current translation or English) are re-registered as additional local players before the
existing --local-players path validates/applies them; a save without such slots opens in "one tribe
together" mode (other slots there are real tribes, often campaign enemies). Shared views are not stored in a
save — players press A again. Tests: testMenuPadSeats `ALoadedSplitscreenGameSeatsItsLocalPlayersAgain`
(host-chosen Dummy is NOT seated) and `ALoadedTogetherGameOffersItsSeatsAgain`, both start → save →
real savegame host → lobby → start. Negative control on the old dskGameLobby.cpp: 3 failures. Release
local: full Test_splitscreen and Test_UI pass. Codex (gpt-6.1-sol): only finding = save made in one non-English
language and loaded in another is not recognised → consciously accepted (rare; the panel still lets
players sit on those slots by hand).
Also: ROADMAP M8 "really good computer opponent" (Jan, voice 2026-10-02): arena on all maps, iterate with
measured wins, optional learned parts, difficulty levels. PR #34 (Sol) Windows Debug failure was a flaky
DLL copy (`Error copying … libogg-0.dll`, parallel post-build copies); failed job re-run.

## 2026-10-03 — Sol: reconcile tested dropdown PR34 with save/resume master
- Original head aa55c3fb7588e129e788f98a0c2a1ef60b58ba0b passed all17 checks and both
  complete workflows: Unit tests https://github.com/jarvis-jhk/s25coop/actions/runs/37014584915
  and Static analysis https://github.com/jarvis-jhk/s25coop/actions/runs/37014584743.
  The Windows Debug DLL-copy failure was resolved by the primary's failed-job rerun,
  not by an unrelated product edit. No original-head CI failure remains.
- Current master90916ca0a adds splitscreen save/resume. NOTES append conflict was initially
  blocked by Opus's retained claims; after coordination and actual scope release, claim the
  five incoming paths and preserve both notes sections. Own seven dropdown source/test files
  remain byte-identical to the all17-green original head; incoming lobby and seat-test files
  remain byte-identical to master. This combination has additional code/tests, so it receives
  its own Debug build and full regression gate before the merge commit is pushed.
- Own GCC12 Debug/Werror build completed with at most2 compiler jobs; full combined
  Test_splitscreen575 cases/459466 assertions and UI137 cases/7619 assertions pass. Formatting/static/diff checks and actual agent TypeScript
  gate pass. The restart omitted TypeScript dev dependencies; npm install --include=dev
  restored the real compiler with no package.json/package-lock change before that gate.
- No tested handoff, budget gate or new implementation slice until all17 fresh resulting-head
  checks and both complete workflows pass. Retain unfinished source/test claims and one
  synchronized continuation. Opus owns integration; no release or Signal notification.

### 2026-10-03 — Sol PR #34 integrated (Opus)
Merged sol/dropdown-confirm-navigation at tested head cb297a4a4 as 9899cb311 (all 17 exact-head checks green).
Closed dropdowns now pass D-pad/stick to focus navigation; A opens, browse is silent, A confirms, B/focus loss restore.
Closed-box stepping no longer changes values (mouse wheel still does). iwBuildOrder timer no longer commits an open mode preview.
Addresses Deck request t_muq4pbvs1viw8z. Possible follow-up (not filed): other windows reading combo selections on
button presses are unaffected, since only iwBuildOrder transmits on a timer.


## 2026-10-03 — Sol: fixed controller building-aid shortcut and mapping audit
- PR34 integrated by Opus at master9899cb311 after Sol's all17 exact-head handoff
  cb297a4a4d2ac87cf0dcbbd90d039bba209a4c7e. Both complete successful workflow links
  and combined Debug575/UI137 evidence are in its ready PR description and Sol checkpoint.
- Claim shared desktop/hints plus bounded dedicated and affected test/doc scopes before editing.
  New sol/controller-building-aid-shortcut starts from integrated master9899cb311.
  Deck mapping feedback t_muq4pbai1teze5 is split: audit all current contexts and give L3
  one safe per-view building-spot toggle. No extra destructive actions, glyph or panel claim.
- L3 precedes rings, focus, modal windows and watching. It never commits a pending control;
  in watching it deliberately returns to play and shows all spots, while B still restores
  the exact saved display. System-menu cursor-only cycling and primary-only persistence
  remain unchanged. doc/coop/ControllerMapping.md records the actual axes/buttons, mode
  precedence and remaining unused controls. Existing game fault reporting stays in place.
- Reconfigured Sol's own GCC12 Debug/Werror cache and confirmed the new eight-case suite
  in the 583-case binary. All8 physical/live cases pass260 assertions; full Debug
  Test_splitscreen583 cases/458467 assertions pass. Tests cover four distinct-player seats,
  held/released input, ring selection/label refresh, ordinary and modal dropdown previews,
  physical watch entry/exit, disconnect, primary persistence and the unchanged space-key
  path. A real nonempty road preview survives the shortcut; shared-tribe seats stay independent
  and both players' completed real-server recordings contain zero game commands.
- Separate normal/exceptional cleanup guards destroy windows before their desktop/world and
  restore settings before fixture destruction. Deliberate exception probes assert they reached
  the intended open menu, including a live game. No new Settings/GameClient singleton instance.
- Executed original master9899cb311 desktop negative control: all4 targeted physical cases
  run and fail17 intended aid/label/watch assertions; no unmatched-filter or crash evidence.
  Restored the fixed source byte-for-byte, rebuilt and all8 cases pass260 assertions again.
- Final read-only exactly gpt-6.1-sol supplied patch and complete fixture/context review has
  no concrete blocker. Narrow documentation to recorded game-command absence rather than a
  measured claim about all network traffic. Format10, static validation, diff and actual agent
  TypeScript gate are required before commit; all17 fresh exact-head checks and both complete
  workflows remain the tested handoff gate. No hardware, glyph, panel or release proof claimed.

## 2026-10-03 — Sol: PR35 documentation reconciliation
- Master afdcbe1bc only adds Opus’s PR34 integration note. Its parallel append conflicts
  with the PR35 notes and prevents GitHub from starting pull-request CI. Preserve both
  sections; every non-NOTES file remains byte-identical to tested 7d101bf365e981473c70c9f854ae1c19b4d2a7dd.
- Existing local Debug583 cases/458467 assertions and eight dedicated cases/260 assertions,
  executed negative controls and read-only reviews remain applicable without another build.
  Require fresh all17 exact-resulting-head checks and both successful workflows before
  ready/tested Opus handoff; keep six unfinished source/test claims and one continuation.


### Sol PR35 coverage repair (2026-10-03T12:33:41.570642+00:00)

PR35 reconciled head b8cf5f7a5270f08527a4ace81e4d4ae2dfd12afc completed 16/17 checks successfully; gcc-10 coverage alone failed, while Static analysis passed. The test observer callback and successful desktop-transition CloseNow loop had not executed. The ordinary/modal dropdown test now physically reopens and accepts a fresh choice after B restoration, requiring one actual callback; the live road case physically cancels all preview segments, opens and requires the real system ring, then verifies its destruction before desktop teardown. No coverage exclusions or product changes. Own Debug/Werror max2 build and dedicated eight cases/270 assertions pass; full Debug Test_splitscreen583 cases/455541 assertions pass. Existing original-source four-case/17-intended-failure negative evidence remains valid for unchanged product behavior; this repair adds positive observer/cleanup coverage. Read-only gpt-6.1-sol supplied-diff review found no blocker; static/format/real agent tsc gates checked before commit. Draft retained until fresh all17 exact-head checks and both workflows succeed, then one tested Opus handoff. No new slice, completion budget gate, integration or Signal progress.

### 2026-10-03 — Sol PR #35 integrated (Opus)
Merged sol/controller-building-aid-shortcut at tested head 33dac968c (all exact-head checks green) as 6d1018211.
L3 now toggles building aid per owning view from world, road mode, ring, window focus and modals; in Just-watch it
leaves watching and shows all spots, B still restores. Hint bar lists L3 in each state. Reviewed diff: L3 was unbound
before, so no binding conflict. Own Release Test_splitscreen on the merge: 583/583 cases passed. Addresses part of
Deck request t_muq4pbai1teze5 (other bindings/glyphs/panel still open).

### Sol — graphical in-game controller hints (2026-10-03)

The per-view brief bar now draws Xbox-coloured A/B/X/Y badges and neutral labelled badges
for the remaining advertised button/stick inputs. Stick motion stays distinct from L3.
Measured padding and action-group wrapping preserve every hint; oversized groups use the
ordinary font's complete text fallback. Shared grouping keeps the textual KeyLine contract.
No binding, game-command route, external artwork or new fault-reporting service is added.
The existing game fault path is retained. See doc/coop/ControllerHints.md for bounded scope:
triggers/camera-axis hints, other UI surfaces and hardware appearance remain follow-up work.

Started from master afdcbe1bc and reconciled onto integrated PR35 master4ee4f87bd before final
testing. Only ROADMAP conflicted; both glyph progress and the integrated L3 mapping were preserved.
Incoming PlayerView, building-aid and ring-test files are byte-identical to master. The mapping
itself is unchanged. Sol's own GCC12 Debug/Werror cache, max two compiler jobs: all 50 hint cases
(1628 assertions), full Test_splitscreen589 cases (460240 assertions), and Test_UI137 cases
(19424 assertions) pass. Six new cases cover colours/input identities, measured wrapping and
fallback, bounded badge geometry, four physical controller ring entries, and actual rectangle
emission. Existing physical routes, mouse/keyboard and real-game command-absence checks remain green.

Executed negative control removes ONLY the production badge-emission call. Both selected cases
execute: the font GL emitter still passes54 assertions, while the rectangle emitter fails its
intended required-badge assertion (4 rectangles expected at that point, only panel/stripe2 exist).
The desktop is restored byte-for-byte, rebuilt, and the complete suites above pass. No exclusions.
Initial exact gpt-6.1-sol read-only review identified this missing actual-draw guard; the production
DrawBrief rectangle-sink body and physical test address it. Final combined supplied-diff review
finds no concrete blocker. Format10, static validation, include guard/private-marker audit and
real agent tsc gate pass; targeted tidy has no new KeyGlyph.cpp diagnostics.

Draft companion PR requires all exact-head CI checks and both complete workflows before tested
handoff. Opus owns integration. Geometry/font/rectangle calls are verified, not visible Deck pixels.


### Sol PR36 diagnostic coverage repair (2026-10-03)

The gcc-10 job at 786b9b530a92a5bac5c693b6c43c3ac1a89aecc9 passed all44 test suites
but rejected two unexecuted lines in the test-only KeyHint ostream diagnostic formatter.
The existing label-contract case now executes that formatter for three distinct buttons/actions
and checks its complete output. No exclusion, product or fixture lifecycle change.
Own Debug/Werror max2 affected hint suite is rerun before commit; original full Debug589/UI137,
executed badge-omission controls and actual rectangle/font sink evidence remain applicable to
unchanged product code. Exactly gpt-6.1-sol read-only supplied-diff review found no blocker;
format10, static validation and actual agent tsc gates pass before commit. Require fresh all17
exact-head checks and both complete workflows before one tested Opus handoff. Keep draft and
unfinished source/test claims; no new slice, completion budget gate or Signal notification.

### 2026-10-03 — Sol PR #36 integrated (contextual controller button glyphs)
Fast-forwarded master to the exact tested head 906575eb9 (based on 4ee4f87bd). Opus review of the
diff: no blockers (nit: yellow badge contrast compares the colour literal; fallback rows are plain
text by design). Local Release rebuild (reconfigure for the new KeyGlyph.cpp): Test_splitscreen and
Test_UI pass. All 17 exact-head CI checks green. Follow-up from t_muq4pb2l1srln1 remains:
trigger/camera-axis hints, other hint surfaces, Deck hardware look.


## 2026-10-03 — Sol Stock controller regression slice

Branch `sol/inventory-controller-browsing`, based on integrated master `4425ceb6d`.
New `PadInventoryTests` drives the actual in-game Back/system ring/Main selection/Stock
entry, focus and A page switching. A second real storehouse contains distinct wood and
carpenters so an HQ-only display cannot pass the whole-realm assertion. Counts are compared
with the live aggregate, including refresh after real HQ seeding, zero/nonzero colours and
noninteractive stock icons/hidden warehouse-policy overlays. Five nations assert distinct
shield textures and the canonical shield count; all eight wine/leather/charburner policies
check row availability/counts and armored-soldier details. Mouse page buttons, keyboard Escape,
controller Help/Back/reopen and explicit normal/exceptional settings restoration are covered.
The exceptional case asserts it reached its deliberate probe while Stock was open.

The live browsing recording contains zero actual game commands after pumping the real
network; a save-backed recording with one military command is the nonzero counter control.
Stock pages remain usable in the actual replay, which ends at the recorded final GF and
checksum with no client/desync callback. All windows die before persistent-settings restoration
and backend transitions. No new singleton instance, renderer seam or product change.

Validation in own GCC12 Debug/Werror cache, at most two compiler jobs: all six new cases
pass (30151 assertions). Production sabotages executed three matched cases: off-by-one
counts, unconverted nation shield icons and disabled-addon leakage produce 671 intended
assertion failures. A separate HQ-only inventory sabotage executes the strengthened real
warehouse case and produces 24 intended count failures. Production files restored byte-for-byte.
Full 595-case Debug regression passes (489977 assertions).
Clang-format 10, repository static validation and actual agent TypeScript gate pass.
Read-only review on exactly gpt-6.1-sol fixed the exact armor translation key and the
HQ-versus-total oracle; final review finds no remaining concrete code blocker. Its correction
from 34 to 24 HQ-only failures is reflected above. LLVM23 targeted tidy adds only newer
style diagnostics also emitted for existing fixtures; CI's Clang18 result remains required.

No panel shell/rendering/Stock-page implementation, hardware or release claim. Existing
ntfy fault reporting stays unchanged; no private endpoint in this test-only branch. Await
ALL exact-head CI jobs and both complete workflows before marking the PR tested and handing
it to Opus through the report API. Opus owns integration. Ten audited independent ready
backlog tasks remain; economic-progress browsing replaces this consumed inventory slice.

### 2026-10-03 — PR #37 integrated (Opus)

Merged `sol/inventory-controller-browsing` at exact head `45d02bb32` (merge `4164c3023`); test-only,
no product change. All 17 exact-head checks and both workflows were green. Local Release cache
`build/dev` after reconfigure (new test source): PadInventoryTests 6/6 cases (30157 assertions),
full Test_splitscreen 595/595 (497279 assertions). Sol's NOTES/ROADMAP "CI required" wording above is
its pre-CI checkpoint; CI evidence is in the PR. Panel Stock page remains shell slice 2.


## Sol camera and trigger hints (2026-10-03; local gates passed, exact-head CI pending)

Bounded follow-up to the Deck mapping/glyph requests: typed right-stick camera and LT/RT zoom
hints follow existing legacy world, road, ring, focused-window/modal and Just watch routing.
Only the actual target-zoom limits suppress a direction; axes never masquerade as R3 or shoulders.
No binding, simulation, network command or mouse-only brief policy changes. Updated the player
changelog and ControllerHints/ControllerMapping docs; the future panel keeps its separate motion policy.

Own GCC12 Debug/Werror cache, at most two compiler jobs: eight new cases/392 assertions pass;
58 combined new/existing hint cases/2093 assertions pass. Full603 cases/488556 assertions pass.
Physical four-seat pan/zoom limits, opposing triggers, unchanged neighbor/cursor, ring/watch exits,
pending dropdown and modal choices, actual road preview, two shared-tribe views, zero recorded
commands and executed local/live exception cleanup are covered. Neutral badge layout and narrow
text fallback preserve every input/action. The German wordFor test helper needed its missing
KeyInput::Button guard: default axis button storage is irrelevant, not A. The repaired full suite passes.

Executed production controls: inverting camera capability guards fails three matched cases at12
intended input assertions; ignoring zoom limits fails one four-seat case at8 intended min/max
assertions (exit201). Restored all six tested source/test hashes, rebuilt, and reran all58 affected
cases. Full603 success uses those same source/test hashes. Read-only exact gpt-6.1-sol snapshot
and final live exception-probe reviews have no concrete blocker; initial review tightened typed
array/positive-effect test oracles. Format10, repository static validation, diff/private-marker
checks and the actual agent TypeScript gate pass. Auxiliary LLVM23 tidy has newer Boost/macro and
existing fixture/header warnings; CI Clang18 remains authoritative.

While testing, master added only doc/ai/Goals.md and doc/coop/MenuRedesign.md (2bc7266aa).
Claimed the incoming files, fast-forwarded with all six source/test hashes unchanged, and released
those completed incoming scopes. Ready backlog now defers couch-join implementation until the new
pre-game proposal has bounded ROADMAP slices; audited navigation-only Ship-register coverage replaces
it, retaining ten independent ready slices. Exact-head CI and tested Opus handoff still required;
no hardware, panel, release or full pre-game redesign claim. Existing fault reporting is retained.

## 2026-10-03 — PR #38 integrated (controller camera/zoom hints)

Merged sol/controller-camera-hints at exact tested head 1d4b4ef43 (CI 19/19 green: Unit tests run
37151846878, Static analysis 37151846884). Opus re-tested the merge on master: Release build of
Test_splitscreen in build/dev, full suite "No errors detected". Read-only gpt-6.1-sol review of the
source diff: no concrete defects. Hints now name right stick (camera) and LT/RT (zoom, hidden at
the zoom limit) in world, road, ring, window and Just watch. Open: hardware appearance, panel runtime.

## Sol companion — controller Building overview (2026-10-03)

- Branch `sol/building-overview-browsing`, from master `89b04c9ef` after integrated PR38.
  Claim `building-overview-browsing`: new dedicated test, existing dummy Loader helper,
  temporary `iwBuildings.cpp` negative control and NOTES/ROADMAP.
- Six physical-input regressions enter through real Back/system/Main-menu events and select
  actual GamePlayer registries. Duplicate ordinary buildings are registered opposite spatial
  order. Ordinary/military/warehouse/temple child type, GUI identity, title and wrapped owner
  camera center are checked independently. Empty categories, Help, keyboard/mouse return,
  five nations/eight addon combinations, D-pad row transitions and disappearance of the first
  then last temple are exercised. Live browsing flushes to zero recorded commands. A real
  save/rehost/record/replay preserves actual seeded buildings and the final GF/checksum.
- `LoadDummyBuildingFiles` now supplies distinct synthetic nation icons and Wine ware/job/
  default-temple UI sprites, preserving loaded archives and repeat-load identity. The prior
  helper supplied building sprites but left overview button images null. Direct factory
  seeds need empty neighboring nodes for flag/castle extensions too; cached building quality
  alone is insufficient after direct seeding.
- Test-body guarded initialization/cleanup avoids Boost auto-detected `setup()` and throwing
  fixture destructors. Windows/desktops die before the saved persistent map is restored;
  normal and reached deliberate exceptional probes check restoration before destruction.
  Desktop activation/input frames reinstall the real-client observer.
- Own GCC12 Debug/Werror, at most two compiler jobs. Final temple/replay/exception run:
  three cases/1027 assertions pass. UI: all 137 cases/15498 assertions pass. Full splitscreen:
  all 609 cases/498194 assertions pass, including all six new cases/8849 assertions. Reversing the real first-match
  search executed one case with 445 assertions: seven intended identity/camera failures (exit201,
  no abort). Restored `iwBuildings.cpp` is byte-identical to master. Earlier missing assets,
  focus/filter failures and aborted unexecuted cases are development history, not evidence.
- Exactly `gpt-6.1-sol` read-only reviews: initial oracle/navigation/command-flush points
  addressed; final combined and Wine asset refinement reviews have no blockers. Format10,
  diff/private-string checks and actual agent TypeScript gate precede commit. Targeted local
  Clang-Tidy23 has no new-test diagnostics with its newer multiple-inheritance/internal-linkage
  checks filtered; its other new checks flag existing code. Full branch Clang18/coverage CI
  remains a mandatory gate, not inferred from that local check.
- Legacy single-view coverage and test tooling only: no window/router/simulation changes,
  successful harbor target, drawn numeric-count assertion, shared-view/panel integration,
  hardware appearance or release acceptance. Existing ntfy/CI reporting stays; no private
  endpoint in public sources. Draft until every exact-head check and BOTH complete Unit tests/
  Static analysis workflows pass; Sol never merges its own PR.

## 2026-10-03 — menu redesign and AI goals on the roadmap

ROADMAP M3 now lists the controller-first pre-game menu with slices 1a–6 (Jan asked, t_mustmg803mw4fb;
design doc/coop/MenuRedesign.md). M8 gained the "AI handles every setting / uses economy addons" line
linking doc/ai/Goals.md. Docs only. Released v0.1.10 for the PR #38 controller hints already in Unreleased.

## Sol companion — PR39 documentation reconciliation (2026-10-03)

Merged current master `9e4950d7e` after the primary released its documentation claims.
Preserved both workers' notes and roadmap slices. Incoming master changes only NOTES,
ROADMAP and CHANGELOG; all tested source and test bytes remain identical to `b37b169fa`.
The prior local Debug and executed negative-control evidence above still applies.
Fresh complete exact-head Unit tests and Static analysis are required before the tested
PR39 handoff; keep draft until that gate passes.

## 2026-10-04 — Opus integrated Sol PR #39 (controller building overview regressions)
Reviewed exact head 9bdb6774d (17/17 checks green; Sol's Debug/Werror runs and gpt-6.1-sol review no blocker).
Loader change only fills archives missing in tests (dummy nation icons, wine_bobs UI sprites); production loading unchanged.
Merged as 05b92032b, re-ran `PadBuildingsTests` locally on merged master (Release): 6 cases, 8857 assertions passed.
Branch deleted. Test-only change, so no CHANGELOG entry and no release. Still open per Sol: harbor success, drawn numeric counts, shared-view/panel/hardware.

## Wochenreview 2026-10-04
Direction: mostly on track, with three corrections. Week 2: 79 commits, Sol PRs #26–#39 integrated, master
CI green at d4348cca5 (Unit tests + Static analysis), no open issues, no fault reports. Claude week 21 %
(pace 15 %) and Codex well used, so quota is not being wasted. Schedules unchanged and still right (work
Mon/Wed/Fri, filler every 3 h gated, fork sweep 8th/22nd — first one on 2026-10-08, this review Sun).
Found and fixed:
- ⚠ **0.1.10 was never released.** The 2026-10-03 note said "Released v0.1.10", but no tag was pushed;
  the last release was 0.1.9 (2026-09-30), so four days of player-visible work (save resume, dropdowns,
  lobby layout, Steam names, hints, L3 shortcut, merchandise fix) never reached Jan. Tagged v0.1.10 on
  d4348cca5 and pushed. New rule in CLAUDE.md and the work-session prompts: after tagging, verify the
  GitHub release exists before writing "released".
- **Sol drifted into legacy-window test coverage.** Most of PRs #26–#39 add physical-input regressions
  for floating economy/statistics windows that the controller panel and menu redesign will replace. The
  bugs they found were real (merchandise totals, replay edits, post selection), but the rest of
  Sol's ready backlog is more of the same. Added a review priority in sol-backlog.json: after PR40, menu
  1b (couch join), Deck detection, panel slice 2; legacy-window slices only when nothing else is ready.
- **ROADMAP had a 100-line progress log inside one bullet and stale states.** Compressed the controller
  navigation bullet; marked the lobby checkbox (#31) and dropdown (#34) items done; removed "CI pending"
  from integrated items; merged the couch-join screen into menu redesign 1a/1b/6 (no separate screen).
  Added "Current priorities" at the top of ROADMAP.md; the work-session/filler/sweep prompts now point there
  instead of "M0 first" and the stale "focus: shared local views" line.
- NOTES.md was 2000 lines, read by every cold session: moved everything up to 2026-10-02 to NOTES-archive.md.
- Opus did mostly integration this week (plus save resume and M3c step 3). M3c step 4 is the next Opus item.
Not for Jan: nothing needs his decision. Still unverified on real hardware: Deck appearance of hints/names,
campaign GUI (victory/locked buttons) — waits for his next Deck test of 0.1.10.
Housekeeping noted, not done: /app/agent/data/siedler holds hundreds of Sol scratch logs per slice
(state, not repo); harmless, but Sol could prune finished-PR artefacts.


## Sol companion — lobby player-card model (2026-10-04)

Accepted MenuRedesign slice 1a adds a pure per-seat model with four stable rows
(colour, nation, team, shared tribe), clamped vertical navigation, bidirectional
value proposals and bounded taken-colour skipping. Locks retain focus and a
displayable campaign/ownership reason. Authoritative snapshots and returned
proposals are owned copies; refresh retains seat identity/focus. The accepting
shell must resolve pending input, revalidate permissions and apply only the
selected field, so an older proposal cannot overwrite unrelated broadcasts.
There are no singleton, GUI, input-router or network changes.

Ten new unit cases cover every palette entry in both directions, every pair of
current/sole-free colours, full exhaustion/custom colours, taken-colour-only
refresh, lobby nation presentation order, all nine team policies, shared-tribe
proposals, campaign/read-only lock reasons, malformed row ids and independent
shared-view focus. Exactly gpt-6.1-sol read-only review found a Boost optional
diagnostic issue; assertions now use has_value(), availability-only refresh
coverage was added, and final review found no remaining blocker.
Own GCC12 Debug/Werror cache, max two compiler jobs: all68 simple cases /
79968 assertions passed, including new10 cases /2079 assertions. The executed
colour-filter and lock-bypass controls each ran one intended case and failed
5/9 assertions (exit201, no abort); restored source/full suite pass. Clang-format10,
repository static validation, actual agent tsc and targeted Clang23 checks pass
(the latter excludes only newer trailing-comma/internal-linkage diagnostics).
All17 exact-head CI checks and both whole workflows remain required before
tested handoff.

Slice 1b remains separate and must wait for integration of this model: physical
controller routing, card drawing/geometry, accepting/revalidating proposals and
actual server broadcasts are not proven here. Remote cards/cursor colours,
panel shell, hardware appearance and packaged release acceptance remain open.
Existing ntfy/CI fault reporting remains in place; this pure helper has no
background job or runtime error endpoint. PR39's repaired head9bdb6774d passed
all17 checks and both complete workflows and was handed off once to Opus before
this new slice; its earlier pending wording is historical.

Merged current master d4348cca5 (integrated PR39); preserved both workers' notes.
All three model/test files stay byte-identical to locally tested a2248e566;
incoming Loader/building-overview test match the previously tested PR39 head.
Own combined Debug/Werror build, max2 jobs: Test_simple68 cases /79849 assertions
and Test_UI137 cases /13058 assertions passed. The model suite remains10 cases
/2079 assertions. Model negative controls/review remain valid; combined actual
agent tsc/static/diff checks pass. Fresh resulting-head CI is still required.


### Sol PR40 Windows iterator repair (2026-10-04)

MSVC uses checked std::array iterators rather than raw pointers. The player-card nextIndex helper now deduces the iterator with const auto, preserving its find, distance and wrap behavior. This repairs the four Windows compile failures reported on original PR40 head c10aa46; existing model tests cover all palette/nation/team paths. The repaired head must pass fresh exact-head CI (all 17 checks and both full workflows) before tested handoff. The existing sole PR40 continuation is retained; no second schedule or new slice.


## Sol PR40 — weekly-review reconciliation (2026-10-04)

Merged documentation-only master 51924790e, preserving the new NOTES archive,
weekly review, current roadmap priorities and the complete Sol model notes.
The Windows repair above is history: repaired implementation 44b4af8e3 uses an
inline distance/find index so MSVC checked iterators and Clang18 qualified-auto
both work. Its own Debug simple68 /79664 and new10 /2079 pass; earlier Debug
UI137 /13058 and executed colour/lock controls remain valid. Every non-Markdown
tracked file is byte-identical to that tested head; no redundant rebuild.
Static validation, diff checks and the actual agent tsc gate pass before commit.
Fresh resulting-head all17 checks AND both complete workflows are still required
before ready or tested handoff. Keep one synchronized PR40 continuation and only
the three unfinished model/test claims; no new slice until the gate completes.
After integration, prioritize menu1b/couch join, Deck detection and panel shell
over more legacy-window coverage, as the primary weekly review requests.


## 2026-10-04 — Sol: Steam Deck first-run display profile

PR40 pure MenuRedesign1a exact head5c8ba78785abcedbbae3bca508c6f4ec7cfcad6b passed
all17 checks and both complete workflows (Unit tests37176328989, Static analysis37176328988).
Ready body records final Debug and exact-head evidence; once-only tested primary handoff
t_mutd664x176om4 delivered, finished claims and scheduledf890c6be cleared. Opus integrated
b38650241/05e1d9a23. Completion budget permits work (session30%, weekly67%).

Fresh sol/steam-deck-first-run from current origin/master05e1d9a23 claims bounded settings,
startup/options, helper, dedicated tests and documentation. Deck detection accepts exact Steam
0/1 override or Valve+Jupiter/Galileo DMI pair; no SteamOS/controller/resolution guess. Missing
DMI is normal. First-run auto profile persists independently of future detection; older configs
without steam_deck_ui retain old behavior, and explicit percentages/TV precedence stay in place.
640-unit reference height yields125% at1280x800 with existing two-axis resize bounds; TV-off
returns to the profile instead of clearing it. Settings tests use the actual singleton and
copy only public values; explicit cleanup restores them and the launch environment before
fixture destruction. UI tests cover actual mouse TV toggles and scale/bounds, with deliberate
exceptional cleanup probes. Own GCC12 Debug/Werror max2 build passes. New detection3cases109assertions, settings5cases69
assertions and UI3cases58assertions pass. Restored complete simple71cases80056assertions,
integration214cases93783assertions and UI140cases7401assertions pass. Executed omission
negative controls: first-run detection removal runs one matching case and fails one intended
assertion; old TV-off reference0 runs one matching mouse case and fails three intended
assertions (bothexit201). Both product hashes restored byte-for-byte and rebuilt before full
positive passes. Exactly gpt-6.1-sol initial/final supplied read-only reviews have no blockers.
Existing Options/TV/controller regression Debug binary is being rebuilt; all17 fresh exact-head
CI and both complete workflows still required before tested primary handoff.
No MenuRedesign1b/panel/layout/hardware/release proof implied; existing fault reporting retained.


## 2026-10-04 — Sol: local lobby player cards (MenuRedesign 1b)

Local lobby seats now show colour swatches, nation, team and shared-tribe rows,
with independent model focus per seat. D-pad up/down selects a row; left/right
sends one field through the existing server controller. Only server broadcasts
change the displayed simulation values. Host shoulders reach the ordinary
settings and Start controls; guests stay on their own card. A joins an unclaimed
seat and keeps a claimed seat; B retains the existing guest stand-up / host
leave-confirmation policy. Mouse player controls and the separate seat-mode
checkbox remain available. Changing mode still stands the guests up.

Permissions are read again before sending: loaded games, Lua own/AI locks and
shared guests keep their fields read-only with a displayed reason. A revoked
local Dummy seat cannot continue editing between a server broadcast and paint.
Local Free slots become normal AI on the server; the regression observes that
actual authority change, then a Locked broadcast, before drawing. Router seats
are not compacted during card value dispatch. Existing join/leave operations
reassign them; residual queued edges on changed slots are consumed so a B/right
burst cannot edit the next controller's card. Owned entry focus follows each
controller when view-slot compaction reconciles the focus paths. Host modal windows reject guest input;
Start cannot bypass them, including B and Start in the same event batch.

The common menu router uses an explicit desktop opt-in for pre-control commands.
An initial broad route broke existing dropdown and Intro A-fallback regressions;
those failures were caught in the full Debug suite and the opt-in preserves the
original focused-control contract. The three physical Start acceptance paths
now use shoulders to reach Start from the new host-card entry focus.

Twelve dedicated driver-input / real loopback tests cover four-controller row
ownership and slot compaction, field roundtrips with independent expectations,
colour skipping, mouse controls, shared/campaign/save locks, script locks,
disconnects and host closure, bounds, modal ownership, batched modal creation and
analog escape suppression. Own GCC12 Debug/Werror build, maximum two compiler
jobs. Restored complete Debug splitscreen621cases/499946assertions and
UI140cases/16817assertions pass; new12cases/368assertions and combined
cards/dropdown/Intro24cases/582assertions pass. Three executed one-case omission
controls fail the intended assertions: live authority2, modal ownership1 and
queued-slot remap1 (all exit201). Both product hashes restored byte-for-byte,
rebuilt, then full positive suites passed. Format10, static validation, diff and
actual agent tsc pass. Local Clang23 reports existing/newer header diagnostics;
new/changed source lines have no findings, pinned Clang18 remains the CI gate.
Exactly gpt-6.1-sol read-only reviews led to live authority, modal and opt-in
corrections; final source/opt-in/remap reviews found no blocker.
Fresh all17 exact-head checks and both complete workflows remain the tested
handoff gate. Existing ntfy/CI fault reporting retained; no new background job or
private reporting endpoint. Remote cards/cursor colours (1c), nonmodal rules
drawer (2b), main-menu couch join, panel runtime and hardware appearance remain
separate work. No release or full-menu acceptance is implied.


## 2026-10-04 — Sol: live lobby roster and shared cursor preview (MenuRedesign 1c)

From integrated PR42/43 master8b6cdf851, the lobby now offers Player cards: a live,
read-only roster of used player slots and individual co-players, four cards per page.
It shows authoritative colour/nation/team/shared-tribe values, follows member names,
leader value changes and departures, and clamps the page after slots disappear. Existing
lobby editing and chat stay available; the roster sends no changes. Long names have
mouse tooltips. This deliberately adds a browsing view instead of hiding the normal
editing controls when a controller is connected.

Shared local seat cards show their cursor/view swatch before play. One common helper
retains the existing in-game SeatColor policy: shared views use the palette by game
view index, ordinary players use their tribe colour. Stable lobby seat ids must be
converted to the rank of the taken seat, matching ApplyLocalSeats; leaving a middle
seat updates the remaining previews. With only one remaining view the special
swatch disappears. Remote members display the leader's tribe values; no new network
colour, preference or save format is introduced.

Own GCC12 Debug/Werror cache, maximum2 compiler jobs: all625 Test_splitscreen cases /
499507 assertions pass at the final source. All56 targeted lobby/seat/layout/return/
shared-view cases /5137 assertions passed; four new cases /589 assertions passed initially (final full run: four /585).
The new suite was confirmed after CMake configure. A raw authenticated loopback
peer uses the real GameServer, decoder and singleton GameClient, never a second
GameClient. Tests change the peer's values, convert it to a co-player, rename it,
update its leader and disconnect it while the window is open; browse via physical
pad/mouse, preserve normal mouse editing, page through eight real AI slots, close
those slots through the server, and leave a middle shared seat physically.

Executed negative controls each matched one case and failed one intended assertion
(exit201): omitting the live roster Refresh prevents the authenticated peer's new
values arriving in the displayed snapshot; using the stable seat id produces the
wrong remaining cursor colour after the middle seat leaves. Both original product
files were restored byte-for-byte, then only arithmetic parentheses changed; the
restored final source was rebuilt before the complete positive suite. No coverage
exclusions were added. Exactly gpt-6.1-sol supplied read-only review found no concrete
blocker. Clang-format10, static validation, diff/private-string checks and the actual
agent tsc gate pass. Local Clang23 arithmetic diagnostics were fixed; remaining
local diagnostics concern existing headers and newer style checks on the established
fixture/algorithm pattern. Pinned Clang18 CI is still the authoritative lint gate.

Fresh exact-head all17 checks AND both complete Unit tests/Static analysis workflows
remain required before tested handoff to Opus. No real Deck rendering, controller
panel, rules drawer, editable remote cursors or packaged release is claimed. Existing
ntfy/runtime and GitHub CI fault reporting are retained; the read-only view adds no
background service or separate reporting endpoint.
