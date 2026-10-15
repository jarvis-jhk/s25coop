<!--
Copyright (C) 2026 s25coop contributors

SPDX-License-Identifier: GPL-2.0-or-later
-->

# Online entry and direct join (F8a)

Home → Play online opens a full-screen page with Join Game, Network/LAN,
Internet Lobby and Create Game. Join Game opens a full-screen address, port and
password form with IPv6 and "Play the host's tribe together" choices. Every edit
opts into the existing controller keyboard: A opens a staged draft and Menu
confirms it without submitting the connection. Mouse and ordinary keyboard input
use the same fields; Enter moves through address/port, then connects from the
password. Passwords stay masked and are never saved in settings or page factories.

B, Escape and the visible Back button rebuild Online and then Home with the
chosen tile focused. Failed connections retain the form for correction/retry.
The live handshake replaces the form with full-screen status/map-transfer progress;
Abort stops that attempt and restores the same form. Success opens the existing online
lobby. Ordinary invalid
ports, wrong passwords and unavailable hosts are displayed locally, without
fault reports. The existing central crash/report path remains unchanged.

The full-screen LAN browser, Internet login/server list/chat and content-first network
hosting are implemented by the follow-up pages. [Internet and hosting](OnlineInternet.md)
also describes their shared connection-progress control. F7d online player cards remain
a separate roadmap item. Classic menus retain their old routes.

Required gate: physical pad/mouse/keyboard and resize tests, actual IPv4/IPv6
password handshakes, co-player join to a host tribe, cancellation and retry,
exceptional cleanup, own Debug max two compiler jobs and all exact-head CI.
IPv6 validation uses the existing localhost loopback path: the inherited numeric-address
resolver filters by configured interfaces and rejects ::1 in a container with only
IPv6 loopback. That separate resolver limitation is recorded for follow-up; numeric
IPv6 reachability in this environment is not claimed. Desktop simulation does not
establish Deck/network hardware acceptance.
