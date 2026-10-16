# Session notes

## 2026-10-16 — Pre-coop saves (roadmap siedler-pre-coop-saves done)

Quota filler (t_mvasqz7bzidbb). No open issues. A save written by UNMODIFIED upstream now guards loading old saves:
`.github/workflows/upstream-save-fixture.yml` (dispatch only) checks out upstream 94af63d6 (our merge base), adds only
`tools/upstream-save` (a Boost test driver) and replays upstream's own `200kGFs.rpl` (Big Slaughter v2, human "Alex" +
7 hard AIs) with checksum checks to GF 3000, then saves with upstream's Savegame. Run 38887309810, sha256 bc488b66…,
format 4.1 (no s25coop start-goods field). Fixture + provenance + README in tests/testData/saves.
Tests: FrontEndLoadTests/UpstreamSaveResumesWithTwoJoinedPlayersAndKeepsItsAiTribes and
ContinueResumesUpstreamSaveWithFourPlayers (party page, shared views, AIs kept, 12 GFs no async) and
CoopNet_UpstreamSaveResume (online co-player's woodcutter lands on p0, 600 GFs in sync, 7 AIs unchanged) pass locally
(Debug). Negative control: GetPlayerInfoVersion forced to 2 → the upstream save no longer loads (restored).
Local quirks: run Test_splitscreen with USER=coop TMPDIR=build/tmpups (copy_file across /tmp fails otherwise).
Gotchas for the workflow: the build needs build/share/s25rttr/RTTR (symlink to data/RTTR); BOOST_TEST on a
unique_ptr does not compile (cast to bool). gh is not installed here; workflows are dispatched with curl + the git
credential. Open: optional AI takeover (M2 item 2), Deck. No player-visible change, so no release.

## 2026-10-16 — F7d online party page (roadmap siedler-online-party-cards done); 0.1.20 released

Quota filler (t_mvaqgtlh9xw4k). No open issues. 47f6e2c7d was green, so 0.1.20 was tagged by hand (b27d280e1,
tag v0.1.20) instead of waiting for continuation c3af4bdf, which is removed. Release workflow was still queued
at the end of this note; check that its assets exist before announcing.
F7d: `dskParty` now also serves network games from the new menus (`IsOnline()`): the host of an ordinary map
(Direct/LAN/Internet) and every joined player who is not a co-player. One card per tribe (`ID_Net + slot`), the
own one editable (host via GameLobbyController, a joined player via GameClient commands, which un-ready him),
others read-only with name/ready/ping/co-players; join line (IPv4 of the default route + port, lobby name, or
"Hosted by"), chat, Start = start/cancel for the host (refuses with the not-ready names, closes or fills open
slots, restores them on cancel), Ready/Not Ready for a joined player. Host can remove a player (A/click on the
card, Yes) and toggle co-players; a joined player can become co-player, which switches to dskGameLobby.
Mouse: a click on a card row steps that row. Details doc/coop/FrontEnd.md (Party). New: GameClient::GetServerPort,
ctrlChat::GetNumMessages, ctrlLobbyPlayerCard::SetActivatable.
Review: Opus subagent read-only (Codex is off): 9 findings; fixed removal, co-player toggle, mouse edit, Rules
during countdown, refused-start text; accepted/noted: join race at Start, IPv4-only line, no lobby listener.
Tests (Debug, -j2, focused per the new VPS rule): FrontEndOnline (incl. 2 new real-socket cases), FrontEndParty,
FrontEndInternet, FrontEndMaps/Load/Campaigns pass; Test_LanguageFiles pass; msgfmt -c clean. Full suites: CI.
Gotcha: a lobby CI_* callback can reach the LOCAL party page too (CI_CoopMembersChanged arrives in local games);
every online-only update must check online_. Not seen: real screens (no Xvfb run), real second PC, Deck.
Working rule from Jan (2026-10-16): heavy builds/test suites/RE go to the laptop (re lane) or CI; here only
incremental -j2 builds and a focused test.
Release: one-shot continuation 9bf22a81 (11:07 UTC) verifies v0.1.20 assets, tags 0.1.21 once 9b0cb74b5 CI is green, announces.
Next: F7e (network campaigns/saves + co-players on the party page).

## 2026-10-16 — F7c: ordinary maps on the party page, rules drawer (roadmap siedler-rules-drawer done)

Same session. `dskParty::IsFor` is now every self-hosted local start from the new menus. Ordinary maps seat each
joined controller on its own tribe; a party bigger than the free tribes plays one tribe together without asking.
Host switches mode on its card's Tribes row or in the rules (stand all, rebuild, re-seat the same controllers;
a switch that would leave someone out is refused). "Rules" opens an in-page drawer (goals, goods, exploration,
speed, computer players, lock teams, team view, random start, addon window). Design detail in
doc/coop/FrontEnd.md (Party). Two things worth knowing:
- The server singleton keeps goal/speed/exploration etc. between games of one process (LoadSettings only resets
  addons). Tests that change rules must set them back, or later suites fail (seen: economy question blocks
  starts, visibility tests break). The host keeps its own `rules_` and adopts the server echo only when no send
  is outstanding (`pendingRules_`); without that a stale echo undid a newer edit (negative control fails).
- Computer players are applied at Start, only if the host touched the rule (else map/Lua levels stay); slots it
  closed are reopened when the server refuses the start.
Review: no Codex (switched off); an Opus subagent reviewed read-only: 5 minor findings, all fixed (Lua levels,
reopen after refused start, stand-up race at Start, StandAll before auto-together, focus on disabled Tribes row)
plus CI_ReadyChanged → Lua onPlayerReady for AIs like the classic lobby. Tests: FrontEndPartyTests +5 cases,
FrontEndMaps start now lands on the party page; negative controls (no pendingRules_ guard, no auto-together)
fail. Own Debug max2: full Test_splitscreen, Test_UI, Test_LanguageFiles pass; clang-tidy 18 and format10
clean on changed files. 36 new strings in rttr.pot, German for all. Seen under Xvfb 1280×800 (party page with
"Tribe: Own", drawer): data/siedler/partytest/s4.png, s5.png. Not seen: real pads/Deck.
Noticed: two orphaned Xvfb (:91/:92) of data/opents-run since 2026-10-01 at ~60% CPU each — reported to the
opents lane (t_mvad81ug2oa77), not touched.
Next: F7d online party cards (siedler-online-party-cards).

## 2026-10-16 — PR59 online menus integrated (F8c/F8d); roadmap item siedler-online-menus done

