<!--
Copyright (C) 2026 s25coop contributors

SPDX-License-Identifier: GPL-2.0-or-later
-->

# LAN browser (F8b)

Default Home → Play online → Network/LAN uses a full-screen server table,
Refresh, Join Game and Create Game. B, Escape and the Back button return to
Online with Network/LAN focused, then Home. Classic keeps dskLAN unchanged.

Discovery retains the existing v4 UDP protocol and minute refresh, with at most
16 received packets per call and 128 remembered addresses. Join defers while
announcements remain queued, so it cannot act ahead of a pending withdrawal. Updates
preserve endpoint identity (address, port, address family), not row number or
server name. Renaming a server preserves selection; withdrawing or replacing
its endpoint clears the active row and Join. A controller can explicitly choose
a new row. Advertisements are drained again at activation so a withdrawal in
the same input frame cannot open a stale endpoint. An incompatible revision
stays visible and cannot join. Malformed payload lengths, boolean bytes, ports
and player counts are rejected before entering UI state; this is ordinary
untrusted network input and creates no fault report.

Join opens F8a's address/port/password/co-player form populated from the selected
advertisement, retaining ServerType::LAN for the real handshake and lobby.
Passwords are masked, never captured by page factories or stored, and empty on
re-entry. Back rebuilds the browser and rediscovers the preferred identity.
Refresh clears stale discoveries while preserving that preferred identity until
it reappears or the player explicitly chooses another endpoint.

The frame resizes for 800×480, 800×600, Deck render units and 1280×800. Too-small
content hides interactive controls and gives the existing scale/window warning.
LAN creation retains the classic create dialog and proxy guard. Internet login,
F4–F6 network hosting, full-screen connecting and F7d network player cards are
separate remaining F8/F7 slices. F8a plus F8b does not finish the combined
"Online menus" item. The central public ntfy fault relay and CI webhook remain
the fault path; no private queue URL belongs in this public repository.

Acceptance uses actual UDP advertisements through LANDiscoveryClient, physical
pad/mouse/keyboard navigation, endpoint change/withdrawal/re-entry, malformed
payloads, real LAN password failure/retry and host-tribe co-player handshake,
exceptional cleanup and the legacy network regression suites. These tests use
the mock renderer; physical Deck and a remote LAN are separate acceptance.
