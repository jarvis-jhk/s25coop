<!--
Copyright (C) 2026 s25coop contributors

SPDX-License-Identifier: GPL-2.0-or-later
-->

# Coop: several clients control one player (design, M2)

Goal: any number of people play the ONE human tribe of a campaign mission together, over LAN or the
internet. Every one of them sees the same economy and may build, attack, set priorities; the game
cannot tell who gave an order. Nothing upstream or in any fork does this (see ROADMAP.md M2).

## How RttR runs a network game today (the parts that matter)

- Lockstep. Every *network frame* (NWF, every few game frames) each world player sends ONE
  `GameMessage_GameCommand` (its commands for NWF n + cmdDelay and the checksum of its world).
  The server keeps them in `NWFInfo`, relays each one to everybody with `SendToAll`, and only lets
  the game advance (`GameMessage_Server_NWFDone`) when it has a set from every player in NWFInfo.
- Every client, the one that sent them included, executes commands **only from the server's
  relay**: `GameClient::OnGameMessage(GameMessage_GameCommand)` → `nwfInfo->addPlayerCmds` →
  `ExecuteNWF` → `gc->Execute(world, playerId)`. A client never executes its own commands locally.
- The network player id *is* the lobby slot index *is* the world player id:
  `networkPlayers[i].playerId`, `playerInfos[id]`, `GameClient::GetPlayerId()` and `GamePlayer`
  index are the same number everywhere. The host may send commands for its AIs (`GetTargetPlayer`).
- Replays record, per NWF and per player id, the commands the server relayed.

## Chosen design: member connections, commands merged by the server

A **member** is a network connection that belongs to an existing world player (its **leader**
slot) instead of being a world player itself.

- **1 Server.** A member's `GameMessage_GameCommand` is not added to `NWFInfo`. The server keeps
   its commands in a per-leader buffer and appends that buffer to the leader's next command set
   before it stores and relays it (details below). From then on everything is the stock path.
- **2 Everybody else** — clients, the leader, the replay, the async check — sees one player with a
   few more commands than usual. No world code changes, the world has no extra players, and
   replays and savegames keep their format.
- **3 The member's client** runs the world like any client, with `GetPlayerId()` = the leader
   (what it sees, what its UI orders act for); its connection has no player id of its own. Its
   NWFInfo holds only world players; it sends its own commands every NWF like everyone, but nobody
   waits for them.

Why not "member slots are full NWF participants, remapped at execution": that would need an id
mapping at every `Execute`, in the replay format (two command sets for one player per NWF) and in
every loop over players. The merge keeps all of that untouched. Its price:

- A member does not hold up the game. If it lags, its orders simply land one or more NWFs later
  (fine: it is a person clicking), and the server must drop a member that falls too far behind
  (timeout like the ping timeout) instead of waiting for it.
- Members are not part of the async check for free (step 5, below).

### Members are a connection role, not a player id (Codex review, 2026-09-28)

A fabricated id range would run into `uint8_t` player ids with `0xFF` as a sentinel, and into every
server loop that treats `networkPlayers` as world players (`KickPlayer` returns early for ids
outside `playerInfos`, `CheckForLaggingPlayers`, `CheckAndKickLaggingPlayers`, `CheckForAsync`,
`ExecuteNWF`, ping, map checksum and password handling all index `playerInfos[senderPlayerID]`).
So members get their own list on the server (`GameServerMember`: socket, leader slot, name, ping,
state), their sockets are polled separately, and only a whitelist of messages is accepted from them
(join handshake, pong, chat, game commands, async log); anything else kicks the member.
`networkPlayers`, `playerInfos` and `NWFInfo` never contain a member.

### The merge, exactly

- The canonical command set of a world player for an NWF is sealed **once**, on the server, when
  the leader's `GameMessage_GameCommand` arrives: leader commands first, then every member command
  that arrived since the previous seal, in arrival order (per member in send order). This happens
  **before** `nwfInfo.addPlayerCmds` and the broadcast, so the server's own `NWFInfo` and every
  client hold the identical set. Conflicting orders resolve as they always do between two orders
  of one player: the later one in the set wins or is rejected by the world.
- Member commands carry no NWF number and need none: a member order is executed in whichever set
  is sealed next. Latency therefore varies with how the member's packet and the leader's next set
  interleave (typically ≤ one NWF plus the member's ping); it is a person clicking, that is fine.
- A member's commands are validated on execution like everybody's, but an order given on a
  desynced picture of the world is still a legal, wrong order. So a member whose checksum does not
  match (step 5; checksums must be tagged with the GF they were taken at, since members may run
  behind) gets no more orders accepted and is asked to reconnect.
- The replay records the canonical sets, so it replays unchanged — and it does not know which
  person gave which order. That is accepted (a debug log on the server can record member ids).
- Members connected when the game starts receive every relayed set from GF 0 over their socket;
  the server does not wait for them while loading. Joining a game already running needs a savegame
  transfer and is out of scope.

### Local players (M3, splitscreen)

Local players on one machine that share the campaign player are simpler still: all their input
goes into the one client's `gameCommands_`. Only players on *different* machines need the above.

## Status

Steps 0–2 are implemented (2026-09-28): `GameServerCoop.cpp` (members, whitelist handler, merge), the member mode in
`GameClient` (`Connect(..., coopMemberOf)`), the message `NMS_COOP_JOIN_MEMBER`, and ctest `CoopNet_Member*`.
How a member joins today: it connects like any client and, whatever id it is offered (a free slot or none), answers
with `GameMessage_Coop_JoinMember(leader)`; the server hands a reserved slot back and keeps the connection as a member
if the leader is an occupied human slot, then the stock handshake (type, password, map) follows. Found while testing:
nobody waits for a member, so it can fall many NWFs behind; its NWFInfo therefore keeps any number of command sets
(`setUnboundedCmds`) and it runs GFs without waiting while more than cmdDelay NWFs are pending (catch-up).

## Steps, each finishable and testable on its own

- **0 Network test harness.** Nothing can be merged here without running it, and a network game
   has never run headless: a test binary that hosts or joins a game over localhost without video,
   plays N game frames with scripted input (the ai-battle `test` API) and prints the final
   checksum; a ctest starts a host and a client process and requires equal checksums.
- **1 Server merge.** `GameServerMember` list, message whitelist, command buffer, seal on the
   leader's packet. Harness test: leader and member both give orders; both processes end with the
   same checksum and the member's building exists.
- **2 Client member mode.** `GameClient` joins as member of slot X: `GetPlayerId()` = X, NWFInfo
   only for world players, commands sent without a player id of its own.
- **3 Join flow and lobby.** How a member joins (the server offers "join player X" when the host
   allows members; default for campaigns: the human slot), lobby list, ready state, kicking a
   member. New protocol messages; protocol version bumped (fork clients never talk to upstream).
- **4 Campaign as network game.** `dskCampaignMissionSelection` hosts LAN/online instead of local
   (`ServerType`), mission script unchanged (player 0 stays the only human world player; the
   group is its members). Harness test: MISS200 walkthrough with its orders split over two clients.
- **5 Robustness.** Checksums tagged with their GF, member vs. leader compared on the server
   (mismatch → member's orders refused, asked to reconnect), member timeout, leader leaves → the
   first member takes over the slot and its host rights atomically.
- **6 Save and resume** (M2 item 3): the save stores nothing new; resuming is a fresh lobby where
   the host loads the save and the group joins the saved human slot again as members.