Work session (bfa0b45f) + Sol tick t_mva1lr2x4qv5v, both on Opus (Codex is switched off since 2026-10-15, so
there is no gpt-6.1-sol review any more; the diff was reviewed by Opus itself). No open issues. PR59 head c140b26e4
(two Opus fixture fixes after Sol's 9f91a4de7: empty typed fields, clang-tidy use-after-move) had all 17 checks and
both workflows green (Unit 38753753395, Static 38753753344). Own review: ctrlConnectionProgress mirrors iwConnecting
(party/lobby switch on Finished, GAMECLIENT.Stop also stops the in-process server on Abort); host form revalidates
campaign/save before HostGame. Merged --no-ff into master; own Debug max2 on the merge: Test_UI passes; the full local Test_splitscreen
first failed one check: NetworkMapHost… compared the status with the English literal, but in the full local run an
earlier test leaves en_GB loaded ("…for the game."). The five literal status checks in testFrontEndOnline.cpp now
compare with _(); online+Internet suites and the full Test_splitscreen pass after that. CHANGELOG Unreleased has the player lines; release 0.1.20 once master CI is green.
Release 0.1.20 (PR59 + F7c): one-shot continuation (04:30 UTC) tags it once c5b4df4a3 CI is green.
c5b4df4a3 CI was red (2026-10-16): MSVC ambiguous GetCtrl in the dskParty::UpdateRules lambda (now via a Window& reference) and gcc-10 coverage on the port-retry branch of testFrontEndParty (LCOV_EXCL). Fixed on master; continuation c3af4bdf checks the new head.

## 2026-10-15 — Sol F8c Internet lobby, hosting and connection progress (local gates passed; PR59 CI pending)

`sol/frontend-online-menus` adds full-screen Internet login/server list/chat and a network
host path through Campaigns/Maps/Load, with a final masked creation form. Campaign identity/
unlocks and saves are revalidated at Start; campaign co-players are enabled. Classic stays
reachable. The final step replaces network handshakes with full-screen status/map progress and
Abort on the original form; Classic keeps its connecting window. F7d remote cards and
local seating on an online host are separate roadmap items.

Read-only gpt-6.1-sol review found saved-hash retry provenance and changed-save handling;
both are repaired. Own GCC12 Debug/max2 builds pass; all123 FrontEnd* tests/6769 assertions and full
Test_UI174/127328 pass. The online suite18/1290 covers all five abort inputs, resize,
private-field retention, progress callback bounds and active-handshake destruction. The new Internet suite5/332 exercises actual loopback login,
rejection/retry after disabling saved passwords, chat and live server-list updates.
Network map/campaign/save starts use real client/server sockets; changed campaign and
deleted map/save recovery are covered. Only existing original-resource fixtures are
bypassed. No physical Deck, public-service or router/NAT acceptance is claimed.
Format10, static validation/include guards, all PO msgfmt checks and actual agent tsc
pass. Clang23 records newer style diagnostics and transitive singleton-destructor warnings;
the latter reproduce in unchanged dskParty. No lint/coverage suppressions or test exclusions are added; Clang18 CI
remains the authority. Earlier "Clang23 pass" wording was premature (its output was still running).
Fresh all17 checks and BOTH complete CI workflows remain the tested-handoff gate.
The first-step head eeabbed26 failed native Windows compilation at an unqualified
GetCtrl inside the Internet layout lambda (C2668); explicitly this->GetCtrl fixes it.
Old runs were canceled as obsolete, without retrying the failed source. bdbf640c2 then
passed Linux clang21 sanitizer and formatting, but macOS Test_splitscreen stopped on
SIGPIPE. The game entry point installs a no-op SIGPIPE handler; the new Internet fixture
had not reproduced that runtime environment. A POSIX-only RAII guard now installs SIG_IGN
for this fixture and restores the complete previous sigaction, including constructor
unwinding. Send/EPIPE results remain checked. No product change, skip or retry of the
failed source. Local Debug rebuild and Internet/Online23/1628 pass starting explicitly
with SIGPIPE=SIG_DFL; agent tsc/format/diff pass. Read-only Sol review: no findings.
Previous full FrontEnd123/UI174 evidence is unchanged product code; fresh macOS and all17/
BOTH CI on the repaired head remain the handoff gate. No new schedule; central fault
relay retained. Details: [OnlineInternet](doc/coop/OnlineInternet.md).

Older entries (2026-09-26 – 2026-10-02) are in `NOTES-archive.md` — grep it for durable how-tos:
local build env and `build/dev` (2026-09-27), headless harness `ai-battle --test` and `ctest -R Coop`,
S2 data in /app/agent/data/siedler/S2 (`-DRTTR_COOP_S2_DIR`), fault-report path, coop-net tests,
CI lessons (clang-tidy, coverage, GCC/Boost quirks), Sol integration routine, earlier weekly reviews.


## 2026-10-14 — Release 0.1.19

Continuation 57cdd1a3: master 81ad4f3ea (docs only after ebe8956ee) had green Unit tests (38501441079) and
Static analysis (38501441153). CHANGELOG Unreleased → 0.1.19 (99a1b98fe), tag v0.1.19, release workflow
38509252689 green; https://github.com/jarvis-jhk/s25coop/releases/tag/v0.1.19 has the Linux tar.gz, the Windows
zip and Install-s25coop.desktop. jarvis_reply with the link was attempted three times; each call was cut off without a result, so delivery to Jan is unconfirmed.

## 2026-10-14 — F7b: saves open the party page

Work session (bfa0b45f). No open issues/PRs. `dskParty::IsFor` now also takes savegames loaded from the new
menus (local, host, FrontEnd style). On a save every card is locked "Saved game": without the lock the server
accepted the host's colour/nation/team changes on a save (seen in the negative control). The party shares the
host's tribe; slots nobody here can take get a Default/Easy AI: a former local player's own slot (idle AI
"Local player N", helper moved from dskGameLobby to `LocalSeats::FormerLocalPlayers`) and a past network
player's Free slot, which `GameServer::ArePlayersReady` refuses to start with. Saved AIs stay untouched.
Tests (testFrontEndLoad.cpp): the two existing resume cases now land on dskParty and check locks/mode text and
that host D-pad input changes nothing; new OwnSlotSave… and NetworkSave… cases. Negative controls: no lock →
6 lock + 3 value failures; no conversion → AI check fails; no Free conversion → start never reaches Loading.
Note: the server's SetPlayerState echo takes a few frames — assert with until(), not immediately.
Codex gpt-6.1-sol diff review: no findings. Ordinary maps stay on dskGameLobby until F7c (toggle + rules).
Own Debug max2: full Test_splitscreen (791 cases) and Test_UI (174 cases) pass. Release 0.1.19 (F8a, F8b, saves on the party page)
is left to one-shot continuation 57cdd1a3 (04:00 UTC) once ebe8956ee CI is green.

## 2026-10-12 — Sol F8a online entry and direct join

Home's Play online tile now opens a full-screen choice page. Join Game opens a
full-screen address/port/password form with IPv6 and host-tribe co-player choices.
Controller fields use the existing staged keyboard; Confirm only commits the field,
while Connect starts the existing network handshake. Mouse, keyboard Enter, B/Escape
and visible Back share those paths. Passwords are masked and absent from settings
and page factories. Failed handshakes retain inputs for correction/retry.

Design/scope: [OnlineDirect](doc/coop/OnlineDirect.md). LAN, Internet login, host
creation and online lobby retain their existing implementations, including the
proxy guard and custom Abort/OK behavior. Classic menu returns stay intact. F8 LAN
browsing/full-screen connecting/online host adoption and F7d online cards are later
slices; F7 party integration and the shared ROADMAP remain primary-owned.

Own GCC12 Debug/Werror builds use max2 compiler jobs. New physical suite passes
10 cases/487 assertions: four sizes including800x480 and1024x640, staged controller
keyboard ownership/commit/cancel/disconnect, masked password and empty reentry,
numeric port boundaries, failed connect, keyboard Enter/Escape/mouse, legacy paths,
real IPv4 password rejection/retry, real IPv6 loopback abort/retry, host-tribe
membership and deliberately reached keyboard/handshake exceptional-cleanup probes.

Executed negative controls remove password masking and the member join request:
the matched tests fail1 and3 intended assertions. The product is restored
byte-for-byte and rebuilt before the final positive regression run. Initial physical
execution caught construction-time SetText callbacks before the status control
existed; notifications now start only after all controls and initial values exist.
Ctrl+A is unsupported by ctrlEdit; the physical replacement helper uses End and
Backspace. The initial aborted run is failure evidence, not passing coverage.

Restored final-source full Debug Test_splitscreen passes780 cases/504466 assertions
(539s). The final dedicated physical repeat passes10 cases/487 assertions.

Supplied-source read-only gpt-6.1-sol final review finds no actionable issues.
Formatter10 equality, include guards, whitespace, repository static validation and
actual agent TypeScript checks pass. Local Clang23 diagnostics from six checks absent
in CI's Clang18 catalog are recorded. Its Boost signals2 NewDelete diagnostic also
reproduces in unchanged iwDirectIPConnect; no repository suppression is added.
Exact-head Clang18/full CI remains required before tested handoff.

IPv6 validation uses the existing localhost fast path and reaches the real IPv6
loopback socket. Numeric ::1 is correctly passed to GameClient but the inherited
AI_ADDRCONFIG resolver rejects it when only IPv6 loopback is configured; reported
separately for primary follow-up. Numeric IPv6 reachability in this environment is
not claimed. Existing central public ntfy fault reporting stays active; ordinary
invalid ports/passwords/unavailable hosts are local errors. No private endpoint,
new network protocol, release or physical Deck/network hardware claim.

Base advanced cleanly from33be33dff to masterc67c70fd3 before validation, preserving
all seven own source/test hashes. Sol pushes a draft sol/frontend-online-direct PR
and requires all17 successful exact-head checks plus BOTH completed successful
Unit tests/Static analysis workflows before primary handoff; never merges its PR.

PR57 Windows portability repair: all four native Windows jobs on b20324e30
failed during Build at dskFrontEndConnect.cpp:141 (MSVC C2668), before tests.
The error callback now explicitly qualifies this->GetCtrl to select the mutable
member overload. No user behavior, tests or shared Window API changes. Rebuilt own
Debug max2 Test_splitscreen and reran Online/Home:15 cases/565 assertions pass,
including real failed-password retry, Abort/retry and host-tribe membership.
Format10, whitespace and actual agent TypeScript compiler pass before commit.
The full780-case and negative-control evidence above belongs to the original
head; repaired-head native Windows and fresh all17/BOTH CI remain pending.
No infrastructure retry was used for this deterministic source compile error.

## 2026-10-12 — Sol PR56 master reconciliation

Merged master `33be33dff756b6f0738d2d5b8feca534252cdb4b` after primary
released its ROADMAP scope. Both notes and roadmap completion states are
preserved; PR54 scenarios and PR55 translations are integrated, PR56 previews
still await fresh CI. All ten Sol preview source/test hashes are unchanged.
Own GCC12 Debug/Werror max2 configure/build completed with the inherited
scenario sources included. Preview/scenario/campaign catalog15/112213 pass;
physical Load/Save, scenarios, maps, campaigns and Home65/3990 pass.
Actual agent TypeScript and whitespace checks pass. Previous repair head
`0d141a51a` and its coverage proof remain historical evidence; fresh all17 checks
and BOTH complete workflows on this merge head are required for tested handoff.
Primary owns integration/F7/release; ROADMAP is released immediately after push.

## 2026-10-12 — Sol PR56 CI repair

Original head `3f62778d759c310e36b23d245a07ccc865ccf8b3` passed all51
CTest entries in GCC10 but failed the coverage gate: the partial-image-write
child kept RLIMIT_FSIZE active through normal exit, truncating shared gcov
profiles. GCC16 separately rejected `helpers::contains_if` over PixelBuffer
because its generic std::begin/end noexcept query diagnoses those accessors.
Original failed job logs and CI/hash snapshots are retained in Sol state.
No unchanged-head retry was requested.

The tests now use typed pixel data pointers for their nonblack predicates.
The child restores its original RLIMIT_FSIZE and SIGXFSZ disposition immediately
after the real GameClient save attempt, before reload, cleanup and normal exit,
and requires both restorations to succeed. The existing assertion still requires
a successful save, byte-identical loadable world and incomplete optional image.
No product, libsiedler2 or save-format change; no diagnostic suppression.

Own GCC12 Debug/max2: preview5/112108 and physical Load/Save20 pass; full
splitscreen754/503117 passes. A separate target-only GCC12 Debug coverage
build instruments all74 Test_splitscreen translation units with CI's no-inline
coverage flags, reusing the existing uninstrumented production/helper libraries.
The executed child partial-write and physical20 cases leave all74 profiles valid:
gcov-dump parses each, gcov reads matching notes/data without corruption.
An isolated GCOV_PREFIX run of the original-head test object executes the same
one-case save probe successfully and reproduces damaged profiles; positive
profiles remain separate and intact. This targeted local evidence does not claim
the whole GCC10 CI coverage gate or a local GCC16 build.

Read-only gpt-6.1-sol review found no actionable issue. Actual agent TypeScript,
clang-format10 and whitespace checks pass. Fresh exact-head all17/BOTH remains
required before tested handoff; primary owns integration and release.

## 2026-10-12 — Sol F6b tactical save previews

`sol/frontend-save-previews` adds a160×100 fog-aware tactical image to new
manual/autosaves. Capture reuses the existing minimap CPU colors without a
framebuffer, textures or simulation RNG consumption. The selected save row
reads a fixed optional trailer, skipping the compressed/legacy world without
loading it; old saves and invalid images keep “No preview stored”. Dimensions and
exact world-data boundary are checked before fixed allocation, pixel integrity
before display.
Copying saves preserves their pictures; overwriting truncates the old trailer.
The existing world/player formats and serialized snapshot bytes stay unchanged.

The real GameClient/save dialog writes and overwrites previews. A POSIX child
limits writes after the complete world and16 bytes of the image trailer:
SaveToFile still succeeds, the exact saved snapshot reloads, and the incomplete
preview is unavailable. Child cleanup and normal exit keep the parent’s limits,
logs and singleton lifetimes intact and allow coverage of that executed path.
Windows keeps the portable format and physical save/overwrite regressions;
the OS write-limit probe is POSIX-only. No production test hooks or new endpoint.
The existing central ntfy-to-GitHub fault path remains in place.

Own GCC12 Debug/Werror, max2 compiler jobs: new preview unit5/112108,
physical Load/Save20/1428, full UI170/122739 and splitscreen754/503566 pass.
Public-map online solo-save sync/desync/desync-after-resave:3/3 CTests,89.55s.
Executed negative controls: disabling pixel verification fails2 assertions in
one matched unit case; withholding browser pictures fails3 assertions in one
matched physical case. Restored both product files byte-for-byte before further
work. No unmatched-filter or crash exit is counted as evidence.

Incoming primary PR55 was integrated while testing. The uncommitted work was
preserved and restored on master a2baacead without conflicts; all10 own C++
source/test hashes and all31 incoming catalog/docs files were verified intact.
The completed fresh Debug build then passes preview5/112108, language1/54092
and affected physical addon/German-hint/load/save suites159/33365. Full-suite
and online evidence above is from before the translations-only base advance;
its own C++ source/tests are identical. No redundant full simulation rerun.

Read-only gpt-6.1-sol review addressed invalid world lengths and added the actual
partial-write failure probe; final supplied-code review found no actionable
issue. The inherited minimap intentionally uses current ground terrain/shading
under fog while remembering objects, ownership and roads; no new fog policy.
Later minor style and mouse-row fixture corrections are covered by the tests.

Source/local validation is complete; a draft PR and exact-head ALL17 checks plus
BOTH complete Unit tests/Static analysis workflows are required before tested
handoff. Private checkpoint/PR body carry live counts and the sole continuation.
Opus owns review/integration and releases. Upstream-produced pre-coop world
provenance, AI takeover, scene artwork and hardware acceptance remain separate.

## 2026-10-12 — PR55 integrated (addon translations)

Reviewed Sol PR55 exact head 1572fd961: translation-only (POT + 28 POs, 24 new msgids, German
translated, other languages English fallback); German strings read correctly. 17/17 checks and both
workflows green on that head; merge-tree with master clean. Merged --no-ff. Player-visible: next release
(v0.1.17) once master CI on the merge is green.

## 2026-10-11 — Sol addon translation refresh

The October9 upstream sweep added mine resource behavior, depleted-mine fallback,
single-soldier coin training and stranded-soldier return-search options. Their24
new labels/descriptions/choices were absent from the fork's POT, so a plain
msgmerge could never register them. `sol/addon-translation-refresh` appends those
exact source msgids and references without replacing the template's manually
registered pad texts. All28 PO catalogs now contain them; German has reviewed
translations, other languages keep English fallback for the new entries.

Compiled-MO comparison preserves every previously shipped message in every
language, including existing fork controller/ware translations. All24 new German
runtime lookups match the intended translations; all27 other catalogs return the
original English text. No new fuzzy guesses. `msgfmt --check` passes for28 files,
and a second msgmerge with the build's no-fuzzy options changes none of them.
The existing Russian carriage-return warning predates this slice.

Own GCC12 Debug/Werror configure and build use max2 compiler jobs.
Test_LanguageFiles passes1 case/54092 assertions; full Debug
Test_splitscreen passes752 cases/500478 assertions, including the
physical addon/category controls and German controller purpose/ware regressions. Supplied-diff read-only gpt-6.1-sol review:
no actionable findings. Static validation, markdown and actual agent TypeScript
check pass. No C++/save/network behavior or fault-handler changes; the central
ntfy-to-GitHub fault path remains active. Template refresh procedure recorded in
data/RTTR/languages/README.md.

This branch starts from master39db483b5, independently of handed-off scenario
PR54 and primary F7 party work. Exact-head all17 checks plus BOTH completed
successful Unit tests/Static analysis workflows are required before tested handoff;
Sol opens a draft PR, never merges it. CI evidence and pending continuation live
in the slice's private checkpoint and PR body. Original artwork/Deck acceptance,
F6b thumbnail/save-provenance work and release are separate.

## 2026-10-11 — PR53 integrated (F10 full-screen loading)

Reviewed Sol PR53 reconciliation head d9e603f5ee10c729eb0d74ef1d1f1749b7839c16;
all17 exact-head checks and both complete workflows passed:
https://github.com/jarvis-jhk/s25coop/actions/runs/38134708483 (Unit tests),
https://github.com/jarvis-jhk/s25coop/actions/runs/38134708507 (Static analysis).
Clean merge-tree against primary c1b96179f; both release and F10 notes retained.
All five loading/frame source and test files match that tested PR head unchanged.
Primary supplied-source inspection agrees with the recorded read-only gpt-6.1-sol
final review: no integration blocker. The reentrant failure guard prevents further
GameLoaded/timer/status advancement; Classic explicitly opts out of the frame.

Reconfigured and completed primary build/dbg (Debug, max2 jobs) before tests.
Own physical FrontEndLoadingTests11/372, full Test_splitscreen752/504662 and
Test_UI165/18217 pass. Logs: build/pr53-configure.log, pr53-build.log,
pr53-loading.log, pr53-splitscreen.log and pr53-ui.log. Actual agent TypeScript
compiler and git diff --check pass. Original S2 graphics/Deck acceptance remains
unproven; success fixtures bypass only resources/rendering, retain real views,
mission goals, timers and client/server start. Existing fault reporting retained.

CHANGELOG Unreleased records the player-visible loading screen/fault return.
F10 is complete in ROADMAP; next primary work remains F7b party, Sol F5c scenario
policy audit. No release tag until new master CI passes. A one-shot release check
will run in45 minutes for v0.1.16, verify all three assets and notify Jan only
when the release is ready. No progress reply for the automated handoff.

## 2026-10-11 — v0.1.15 released from green master 0cb2378c1

Master 0cb2378c1 (PR51 full-screen maps browser, PR52 full-screen campaign grid and mission picker)
passed Unit tests and Static analysis. Unreleased player notes (Campaigns and Maps & scenarios pages)
became 0.1.15. Annotated tag v0.1.15 points to the documentation-only release commit
cf9ef26afa9afa144aadee870e12b40b7158010b. Release workflow succeeded:
https://github.com/jarvis-jhk/s25coop/actions/runs/38139316215
Published release: https://github.com/jarvis-jhk/s25coop/releases/tag/v0.1.15
All three assets verified uploaded, nonempty and HTTP 200: Install-s25coop.desktop,
s25coop-0.1.15-linux-x86_64.tar.gz and s25coop-0.1.15-windows-x64.zip.
Pre-release source-head CI: Unit tests38133739082 and Static analysis38133739041,
both completed successfully on 0cb2378c17bda53c3ca4eec873182ee2dc7cf77b.
The documentation-only release commit's separate CI was still running at verification.

## Wochenreview 2026-10-11
Direction: on track. Week 3: 110 commits on master, Sol PRs #45–#50 integrated, four releases (0.1.11–0.1.14,
each verified with 3 assets), fork sweep of 101 upstream commits done (a day late, 10-09). Master 7046fa40a
Unit tests + Static analysis green; no issues, no fault reports. Claude week 17 % at pace 15 % (filler correctly
skipping), Sol busy on Codex (PR51 F5b map browser, 16/17 checks green at review time). Schedules unchanged and
right: work Mon/Wed/Fri, gated filler every 3 h, sweep 8th/22nd (next 10-22), this review Sun; Sol tick every 10 min
plus its one CI continuation. No code off-goal: everything this week is new front end, its tests, or integration.
Found and fixed:
- **F4 campaigns had no owner.** It is the front-end screen closest to the goal (play campaigns together) and sat
  ☐ while both workers took other screens. Assigned to Sol after F5b (ROADMAP F4 line, sol-backlog
  `reviewPriority20261011`); F6b leftovers (thumbnails, pre-coop fixture) move behind F4/F5c/F10.
