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
A successful direct handshake uses the existing connecting window and online
lobby; Abort stops that attempt and leaves the form usable. Ordinary invalid
ports, wrong passwords and unavailable hosts are displayed locally, without
fault reports. The existing central crash/report path remains unchanged.

This bounded F8 slice replaces the default online entry and direct-join form.
The LAN browser, Internet login, host creation and online lobby still use their
existing implementations, including proxy restrictions. LAN Back goes through
the classic multiplayer selector before returning Home. Full-screen LAN/server
browsing, network host selection through F4–F6, connecting presentation and F7d
online cards remain subsequent slices. Classic menus retain their old routes.

Required gate: physical pad/mouse/keyboard and resize tests, actual IPv4/IPv6
password handshakes, co-player join to a host tribe, cancellation and retry,
exceptional cleanup, own Debug max two compiler jobs and all exact-head CI.
IPv6 validation uses the existing localhost loopback path: the inherited numeric-address
resolver filters by configured interfaces and rejects ::1 in a container with only
IPv6 loopback. That separate resolver limitation is recorded for follow-up; numeric
IPv6 reachability in this environment is not claimed. Desktop simulation does not
establish Deck/network hardware acceptance.
