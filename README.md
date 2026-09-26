<!--
Copyright (C) 2005 - 2025 Settlers Freaks <sf-team at siedler25.org>

SPDX-License-Identifier: GPL-2.0-or-later
-->

# s25coop — The Settlers II campaigns, together

**s25coop** is a fork of [Return To The Roots](https://github.com/Return-To-The-Roots/s25client)
(RttR), the open-source remake of *The Settlers II*. It has one goal:

> Play the Settlers II campaigns **cooperatively** — any number of players controlling
> **the same people** in a campaign mission, with progress saved and continued by the same group.

That is different from ordinary multiplayer: nobody plays "red against blue". Everyone steers
the one tribe the campaign is about, over the internet or on one screen in splitscreen.

## What this fork adds (planned and in progress)

- **Coop campaigns** — several players share control of the single campaign player.
- **Campaign progress** — finished missions are remembered, the next ones unlock the way the
  original did, conquered continents show on the world map. Progress belongs to the group.
- **Faithful campaigns** — the original Roman and World campaigns first, then fan campaigns
  (Die Rückkehr der Wikinger, Roman Campaign II, FANpaign, 2NDpaign, …).
- **Steam Deck first** — a one-click installer that adds the game to Game Mode, controller
  support throughout (menus included), a UI sized for the Deck, automatic updates, and a
  changelog shown in-game after each update. Mouse and keyboard keep working everywhere.
- **Linux and Windows builds** on every release.
- **The best of the community** — work from other RttR forks and branches (splitscreen with
  gamepads, campaign status, one-sided alliances, …) is merged here when it is safe to.

See [ROADMAP.md](ROADMAP.md) for the order things are being done in and what is finished.

## Where it comes from

Everything here is built on the work of the Return To The Roots team and its contributors;
this fork only adds to it. Upstream: <https://github.com/Return-To-The-Roots/s25client>,
website <https://www.rttr.info>. Their original README is kept as
[README.upstream.md](README.upstream.md), including build instructions.

Changes that are useful beyond this fork are offered back upstream as pull requests.

## Install

Downloads are on the [Releases page](https://github.com/jarvis-jhk/s25coop/releases/latest).

**Steam Deck** (and any desktop Linux): in Desktop Mode, download
[Install-s25coop.desktop](https://github.com/jarvis-jhk/s25coop/releases/latest/download/Install-s25coop.desktop)
and open it in the file manager (Dolphin). It downloads the game, finds your Settlers II files
(or asks for the folder), and adds s25coop to the menu and to Steam, so it starts from Game Mode.
Every start checks for a new release and updates itself.

**Linux by hand:** unpack `s25coop-<version>-linux-x86_64.tar.gz` anywhere, put `DATA` and `GFX`
into `~/.local/share/s25coop/S2/`, run `s25coop/s25coop.sh`.

**Windows:** unpack `s25coop-<version>-windows-x64.zip`, copy `DATA` and `GFX` into the unpacked
folder, run `s25client.exe`.

**Fault reports.** On Linux, when the installer, an update or the game fails, s25coop sends an
anonymous report (version, OS name, exit code, the last lines of output and of the game log, with
your home folder replaced by `~`) to a public [ntfy.sh](https://ntfy.sh) topic; a GitHub Action
turns it into an issue labelled
[`fault-report`](https://github.com/jarvis-jhk/s25coop/issues?q=label%3Afault-report), so bugs get
fixed without anyone having to write them up. To switch it off, set `S25COOP_NO_REPORTS=1` or
create an empty file `~/.local/share/s25coop/no-reports`.

## You still need the original game

Like RttR, s25coop uses the original graphics and sounds: copy the `DATA` and `GFX` folders from
*The Settlers II Gold Edition* (for example the GOG version) — see Install above for where they go.

## Who maintains this

This fork is maintained by **JARVIS**, an AI agent, on behalf of its human players. Issues and
pull requests are read and answered here — please open one if something is broken or missing.

## License

GPL-2.0-or-later, like upstream. See [LICENSE](LICENSE) and [LICENSES/](LICENSES).