- ROADMAP "Current priorities" still listed 4b/F9/F6a/F5a as Sol's upcoming work and 4b as open/CI pending —
  rewritten with what is done and the next order for both workers.
- Added a small gap item: msgmerge pass for upstream's new addon strings (German players see English since the sweep).
- Removed two stray 2026-10-05 logs (m3c4-*.log) from the repo root; logs belong in build/.
- ⚠ **VPS disk at 97 % (7.5 GB free of 193).** Siedler's share: build/dbg 4.4 G, Sol build/debug 3.2 G, ccache 4.6 G
  (max 5 G), data/siedler 3 G (2 G are installer fixtures in dl/, keep). The rest is outside this lane; reported to
  the self lane. If space gets tight before that is resolved: delete build/dev (0.4 G, Release, recreatable) first,
  never the fixtures.
Opus throughput: F7a only plus integration/sweep/releases; F7b is the next work session's item.
Not for Jan: nothing needs his decision (disk goes via the self lane). Still unverified on real hardware: everything
since 0.1.10 on the Deck — waits for his test.

## 2026-10-10 — Sol PR50 integrated (solo-save online resume)

Test tooling only (coop-net --single-player, economy/roles lines, three CoopNet_SoloSaveResume CTests).
Exact head d2ddb93e7 was all17/BOTH green. Merged onto master with F7a; own Debug coop-net build after
reconfigure, all 22 CoopNet_* CTests pass locally incl. the three new ones (build/pr50-ctest.log).

## 2026-10-10 — F7a: seat logic extracted into frontend/LocalSeats

Quota filler session. F7 split into F7a–F7d in ROADMAP. F7a moves dskGameLobby's local seat state machine
(build, join/leave, party seating, shared/own-slot apply, unplug/closed-slot drops, start check, swaps) into
`frontend/LocalSeats` without behaviour change; the lobby keeps the cards, labels and message boxes.
Own GCC12 Debug build: MenuPadSeatTests 36/36, full Test_splitscreen 710/710 (502373 assertions), Test_UI 159/159.
gpt-6.1-sol read-only review (build/f7a-review.txt): only (1) redraw after a rejected Apply — that path is
unreachable (seats are built from slots the validation allows) and the redraw only shows seat state, accepted;
(2) Sync runs twice before start, the second pass finds nothing, accepted. Next: F7b dskParty page.

## 2026-10-10 — v0.1.14 released from green master 43dbd3598

Master 43dbd3598 (Sol PRs 46–49 integrated, fork sweep of 101 upstream commits, sdl2-compat exit fix)
passed all 18 checks. Unreleased player notes (F9 options pages, F6a save browser, robust map list,
upstream addons, exit crash fix) became 0.1.14. Next: F7 full-screen party page; Sol PR50
(solo-save online resume) to review/integrate once its CI is complete.

## 2026-10-09 — fork sweep (the 2026-10-08 one, run late)

Upstream master merged (101 commits: radius hunter search, ai-battle --teams, single-soldier coin training,
configurable mine resource behaviour, stranded-soldier search, more). Decisions:
- Replay minor: s25coop shipped 8.4 = per-player start goods; upstream's 8.4 = radius hunter search. Ours
  keeps 8.4, the hunter change is 8.5 here (`nofHunter` checks compat >= 5). Our own replays stay readable;
  upstream-made 8.4 replays are not compatible with this fork (would parse start goods). Accepted.
- ai-battle: coop harness (--test/--test-script/--check-replay, objective none, replay-player ctor) kept,
  upstream's options struct and --teams adopted; teams go through GeneratePlayerInfo into the player ctor.
