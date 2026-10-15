<!--
Copyright (C) 2026 s25coop contributors

SPDX-License-Identifier: GPL-2.0-or-later
-->

# Internet lobby and network hosting

The new Play online → Internet Lobby page uses the existing lobby client and protocol.
It supports account login, optional saved password, IPv4/IPv6, server refresh/selection,
public chat, disconnect and Create Game. Registration instructions point to the upstream
forum. Classic retains its original dialogs.

Both password fields and their controller keyboards mask input. The saved-password
hash is recognized independently of the Save Password setting, so disabling persistence
cannot double-hash a retry. Successful login clears the password edit. The page trail
contains no credentials. Login failures and lost connections remain on a usable page;
ordinary authentication/network errors are not anonymous fault reports.

Server identity includes lobby id, host and port. Refresh preserves that identity rather
than its row number; removals, endpoint replacement and an empty list clear selection.
Activation checks the current backend list and version before opening the existing
masked join form. Lobby joins pass the same lobby adapter as Classic, preserving join
notifications and public chat in the existing game lobby. LobbyBot recipient filtering
is retained. The table, chat and login fields resize inside the full-screen frame, with
Back available when the window is too small.

Create Game from Direct, LAN or Internet first chooses Campaigns, Maps & scenarios or
Load game. These reuse the normal catalogs, previews and page trail, carrying only the
network type. After selecting content, the final full-screen form asks for game name,
port, password and address family. A password never enters a reconstruction factory or
settings. Back discards the form, returning to the selected content.

Start checks the proxy policy, lobby connection, name and port. Campaign registration,
mission identity and current chapter availability are revalidated; campaign hosts allow
co-players of the host's tribe. Saves are revalidated at this final step too. HostGame
remains authoritative for reading the map/save and binding the server. Failed reads,
exceptions or failed handshakes keep a retryable form; partially started hosts are
stopped. Existing installer/runtime-exit and CI fault reporting are retained, without
adding a private endpoint to the public tree.

This is the F8c Internet/hosting part of Online menus. The connecting window and network
party/lobby still use their existing presentation; the full-screen connection stages
and F7d remote cards/readiness/chat remain next. Local-party seating in an online host
is not added. Mock-driver input and loopback sockets do not establish physical Deck,
public Internet service or router/NAT acceptance.