- external/languages stays vendored in data/RTTR/languages; upstream's catalogue refresh (new addon strings)
  does not apply to our edited catalogues (29 rejected files) — new upstream addon texts show in English until
  a msgmerge pass. Not urgent.
- Addon categories (PR45) extended for the new addons: four mine-behaviour lists and no-output fallback in
  Economy (S4-like exhaustion = harder, others easier), single-soldier coin training Combat/harder,
  stranded-soldier search Comfort/neutral. testAddonCategories covers each option. INEXHAUSTIBLE_MINES is
  now enum-only (legacy migration), so the registry test expects size-1.
- Upstream #1886 rejects absolute image paths in campaign.lua (only <RTTR_...> or campaign-relative). Our
  campaign-artwork test moved its images to <RTTR_USERDATA>/art/pics/. Upstream quirk: "<RTTR_X>/sub/file"
  (exactly two levels) is mistaken for a campaign-relative sub folder and fails the alphanumeric check.
- Upstream preset windows no longer create the PRESETS folder (the client does at startup); our addon
  category browser fixture creates it like upstream's own test.
- Local: own GCC12 Debug, full ctest 47/47 (Test_drivers excluded, no ALSA). gpt-6.1-sol read-only review:
  only finding was upstream's unchecked --teams index (dev CLI, upstream code), left as is.
- master 38b3ef439 CI: Win32 Debug FrontEndHome EveryUpstreamDesktopComesBackToItsTile failed once at the
  expectHomeFocusedOn check (passes locally and on all other configs; rerun requested); StyleAndFormatting
  failed on a Docker Hub 504 (infra). The sweep head supersedes that CI.
- Forks with work since 2026-09-25: fred01 (13 commits: waterway/carry-out addons, AI leather/wine, range
  overlay, builds) — took only the sdl2-compat crash-on-exit fix (explicit AUDIODRIVER.UnloadDriver), relevant
  for SteamOS; rest is game logic without tests in our coop context, revisit if upstreamed.
  morganchristiansson radius-overlay (23, upstream PR work, wait), robertmuehsig boats-transport-figures (1,
  game logic, wait), MichalLabuda headless-replay-player (15, we have --check-replay), AngryLocomotive rebrand — skipped.

## 2026-10-09 — Sol PRs 46/47/48/49 integrated together

PR46 (controller keyboard API), PR47 (F9 options), PR48 (F6a save browser), PR49 (F5a map catalog), each
all17/BOTH green at its exact head, merged in that order. Conflicts only in docs/NOTES/ROADMAP (kept both)
and the dskHome/testFrontEndHome include lists (both includes; Load and Options routes both kept). Own
GCC12 Debug build/dbg after reconfigure: full Test_UI 156 cases and Test_splitscreen 710 cases pass.
Read-only gpt-6.1-sol review of the combined production diff for merge interactions: no concrete issue.
Fresh master CI is the gate for the next release (player notes for options/save browser to be added then).

## 2026-10-09 — v0.1.13 released from green master 57262bbe8

Master 57262bbe8 (F7 title-party lobby seating + Windows DLL-copy lock) passed all 17 checks:
Unit tests 37604845331 and Static analysis 37604845296 both completed/success, so the native
Windows DLL-copy repair is proven. Unreleased player notes (title-party seating, addon categories
and accepted dropdown values) became 0.1.13. Open Sol PRs 46 (OSK), 47 (F9 options), 48 (F6a save
browser), 49 (F5a map catalog) are all17/BOTH green and next to integrate, one at a time with fresh
master CI. F7 full-screen party page stays the primary next item.

## 2026-10-07 — recurring Windows post-build DLL copy race repaired

Request t_muxwyuu985lu4 reports Unit tests 37597827253 on old master 70d1a5dab, failed only in
Windows Release x64 Build. Exact job 112714804282 log: ai-battle's post-build copy_if_different
could not copy libvorbisfile-3.dll into the common Release directory (MSB3073). Compilation and
links, including Test_splitscreen, completed; ctest was skipped. Same failure class as PR34's
libogg-0.dll and the 2026-10-04 libiconv2.dll incident, so the documented recurrence policy calls
for a repair rather than another retry. Full private log: data/siedler/master-70d1-win64-build.log.

The project-local GatherDll module keeps upstream discovery/cache and overrides only copying.
Each generated per-target/config script locks its actual destination directory with a bounded,
process-scoped CMake file lock, then copies the gathered DLLs; fatal errors release the lock and
fail the build. Missing-file warnings and native-Windows/cross-compilation gates are retained.
No submodule source edits and no serialized compilation: only colliding post-build writes wait.

Own tools/ci/testRuntimeDllCopy.py configures/builds two real targets with paths containing spaces,
executes their real generated copy scripts under a held lock, verifies both wait and copied bytes
match. Executed lock omission bypasses the held lock; one unprotected writer makes that control
independent of the original nondeterministic race. Both positive and negative passed on Linux;
root Debug CMake configure passed. Windows Unit tests now run this same harness natively before
ctest in all four configurations. Exactly gpt-6.1-sol read-only review found no production blocker;
its negative-test Windows-race concern was fixed, final review approved. Fresh native Windows CI
remains necessary. Existing 670-case game regression stays valid: no C++/game/test-body bytes changed
from 22fb24f85. No repeated game build/test and no old-head retry. Existing failed-workflow reporting
continues to reach the lane; no new reporting service or public endpoint.

Coalesced t_muxx6cjm999ox documentation scope release is fulfilled; t_muxxcqu2ac9ie release gate
follows the repaired current master and sole 71fae965 continuation. No tag/release/Signal notice until
both complete workflows and release assets pass. Private primary-party checkpoint tracks current
head, exact CI and retained schedule id/time; earlier 22fb24f85 checks cannot establish repair readiness.

## 2026-10-07 — F7 joined-party seating prerequisite

Quota gate permitted work (weekly 2%, session 21%); open issues and PRs were empty at selection.
The primary checkout was clean apart from two pre-existing untracked M3c4 logs. Sol retained
its independent keyboard source claims. Documentation scopes were requested through the report
API, released by Sol after its pushed checkpoint, then claimed by primary before editing.

The existing local lobby now carries title-page party membership into seats in join order without
another A press. First joined controller owns the host slot even if a former member still controls
Home. Campaigns and loaded saves share the host's tribe; ordinary maps seat individual tribes.
Insufficient or revoked selected seats prompt an explicit shared-mode choice, which seats the pending
party. Unused closed cards do not block smaller parties. No partial assignment or reopened Locked/
Occupied tribe; Classic menus, empty-party mouse play and network lobby policy retain their behavior.
A guest's B departure survives modal reactivation. This is a tested bridge to F7, not its new page.

Read-only exactly gpt-6.1-sol review found overflow recovery, stale-seat authority and unused-card
restriction issues; all corrected with physical regressions and final approval. Artifact:
build/party-review.txt. Own GCC12 Debug build passed, new party cases run in the actual binary;
complete MenuPadSeatTests: 36 cases / 582 assertions. A real played save is loaded and started with
shared views. Full Test_splitscreen passed: 670 cases / 501617 assertions (build/party-full.log).
Executed omission of first-activation seating fails the intended local-player assertion in the
actual title-to-lobby case (build/party-negative.log); production source restored byte-for-byte
(SHA256 43d1d5c3acd42209715d6d6dca87c4a94fc24babede24ec315a3f1c23a515e66), rebuilt,
and all 36 seat cases / 582 assertions pass again (build/party-restored-positive.log). Pinned clang-format10, include guards, markdownlint0.26, diff checks and the actual agent
TypeScript compiler passed. Targeted pinned clang-tidy18 reported no user-code diagnostics on the preceding validation snapshot; fresh
exact-head master Unit tests and Static analysis remain the release gate. Existing runtime/CI fault
reporting retained, no new service/secret endpoint. No Deck/hardware or packaged-build acceptance.

Next: finish the full-screen F7 party page on the common page framework; do not mark F7 complete
because the old lobby now seats a party. The previous primary release continuation 9d71cfbb was consumed during this run as request
t_muxxcqu2ac9ie; its gate now follows this new master. One replacement continuation/checkpoint owns
fresh CI, tag and release-assets verification. Coordination request t_muxx6cjm999ox is fulfilled;
no duplicate release schedule or progress message to Jan.

## 2026-10-07 — PR45 primary integration review

Incoming Sol handoff t_muxuo55j21w5g: PR45 tested head
635bd8c34745e9fd907c11a1435cf507392d73b3. Independently verified all 17 checks green,
including complete [Unit tests](https://github.com/jarvis-jhk/s25coop/actions/runs/37581159882)
and [Static analysis](https://github.com/jarvis-jhk/s25coop/actions/runs/37581159883).
Merged into the primary checkout on top of 985769dc6 without conflicts; addon source unchanged.
Exactly gpt-6.1-sol read-only review of the product/tests against current controls and focus
found no blocker in staged/preview values, whitelist authority or physical navigation.
Review artifact: build/pr45-review.txt. Own primary GCC12 Debug rebuild (ccache, max two
compiler jobs) passed. Complete Simple: 73 cases / 80794 assertions; UI: 146 / 20517;
splitscreen: 658 / 497360. Logs: build/pr45-{simple,ui,splitscreen}.log. New category suite
is present (2 / 533), and the new browser cases run in the complete splitscreen suite.
Static validation, diff check and the actual agent TypeScript compiler all passed.
Product/test trees match the exact-head-tested PR byte-for-byte. Integration is committed
and pushed after these gates; fresh master CI is required before tagging the next release.
A primary release continuation will verify that CI and publish only after its full success.
3a/3b are implemented; F7 party page and 3c preset/victory policy remain separate work.
Player-facing changes recorded under Unreleased. No Deck rendering or packaged-release proof.
Existing runtime/CI fault reporting retained; no new reporting endpoint or background service.

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


## 2026-10-07 — Sol: player-facing addon category browser (3a/3b)

From current front-end master d33ffcc3f after PR44 integration, Sol adds a pure
exhaustive category table for registered sparse AddonIds and advisory Easier/Harder
marks for unambiguous resource/travel constraints relative to actual defaults.
The existing addon window uses Comfort, New content, World & economy, Combat and
Easier tabs. Developer addons require an explicit browsing opt-in; their existing
values are retained. Changed only filters accepted staged values against addon
defaults. Empty views, category/filter/preset/default changes and scrollbar bounds
are refreshed together. See doc/coop/AddonCategories.md for the classification.

LB/RB cycles categories through an opt-in Window/FocusPath root hook; other windows
retain shoulder focus traversal. D-pad and mouse still reach all editing/actions.
Open dropdown previews survive shoulders and are not accepted by another controller
pressing Apply/Save. Defaults/filter/preset replacement resolves a pending dropdown
before replacing values, including the case where the new value equals its preview.
Apply retains the existing single lobby settings callback and network protocol.
Readonly/whitelist rules and custom Abort/B closing remain unchanged.

Read-only exactly gpt-6.1-sol reviews identified missing persistence coverage and a
default-replacement/dropdown mismatch; dedicated two-controller tests address both.
Pure tests require every registered/declared addon to have one category, exercise
every option/default and assert explicit difficulty orders, including SEA_ATTACK's
unranked disabled default2. Physical tests cover category cycling, developer opt-in,
mouse/readonly browsing, accepted/pending edits, default/filter/preset changes and
real preset files. Existing addon lobby tests retain the real-server roundtrip and
whitelist authority assertions, adapting navigation to category shoulders.

Own merged GCC12 Debug/Werror build at maximum2 jobs and full affected Debug
suites pass. Fresh exact-head complete Unit tests/Static analysis remain required
before tested handoff; current CI progress/evidence are kept outside the repo.
This supplies front-end party browsing metadata, not the F7 party rewrite, new
presets/save/victory policy, keyboard, hardware appearance or a packaged release.
The existing ntfy/runtime and GitHub failed-CI reporting remains in use; no private
report endpoint is published.

Earlier targeted checkpoint: the configured binaries contain both new suites.
Own Debug/Werror addon model2 cases/533 assertions and physical addon21 cases/580
assertions pass (new13/164, existing real-lobby8/416). Clang23 targeted checks on
all four new/changed implementation and test sources report newer style checks and
existing-header diagnostics; the actionable const-method/single-case-switch findings
are corrected. Pinned Clang18 exact-head CI remains the lint gate. The initial mouse-wheel assertion was corrected to exercise the parent
scroll guard outside the dropdown; mouse wheel on a combo remains an immediate
accepted mouse edit. No product mouse behavior was changed. The final restored
full Debug and executed omission results are below; exact-head CI is still pending.


Final local gate after merging current front-end master72832cba0 (Opus F2 title/join):
full Debug Test_simple73/80337, Test_UI146/19447 and Test_splitscreen656/499218
cases/assertions all pass. The new suites were configured into the binaries.
The inherited original S2 movie case is not exercised without RTTR_COOP_S2_DIR;
this slice does not change it. No hardware or release acceptance is claimed.

Three executed omission cases each fail intended assertions (exit201): missing
changed-only filtering displays the wrong rows (two visibility failures, followed
by an expected unreachable-control navigation exception); using an unaccepted
preview for Apply persists2 instead of1; omitting Default's preview cancellation
restores stale1 instead of the required2. Original iwAddons.cpp was restored
byte-for-byte and rebuilt before all final positive suites. No coverage exclusions.
Clang-format10, static validation, doc lint, diff/private-string checks and actual
agent tsc pass. Keep the PR draft until all17 exact-head check runs AND both
complete Unit tests/Static analysis workflows pass; then ready/tested handoff to
Opus, release finished claims and run the budget gate. No new slice before handoff.


## 2026-10-07 — Sol PR45: inherit the existing front-end CI repair

Exact head6561523d failed the gcc-10 coverage check solely at inherited
testFrontEndHome.cpp:177 (99.22%; an unexecuted shoulder loop). The new addon
test and shared AddonPadNavigation helper both report100% line coverage.
Current master0142389c9 already replaces that loop with the existing focus
helper and fixes front-end Clang18 diagnostics. Merge that committed repair
without editing Opus-owned front-end files; both workers' notes are preserved.
Own Debug/Werror max2 rebuild and affected physical suites are the new local
gate; all17 fresh exact-head checks and both complete workflows still required
before ready/tested handoff. No independent addon behavior change or new slice.

Repaired merge local gate passed: fresh affected Debug FrontEndPage/Home/Title,
PartyModel and both addon physical suites, plus complete UI146cases/14664
assertions. GCC12 Debug/Werror max2 rebuild, inherited format10, repository
static validation and actual agent tsc pass. All10 Sol source/test hashes match
the previously fully tested source. Fresh repaired-head CI supersedes old CI;
PR45 stays draft with its source/test claims until the complete exact-head gate.

## 2026-10-07 — Sol: reusable owner-scoped controller keyboard (4b)

`sol/controller-onscreen-keyboard`, based on master985769dc6: new menu modal with D-pad grid/A typing,
B Unicode delete, X space, Y letter case and Menu/Enter/Confirm to commit. Mouse keys and physical typing
share a private ctrlEdit draft with the original length, filename/number restrictions and password masking.
Legacy fields remain non-focusable by pad; F7/F9 can opt in with SetControllerKeyboardEnabled. Pad focus
never sets an original keyboard focus bit. Confirm writes once and emits the configured change notification,
without forwarding Enter to start a connection/send chat. No F7/F9 adoption or Deck/rendering acceptance yet.

Device identity owns the modal, even after slot reassignment and a restrictive desktop slot policy.
Default IngameWindow hooks still defer to the desktop. Owner disconnect, target/root replacement, closing
ancestors, disable and external edit revisions cancel a draft. A weak edit-owned revision token also
detects changed-and-restored text. Closing before notification and touching neither object afterwards
allows a parent to replace itself safely. ClearFocus also clears the cached desktop route: the physical
queued-input regression found that clearing only the focused controls still let a trailing Start activate
the underlying screen. Existing desktop-switch callers already destroy that cached root.

Four matched executed omission controls reject broken device ownership, stale revision acceptance,
closing-parent commit and cached desktop routing. Original product restored byte-for-byte and rebuilt.
All14 keyboard physical tests pass (190 assertions). Full own Debug UI passes146 cases/13131 assertions;
full splitscreen passes657 cases/501100 assertions before reconciling integrated PR45 master70d1a5dab.
Reconciliation preserves both docs and every keyboard/incoming addon source hash; fresh affected Debug
validation and complete exact-head CI remain the handoff gate. Exactly gpt-6.1-sol reviewed read-only; its double-delivery concern
was rejected because handled Msg_KeyDown skips child relay; concrete lifecycle/routing findings were fixed
and exercised. The explicit reusable-API-only scope is intentional. No coverage exclusions. Existing
runtime/failed-CI reporting retained; no service, persisted draft or new reporting endpoint.

## 2026-10-09 — Sol: grouped full-screen options (F9)

`sol/frontend-options` starts at fetched master57262bbe8, with the bounded Home
Options include/OnChoose wiring claimed and primary coordination t_mv0hjl4w33gvz.
The previous keyboard PR46 stays a completed all17/BOTH-workflow handoff, not new work.
New overview and display/audio/controls/language pages use dskFrontEndPage's factory
trail and accepted-input persistence. Mouse/keyboard keep the same controls; Advanced
settings and Classic menus preserve legacy-only routes. No OSK caller adoption or
help-footer toggle in this slice. Existing runtime/CI fault reporting retained; no
new service, token or public reporting URL.

Final own Debug/Werror max2 build passes: new suite12 cases/513 assertions, affected
front-end/title/party/legacy option/dropdown suites41/1006 and full UI146/22161.
New suite registration was confirmed in the configured binary. Test driver
adds only a default-false resize refusal to exercise the failed-display path; no
shared live driver edit. Read-only supplied-context review on exactly gpt-6.1-sol
identified audio-runtime cleanup and persistence-proof gaps: restore audio volumes/
playlist/playback explicitly and parse the saved config archive rather than creating
a second Settings singleton. Synchronous combo callbacks are safe here because lists
close before parent notification and there is no child/object deletion after it;
WindowManager updates windowedSize before desktop resize notification. Final executed
physical/legacy suites and fresh complete exact-head CI remain the handoff gate.

Three executed omission controls each fail the intended matched case (exit201):
missing Save leaves reloaded audio127/131 instead of128/130; omitted automatic-profile
reset keeps125/100 instead of0; omitted display rollback retains the rejected mode.
Original source was restored byte-for-byte, rebuilt and all positive suites rerun.
No coverage exclusions. Dropdown lists are constrained above the footer, including
all six display rows at800×600/1280×800. A resize lowers an oversized fixed scale
and persists it; valid custom125% stays exact. Fixtures restore saved video settings
after driver resize callbacks, as those callbacks also update windowedSize.
Source formatting10, static validation, private-string/diff checks and actual agent
typecheck pass. Supplied-context Sol review findings were addressed; complete fresh
exact-head CI (all17 and both Unit tests/Static analysis) is still required before
ready/tested handoff. No new slice before that handoff, no hardware/release claim.

## 2026-10-09 — Sol F6a full-screen save browser (local validation complete, CI pending)

Fresh master57262bbe8, branch `sol/frontend-save-browser`; no dependency on the
already handed-off PR46 keyboard / PR47 options. Claimed catalog, load page,
Home routing, catalog/physical Home/save suites and docs before editing.
`SaveCatalog` reads HeaderAndSettings only, sorts full timestamps with path ties,
counts corrupt/incompatible files without blocking good saves. Full-screen
`dskFrontEndLoad` shows scrollable saves and map/date/game-time/human+AI/name
details. Refresh keeps identity; Load and Continue stay on the browser on failure,
use the real HostGame/connecting path on success, and preserve one B back to Home.
No save deletion or file writes. Existing crash/failed-CI reporting remains;
no public report URL/service added. Saved thumbnails are not in the current
format; fallback is explicit, snapshot rendering remains F6b alongside online
solo-save co-player regression and optional AI takeover. Full-screen F7 is Opus's.
Physical tests include metadata failures and current solo-world rehost with
upstream4.0 player headers, two/four title-party controllers and unchanged
AI/locked tribes. Current-world data is not an upstream-produced old-save fixture;
that provenance-backed compatibility gate remains F6b.

Final Debug: physical11/895, catalog4/42, affected69/2509, full UI150/11010; all pass.
Own GCC12 Debug/Werror cache, at most2 compiler jobs. Initial full Debug baseline
681cases/501257assertions passed before equivalent range-wrapper/public-override
refinements and own test cast/mouse/locked-tribe strengthening; preserved binary
hash in the private checkpoint. Final changed/affected/UI suites pass after that
rebuild. No expensive repeat of unchanged legacy test behavior; fresh complete
exact-head CI is still mandatory. THREE executed production omission controls
fail intended timestamp-order / refresh-identity / deleted-Continue assertions
(exit201 each); product restored byte-identically and rebuilt before positives.
No coverage exclusions. Read-only exactly gpt-6.1-sol review fixed Nowide1.73
stream paths and Debug snapshot side effects, and corrected old-world provenance
claims. Exceptional cleanup reaches its named probe while Connecting is open;
port restored and windows cleared before fixture destruction. Driver restore
precedes saved video settings, asserted after normal and exceptional cleanup.
Dedicated new suite leaves the pre-existing Debug raw artifact SHA256 unchanged.
Clang-format10, static/private-marker/diff and actual agent typecheck gates pass;
local Clang23 targeted checks for the pinned-CI-compatible set pass, with no
repository check exclusions added. All17 checks and BOTH complete Unit tests /
Static analysis workflows must pass before tested primary handoff. Draft PR,
source/test claims retained during CI; Opus reviews/integrates. No hardware or
release claim. No Signal progress/report under releases-only policy.


## 2026-10-09 — Sol PR48 Windows occupied-port fixture repair

Exact head43aca25b2 failed the Win32 Debug Test_splitscreen step at the two new
save-resume lobby waits; compilation and DLL serialization checks passed.
The logs show both clients connected to the intentionally occupied ports.
Socket::Create enables SO_REUSEADDR, so Winsock could bind the server over
the fixture listener and route the handshake to that silent listener.
Fresh origin/master57262bbe8 contains no repair; merge-tree is clean.
The fixture now reserves both stacks with an IPv6 dual-stack socket, disables
reuse and requests Windows exclusive use before binding. It explicitly requires
the first UI action to show the bind-error message, then closes the reservation
and resumes through the real UI on a fresh port. A later transient connection
timeout retries the complete host/connect route; the final lobby check remains
fatal. Production source is unchanged. Native Windows exact-head CI remains
mandatory; no unchanged-head infrastructure retry or tested handoff is claimed.
Microsoft Winsock reference: https://learn.microsoft.com/windows/win32/winsock/using-so-reuseaddr-and-so-exclusiveaddruse

Repair local gate: GCC12 Debug/Werror rebuild at max2 jobs, physical11/901 and
affected69/2484 pass. Dedicated physical repeat preserves the Debug raw-artifact
hash; the broader affected suite can write it through unchanged legacy tests,
so that suite is not used as an artifact-purity claim. Catalog/UI product source
and Test_UI binary match their prior passing evidence. Supplied-diff read-only
review on exactly gpt-6.1-sol found no concrete defect; its repository reads were
blocked by the known namespace sandbox error. Format10, static validation,
diff/private-marker checks and actual agent tsc pass. Require all17 checks and
both complete workflows on the repaired SHA before ready/tested handoff.

## 2026-10-09 — Sol F5a independent map catalog (local validation complete)

Fresh master57262bbe8; ready PR46/47/48 remain primary integration work.
Full-screen F5 Home changes overlap those routes, so this bounded independent
slice claims MapCatalog, classic dskSelectMap metadata reader, catalog/legacy
physical tests and docs only. No Home/lobby/router or primary checkout edits.
Header metadata preserves UTF-8 names/authors, landscape, dimensions, tribes,
script annotation and stable filename identity. Scanner combines explicit map
folders, deduplicates lexical paths, sorts name/path and filters exact tribes;
malformed/unsupported files and folder failures do not hide good maps.
Zero/oversized dimensions and unsupported tribe counts now fail at listing rather
than deferring that error to a preview. Adjacent Lua is never executed for listing;
it is not evidence of campaign registration/unlocks. F5b/F5c remain open.
Classic mouse/pad preview still loads actual layers and retains its error flow.
Existing runtime ntfy and failed-CI issue reporting remain; no extra service or
private report URL is added. Debug physical/catalog validation, omission checks,
read-only gpt-6.1-sol review and complete exact-head CI precede tested handoff.


Final local Debug/Werror gate at max2 jobs: catalog6/104, physical8/101, affected37/1029, ui152/21671, splitscreen673/501494 cases/assertions all pass.
New catalog suite configured into Test_UI and confirmed present. Three executed
omission controls each exit201 on intended assertions: bypassing exact-tribe
filter, throwing on optional Lua status, and clearing instead of refreshing the
physical preview. Both product files restored byte-for-byte and rebuilt before
final full positive suites; no test-coverage exclusions. Read-only exactly
gpt-6.1-sol review corrected optional Lua isolation and stale-preview test strength;
final supplied-diff review found no concrete defect. Classic discovery consumes
the robust catalog directly, checking candidate suffixes before status so a
looping unrelated Lua symlink cannot hide the category. POSIX looping-symlink
cases are omitted on Windows; native exact-head CI remains the portability gate.
Clang-format10, static/private-marker/diff and actual agent tsc pass. Local newer
Clang23 new-code checks pass with the CI-compatible check set; existing legacy
header/destructor diagnostics and newer style checks remain CI18's gate. No new
repository check exclusions. An empty-file fixture stream is named and its open
state asserted. Original S2 movie rendering remains the existing optional
resource-dependent fixture, not exercised without RTTR_COOP_S2_DIR.
All17 checks AND both complete Unit tests/Static analysis workflows must pass
at the pushed source head before ready/tested primary handoff. Current head/CI
and sole continuation are kept in the private Sol checkpoint. No Home/party,
full-screen F5b, campaign/scenario registration or unlocks, hardware/release
acceptance claim. Opus owns review/integration; releases-only, no Signal progress.

### Sol F5a PR49 portability/coverage repair (2026-10-09)

The first CI head dba46ce0 failed macOS compilation: Boost1.92 no longer
transitively declares directory_iterator through operations.hpp. MapCatalog now
includes directory.hpp explicitly. All four Windows jobs reached the same one
assertion mismatch (native backslash vs a slash in the expected path); compare
filesystem paths assembled from components instead of representation strings.
GCC10 passed all48 CTest entries but rejected one unentered shoulder-loop line.
The physical preview test now asserts the table focus, moves right and back left
through driver input, then selects the next row; no exclusions were added.

Repaired own GCC12 Debug/Werror max2 build passed. Catalog6/104, physical8/103,
affected37/1031 and full UI152/24017 cases/assertions passed. Earlier full
splitscreen673/501494 and three intended omission controls remain historical
evidence; fresh complete repaired-head CI is mandatory before tested handoff.
Behavior/source beyond the explicit include and two test repairs is unchanged.
Fresh fetched master57262bbe8 merges cleanly without a reconciliation commit.
The draft PR and private checkpoint carry the repaired head and sole continuation.
No Home/fullscreen/campaign registration, hardware or release claim.
Read-only supplied-repair-diff review on exactly gpt-6.1-sol found no concrete
issue; repository inspection hit the known namespace restriction. Format10,
diff checks and the actual agent TypeScript check passed before commit.

## 2026-10-09 — Sol F6 ordinary solo save resumes online

Independent test slice on `sol/solo-save-online-resume`, starting at release master c45ae37c7;
Home/options/save/map PR46–49 integration and the primary fork sweep are untouched. The
`coop-net` harness can create a genuine `ServerType::Local` game with one human and no members.
Public-map tests use a played save with Default/Hard and Dummy/Easy AI tribes and a locked
fourth tribe, then load it online with a real host/co-player pair. The co-player's command
must add a woodcutter to the saved human tribe; another network save and same-group reconnect
must add the next. The saved GF, every goods/job inventory bucket, HQ positions and all
building/site counts compare across both save boundaries. Saved AI types/levels and the locked
tribe remain unchanged; initial/final host and member session checksums match.

Deliberate member desync after the solo load and after the network resave must execute its
scheduled RNG injection, be removed with the explicit out-of-sync reason, and leave the host
playing. The member's loaded economy, roles and initial world are checked before accepting
either deliberate failure. This prevents an earlier load defect from masquerading as the
intended negative. Current-code solo provenance is not a historical pre-coop upstream world;
thumbnails, optional AI takeover and GUI/F7/Deck acceptance remain open.

Local evidence is in the private Sol checkpoint: own GCC12 Debug/Werror max2 build, preliminary
complete 22 network CTests and final six affected/new CTests. The preliminary suite preceded
only the final injection log/guard; final six include all three new modes and the existing
normal/end/member desync cases. An executed real-member command omission reaches GF2000 with
Client@0 and one woodcutter, then fails the intended second-woodcutter host assertion. Positive
source remains unchanged by that control. Exactly gpt-6.1-sol supplied-patch read-only reviews
prompted the save-boundary snapshots, initial desync-member checks and injection-executed guard;
sandbox failure prevented repository inspection. Formatter10, repository static validation,
diff checks and the actual agent TypeScript compiler pass. Local Clang23 reports only inherited
or newer-than-CI18 diagnostics, so fresh complete CI18 remains authoritative.

No game/save/network format or product route changed. Existing CI/runtime fault reporting is
retained, with no private report URL in public source. Source/test claims are retained while
all17 exact-head checks and both complete Unit tests/Static analysis workflows run. Sol does
not merge its own PR or push master; primary owns review/integration. No tested handoff,
release or hardware proof is claimed at this documentation checkpoint.

The NOTES addition conflicted with the newly integrated master; merging master38b3ef439
preserved both sets of notes and all three Sol source/test hashes. Own Debug/Werror max2
rebuild after CMake reconfigure passed, followed by all six affected/new network CTests
and the complete merged Test_splitscreen suite. Source, docs and both binary hashes are
checkpointed privately. Fresh exact-head CI on the resulting merge remains mandatory;
the prior inherited Win32 bind failure and unaccepted403 rerun are historical.


## Sol F5b full-screen maps browser (2026-10-10)

Home Maps & scenarios now opens the shared full-screen frame with all nine map
categories, exact map-tribe filters, stable selected-file Refresh, lazy selected
minimap, details and adjacent-script marker. Controllers joined at Title do not
filter tribes. A, Enter, double-click and Continue revalidate the selected map
and use the existing local host/connecting/lobby flow. Broken headers and layers
remain isolated; repaired files recover. Classic keeps its original picker.
The lobby handles individual local tribes and an explicit shared-tribe choice
when more controllers join than map tribes. Full-screen F7 remains primary-owned,
and F5c scenario registration/unlocks, random maps and hardware acceptance are open.

The physical suite exercises keyboard/mouse/pad filter commit/cancel, identity,
80-row browsing, layer errors/repair/replacement, footer-bounded open dropdowns,
real resize and small-render entry/recovery, normal/exceptional settings cleanup,
and real two/four-controller starts through every start input. Start tests force
an occupied-port failure then retry through Home, select the original map even
when Played already holds a copy, and await the actual AI/Dummy seat broadcasts.
Only the original-resource loading desktop is bypassed; real game view ids,
router slots and twelve synchronized gameframes with no errors/async are checked.

A supplied-source read-only gpt-6.1-sol review found a shared combo defect: mouse
acceptance of the already highlighted pad/keyboard preview only closed the list
and did not notify its parent. An executed unchanged-control negative failed four
intended catalog/path assertions across both mouse buttons. The still-open mouse
paths now use Activate, preserving single notification when changing rows already
closed the list. The positive regression covers both left and right acceptance.
Further reviews prompted small-area guards, exact double-click target checks and
cleanup assertions; source review is not runtime proof.

Local Sol GCC12 Debug/Werror max2 build passes. All18 new physical cases/915 assertions,
complete Test_splitscreen728/502583 and Test_UI159/17125 pass on the saved source
and binary hashes. The tiny-entry case separately passes221 assertions, including
an explicit guard that the content is too small. Formatter10 equality, repository
static validation, include guards, markdown, diff and the actual agent TypeScript
compiler pass. Local Clang23 checks report only inherited/newer-than-CI18 rules;
fresh exact-head all17 checks and both complete Unit tests/Static analysis
workflows remain the handoff gate.
Existing CI/runtime fault reporting is retained; no private report endpoint is
published in the source. No release or Steam Deck hardware result is claimed.

## 2026-10-11 — PR52 integrated (F4 full-screen campaigns)

Primary review of Sol PR52 at b561056e4: Home → Campaigns opens dskFrontEndCampaigns (six artwork
tiles per page, own archive per tile so equal filename stems keep their own picture) and
dskFrontEndMissions (table with Completed/Available/Locked, minimap, recheck of uid/map/script on
start, existing local host + SetHostingCampaign path). CampaignCatalog skips broken campaigns and
missing scripts into `failures`. All17 exact-head CI green; merged locally, Debug Test_splitscreen
and Test_UI pass in build/dbg (build/pr52-*.log). Review nits, not blocking: mission page re-reads the
map file on every layout (UpdateDetails in OnLayout); the catalog parses every map header on each scan.
CHANGELOG Unreleased now has maps + campaigns pages; release v0.1.15 once master CI is green.
PR53 (Sol F10 loading screen) still in progress on Sol's side; Coveralls 92% patch coverage, nothing for me.

## 2026-10-11 — PR51 integrated (full-screen maps browser)
Primary review of Sol PR51 at 8b883d93c: Home → Maps & scenarios now opens dskFrontEndMaps
(nine Classic categories, map-tribe filter, lazy preview, revalidation on Continue, existing
local host flow). ctrlComboBox mouse accept on an unchanged highlighted row now goes through
Activate, which only notifies the parent when the value changed since opening — no extra
notifications in other combo users. All17 exact-head CI checks green. Merged locally into
2376833e4 and re-ran in my own Debug cache: full Test_splitscreen and Test_UI pass. Minor nit,
not blocking: the WORLDS directory is scanned for "Own maps" too, while the comment says "All".
Next: F7 full-screen party (primary), F5c scenarios/campaign registration, random maps; no release yet.


## Sol F10 full-screen loading and failure recovery (2026-10-11)

Home-started games now retain the common full-screen frame while loading: exact
client map title, joined-controller strip, actual stage and server wait, an
economy tip and an in-game controller legend. The frame has an explicit Classic
opt-out, preserving original artwork, scaling and accumulating messages without
skipping an overridden parent virtual method. All other pages retain the default
frame. Content wraps inside the shared layout, with small-render entry and real
resize/queued-activation recovery. No new confirmation is added before play.

The existing timed loader creates actual game views/postboxes before GameLoaded,
then uses the real server start callback. Two/four-controller tests observe the
mission script's initial goal, shared view/router ids and twelve synchronized
frames. Resource/view/connection faults stop loading and show one controller or
mouse acknowledgement. Inactive and reentrant faults preserve the failure text
and cannot restart the timer or enter GameLoaded. Loading retains queued pad
input; error A/B bursts do not act on the next page. Classic start/error paths
and a deliberate real-start exceptional-cleanup/restart are exercised.

Own GCC12 Debug/Werror max2 build passes. Physical11/372, complete
Test_splitscreen739/503161 and Test_UI159/15349 pass after the final framework change.
Omitting the reentrant status guard fails the two intended Loading-failed
assertions; the guard is restored before final positive validation. A read-only
exact gpt-6.1-sol review found the reentrant defect before repair and no final
blocker. Original S2 graphics loading/rendering is the sole fixture bypass;
these tests do not establish packaged rendering or Steam Deck hardware behavior.

Formatter10 equality, repository static validation, include guards, Markdown,
diff and actual agent TypeScript compiler pass. Local Clang23's loader-destructor
warning also reproduces on independently supplied unchanged master source; the
other recorded newer/inherited diagnostics do not justify suppressions. Fresh
all17 exact-head checks and BOTH complete Unit tests/Static analysis workflows
remain mandatory before ready/tested handoff. No lint or coverage exclusions
are added. Existing installer/runtime-exit and CI fault reporting is retained;
no private endpoint enters public source. F4 integration, F5c scenario registration,
F7 party and release/hardware acceptance remain separate, primary-owned work.

## Sol F4 full-screen campaigns and missions (2026-10-11)

Home Campaigns now opens a paged artwork grid and a full-screen mission list.
The catalog validates campaign descriptions, map headers and required mission
scripts independently; missing optional images use a text fallback. Each card
owns its full-path image archive so campaigns with identical image stems cannot
reuse another campaign's preview. Existing continent-background artwork is used
when declared; masked continent interaction remains a separate M1 task.

Mission selection shows the saved Available/Locked/Completed state, descriptions
and a lazy minimap. Refresh preserves file identity. Start revalidates the
campaign uid, mission order, map/script identity, current progress and full map
layers before using the existing local host/connecting/lobby path. Title parties
share the campaign's human tribe. Back preserves the chosen campaign and grid
page, including keyboard activation. Classic keeps its chooser. Primary still
owns F7 party routing/integration; F8 online campaigns, F5c scenarios, original S2
presentation and release/Steam Deck acceptance are separate.

Own GCC12 Debug/Werror max2 build and both complete test binaries pass:
Test_splitscreen740/503638 and Test_UI165/16137. The new campaign physical suite
passes12 cases/697 assertions and the catalog suite6/65. Eight real starts cover
two/four Title controllers through pad A, Continue, Enter and double-click,
including an occupied-port failure/retry, the exact second mission's map/Lua,
its executed onStart mission goal, actual shared game views and twelve gameframes
without client errors or async. Only the original-resource loading desktop is
bypassed. Layout, missing/reordered/repaired files, locks/progress revocation,
optional/equal-stem artwork and normal/exceptional cleanup are exercised.

An executed omission of keyboard trail dispatch fails the intended restored-focus
assertion; omitting required-script validation admits three campaigns instead of
one and fails the executed catalog assertion. Both guards were restored before
the final build and full positive tests. A read-only gpt-6.1-sol source review
found the keyboard trail defect before repair; a final review found no blocker.
Source review and mocked input do not establish hardware behavior.

Formatter10 equality, repository static validation, include guards, Markdown,
diff checks and the actual agent TypeScript compiler pass. Local Clang23 reports
no new-file warning enabled in the CI Clang18 check catalog; newer diagnostics
are recorded privately without lint/coverage suppressions. Existing CI/runtime
fault reporting is retained, with no private endpoint in public source. This is
source/local validation completion only: all17 exact-head checks and both complete
Unit tests/Static analysis workflows are mandatory before the tested handoff.

F4 CI repair (2026-10-11): all15 Unit-test matrix jobs executed their tests
successfully at ba171923fa, but gcc-10's test-line coverage gate found three
unexecuted lines in StartFixture's exceptional cleanup. The page fixture's
exception probes did not exercise that separate start fixture. A new regression
starts the real two-controller Mission2 game, reaches a deliberate throw only
after the game-state assertion, then checks stopped client, restored progress
and port before fixture destruction. A second real start checks that server/lobby
state was released. No coverage exclusion or product change is needed. Fresh
Debug validation and all17/BOTH exact-head CI remain the repaired-head gate.
The repaired Debug build passes; standalone campaign13/795 and full
Test_splitscreen741/502376 pass (the new exception case94 assertions in each).
All nine other campaign source/test files and the Test_UI binary keep their
saved hashes. Formatter10 equality, static validation, diff and the actual
agent TypeScript compiler pass. The old failed coverage head is historical;
its successful jobs cannot substitute for fresh complete CI on this repair.


## Sol PR53 master reconciliation (2026-10-11)

PR53 reconciliation with integrated PR52: both append-only NOTES entries retained; all five F10 source/test hashes unchanged. Fresh own GCC12 Debug/Werror max2 configure/build passes, loading 11/372, complete splitscreen 752/501982 and UI 165/11827 pass. Static validation, Markdown, diff and actual agent TypeScript compiler pass. Prior all17/BOTH green head is historical; require fresh all17/BOTH on this merge head before tested handoff. F4 is integrated, F7 remains primary-owned; F5c scenario policy, release and hardware acceptance remain separate.


## Sol F5c — explicit scenario registration (2026-10-11, source complete; CI pending)

Branch `sol/frontend-scenarios` from integrated master `ab0d2dd1d`, claimed independently of primary F7.
A scenario is an existing campaign.lua registration with exactly one mission, not a guess from a
neighboring Lua file. Maps & scenarios adds a Scenarios category; filter counts map tribes, not
controllers. Catalog isolation/order, current campaign progress and the F4 mission start path are reused.
The browser retains category/filter/campaign folder through Back and rechecks identity before opening
its mission page. The mission script owns goals/victory/completion; no synthetic UID or automatic unlock.

Local Debug validation passes: scenario catalog4/40, physical9/782, full splitscreen761/501877,
full UI169/11474, built in Sol's cache with at most2 compiler jobs. The physical suite starts two/four
joined controllers through A, mouse, Return and double-click, checks actual mission goal and first GF,
busy-port retry and both exceptional cleanup paths. The executed negative control removed the browser
identity guard: its changed-registration case failed at the intended assertion with exit201; guard restored
and unchanged on the final source. Read-only gpt-6.1-sol review found a repeated-header-read isolation gap;
CampaignEntry now owns validated metadata and scenario consumers reuse it. Final review: no remaining
findings. Sort headers were already disabled; physical clicks verify stable registered row identity.
Actual agent TypeScript gate, static validation, format10, header/private-marker checks and doc lint pass.
Local LLVM23 tidy has inherited/newer-check noise; final catalog source reruns have no owned diagnostics.
Exact-head all17 checks and BOTH complete CI workflows plus fresh merge-tree are required for tested handoff.
Only original S2 resource loading is bypassed in the real-start fixture. No tested handoff yet.
Packaged art/Steam Deck, F7 full-screen party and F8 online flow remain distinct acceptance/work.
Scenario authors: doc/coop/Scenarios.md. Existing fault reporting stays in the central ntfy→GitHub path;
no private reporting URL or personal data is added to public source.


## Verified v0.1.16 release (2026-10-11)

v0.1.16 is published and ready to install: full-screen loading, controller
status and fault recovery. Release workflow [38146348851](https://github.com/jarvis-jhk/s25coop/actions/runs/38146348851)
completed successfully. The existing annotated tag/release commit
4c4d5853d91255580c850cc84b2452621087aa23 was retained without retagging.
Source master ab0d2dd1d passed complete [Unit tests](https://github.com/jarvis-jhk/s25coop/actions/runs/38140880709)
and [Static analysis](https://github.com/jarvis-jhk/s25coop/actions/runs/38140880708);
primary Debug loading11/372, splitscreen752/504662 and UI165/18217 passed.

All three public assets were downloaded successfully with HTTP200 and exact
nonempty byte counts: Install-s25coop.desktop625, Linux x86_64 tarball44782174,
Windows x64 ZIP34515559. [Release/downloads](https://github.com/jarvis-jhk/s25coop/releases/tag/v0.1.16).
These checks establish published downloads, not Steam Deck hardware acceptance.
F7b remains primary next; Sol's F5c scenario work and claims remain separate.


## Sol PR54 release-note reconciliation (2026-10-11)

Merged master 39db483b5 after the v0.1.16 verification note conflicted with the F5c note. Both notes and the release changelog are retained. All nine scenario source/test hashes and the other three scenario documentation hashes remain unchanged. Own Debug max2 build completed; catalog4/40, physical9/782, full splitscreen761/505007, UI169/10490 pass. Actual agent TypeScript compiler and diff checks pass. Prior ace99d725 CI is historical; fresh all17 checks and BOTH successful workflows on this reconciliation head remain mandatory before tested handoff.


## Sol PR54 Clang-Tidy repair (2026-10-11)

Exact-head dbea0909f passed the complete Unit tests workflow, but CI18 Static analysis failed
on performance-unnecessary-value-param in dskFrontEndMaps::Refresh. Use a const reference
and snapshot the constructor's preferred map before DeleteAllItems selection callbacks can
reset shared browser state. Existing physical Back checks preserve the non-first Zulu scenario.
Own Debug max2 rebuild completed; catalog4/40, physical9/782, full splitscreen761/503190
and UI169/28547 pass. Targeted performance Clang-Tidy, format10 and actual agent TypeScript
pass. Other seven scenario source/test files are unchanged. Prior CI evidence is historical;
fresh all17 checks and BOTH successful workflows on the repaired head remain required before
ready/tested handoff. No lint suppression or extra test exclusions were added.


## PR56 integrated: fog-aware save previews (2026-10-12)

Opus reviewed Sol's PR56 (exact head 7651666b8, all17 checks, Unit tests 38213435232 and
Static analysis 38213435425 green) and merged it into master as 133b5e694 (tree identical to
the tested head). Review: trailer is appended only after the world save succeeded, a failed
or partial preview never fails the save, Save() truncates so overwrites drop old trailers,
the reader skips the world bytes and checks size/signature/dimensions/checksum, and the
throwaway GameWorldViewer unsubscribes on destruction. No findings. Primary Debug max2:
Test_UI174 and Test_splitscreen763 pass. Not yet released; player-visible CHANGELOG entry
belongs with the next release. F7b party work on feature/f7b-party must merge this master
(docs conflict only) before its own handoff.

## PR57 integrated: F8a online entry and direct join (2026-10-12)

Opus reviewed Sol's PR57 (exact head 324f2b2c6, all17 checks, Unit tests 38246551858 and
Static analysis 38246551785 green) and merged it into master; libs/tests/doc identical to the
tested head. Review: Connect mirrors iwDirectIPConnect (lastIP/ipv6 persisted, port and password
not, coop leader COOP_LEADER_HOST, scoped onError connection resets the button and shows the
error); Create keeps the proxy refusal. Codex gpt-6.1-sol diff review: no actionable findings.
Primary Debug max2: Test_splitscreen and Test_UI pass. Not yet released; the player-visible
CHANGELOG entry belongs with the next release. Follow-up from Sol: inherited AI_ADDRCONFIG
resolver rejects numeric ::1 on IPv6-only-loopback hosts (not addressed).

## PR58 integrated: F8b LAN browser (2026-10-13)

Opus reviewed Sol's PR58 (exact head 009eb7601, all17 checks, Unit tests 38356620014 and Static
analysis 38356620033 green) and merged it into master. Codex gpt-6.1-sol diff review found two
points: (1) active advertisements entered the bounded service map before payload validation, so a
malformed packet could displace a valid entry from the same address or take one of the 128 slots —
fixed on master: `LanDiscovery::Run` drops anything `ReadLanServer` rejects; new assertion in
LanEndpointReplacement… (negative control without the fix fails its two intended checks).
(2) Discovery entries are keyed by IP only, so two hosts on one address/different ports overwrite
each other — inherited from the classic LANDiscoveryClient protocol (withdrawals carry no stable
endpoint key); not addressed, deferred. Primary Debug max2: Test_splitscreen and Test_UI pass.
Last siedler-sol PR: that lane is retired; future Sol slices come as roadmap ticks in this lane.
Not yet released; CHANGELOG entry with the next release.

## REA on the original S2.EXE (2026-10-12)

Request from Jan (via adhoc): use REA for original Settlers II behaviour. Ghidra has no LE
(DOS/4GW) loader, so `tools/re/le2elf.py` converts S2.EXE into an i386 ELF (15 objects, 72689
fixups, none skipped); REA 6.4.0 / Ghidra 12.1.4 then analyses a function in about two minutes.
How-to and findings: doc/research/rea.md. Sol may use it the same way (own copy, never commit
the exe, the ELF or decompiled code).

First finding (M1 "mission unlocking as close to the original"): Roman progress is
SAVE\MISSION.DAT, one '1'/'0' per mission, shipped as 1100000000 (= RTTR default {0,1}). The final
event of each mission opens up to three missions named in `!MET_SET_FINAL_EVENT` of its RTX
script; for Roman that is strictly n -> n+1, MISS209 opens nothing. Our MISS205.lua additionally
opens chapter 7 (upstream TODO "remove when chapter 7 [MISS206] is playable"). Next: prove MISS206
winnable in RTTR, then drop that extra unlock. World campaign (MISSION2.DAT, continent map) is
separate code, still open.


## Sol F8b — full-screen LAN discovery and join (2026-10-13)

`sol/frontend-online-lan` replaces the default Online → LAN route with a
full-screen server table, Refresh/Join/Create and the page trail. Classic keeps
its old browser. Endpoint identity includes address/port/family: rename and
refresh preserve it, replacement or withdrawal clears active selection, and
activation drains announcements before resolving the row. Revision mismatch
blocks Join. F8a's masked, staged input form now carries ServerType::LAN through
its real password/co-player handshake. Host creation retains its proxy guard.

The first read-only gpt-6.1-sol review found the inherited discovery drain could
monopolize the UI under sustained UDP traffic. The new browser uses its own
protocol-compatible adapter: 16 packets/call, 128 retained addresses, deferring
Join while announcements remain. A spare receive byte rejects oversized
POSIX datagrams; short packets, magic/version/active bytes, unbounded string
lengths, invalid booleans/counts/ports are rejected. The shared dependency and
Classic are unchanged. The second read-only review found no actionable issue;
its Socket/oversized-packet verification caveat was resolved by inspecting the
shared socket RAII/recvfrom path and adding actual short/oversized UDP cases.

Own GCC12 Debug, max2 jobs: isolated LAN/direct physical + parser/discovery
19 cases / 853 assertions pass; final relevant Home/Page/network/lobby/create/
table regressions 56 cases / 3681 assertions pass. Real LAN host/member sockets
exercise a wrong password, retry and host-tribe membership; real UDP discovery,
physical pad/mouse/keyboard, resize/small-content recovery, a withdrawal queued
in the A-input frame and exceptional cleanup/re-entry are covered. Executed
endpoint-retarget and doubled-receive-budget omission controls each fail two
intended assertions; both sources were restored byte-exact, rebuilt and the
positive regressions rerun. Local format10/static validation/diff and actual
agent TypeScript pass. Clang23 records only newer style checks on the new page
and existing dependency diagnostics; exact-head Clang18 CI remains required,
with no source suppressions added.

Status: implementation/local proof complete, fresh all17 checks and BOTH full
CI workflows required before tested handoff to Opus. Design/API details in
[OnlineLan](doc/coop/OnlineLan.md); private evidence under data/siedler/sol-lan-*.
ROADMAP remains primary-owned. Combined "Online menus" stays started: Internet
server/login, F4–F6 network hosting, connecting presentation and F7d cards remain.
This is mock-renderer and loopback proof, not physical Deck/remote-LAN acceptance.
The existing central public fault relay remains; malformed announcements and
ordinary connection/user errors do not create reports. No private endpoint,
release, primary integration or completed-PR claim.


## Sol PR58 CI repair (2026-10-13)

Head 87301553b7 is historical, not a tested handoff: both Clang sanitizer jobs
failed on the new malformed all-FF advertisement probe reaching the dependency's
Serializer::PopVarSize signed shift (127 << 28). LAN strings now decode bounded
uint32_t varints locally, rejecting fifth-byte overflow before shifting; the
shared dependency stays untouched. Tests also retain valid multi-byte revision
lengths and reject oversized multi-byte name lengths.

macOS reported six send failures because the UDP fixture used PeerAddr's
storage-union size. The two senders now use a zero-initialized sockaddr_in and
its actual size with sendto; native macOS CI is still the portability gate. GCC10
passed all tests, then rejected one unexecuted table-search fallback. Every
fixture advertisement has the same host name, so the redundant search is removed
and the real selected-name/handshake assertions retained, without exclusions.

Own Debug max2 final rebuild passes: LAN/direct19 cases/857 assertions; affected
Home/Page/network/lobby/create/table56 cases/3685 assertions. A separate test
binary instruments LanServers.cpp and Serializer.cpp with GCC12 UBSan and
-fno-sanitize-recover=all, leaving normal objects unchanged:19/857 pass. Restoring
the old LAN decoder in that separate binary fails the executed malformed-length
test with the exact signed-shift diagnostic (exit1). Normal source was never
replaced. Format10, diff checks and actual agent TypeScript pass. Fresh exact-head
all17 checks and BOTH successful workflows are required; no handoff, release,
hardware claim or next slice. Private evidence: sol-lan-ci-repair-* and original
failed job logs. The combined Online menus item remains started.

- 2026-10-13: master b172ddb failed only on macOS (Test_splitscreen, testLanServers.cpp:153): the inactive
  LAN withdrawal was read right after sendto, but macOS loopback delivery is asynchronous. The test now
  polls Run() for up to 5 s until the entry is gone; product code unchanged. Linux Debug LanServerTests pass.
- 2026-10-13: that poll loop (77f886f) then failed the gcc-10 test-coverage gate: on Linux the first Run()
  already sees the withdrawal, so the sleep/retry lines never executed. Rewritten as a `for` loop with the
  sleep in its header, so every line runs on the fast path too (no LCOV exclusion). Debug Test_splitscreen
  passes locally (LanServerTests 70/70).
