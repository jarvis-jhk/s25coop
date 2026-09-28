#!/bin/bash
# s25coop installer and updater, for the Steam Deck (Desktop Mode) and any desktop Linux.
#
#   install.sh           interactive install or reinstall (what "Install s25coop.desktop" runs)
#   install.sh run       what the Steam/menu shortcut runs: update if a newer release exists,
#                        then start the game. Never blocks the start on a failed update.
#
# Layout under ~/.local/share/s25coop: game/ (the release, replaced on update), S2/ (the
# original DATA + GFX, never touched by an update), install.sh (this script, self-updated).
# Pattern after github.com/ArnoldSmith86/minecraft-splitscreen (MIT).
set -uo pipefail

REPO=jarvis-jhk/s25coop
BASE="${XDG_DATA_HOME:-$HOME/.local/share}/s25coop"
GAME="$BASE/game"
S2="$BASE/S2"
SELF="$BASE/install.sh"
STEAM_NAME="The Settlers II: Coop"

# Faults are reported anonymously to a public ntfy.sh topic; a GitHub Action in this repo
# (.github/workflows/fault-reports.yml) turns them into issues. Opt out: S25COOP_NO_REPORTS=1
# or an empty file $BASE/no-reports. Keep this function identical in s25coop.sh.
REPORT_TOPIC=s25coop-reports-ko3knuxwigscapljz76yjk5z
report() { # report <title> <detail>: fire-and-forget, at most a few seconds, never fails the caller
    { [ -n "${S25COOP_NO_REPORTS:-}" ] || [ -e "$BASE/no-reports" ]; } && return 0
    local os ver t="${1//$HOME/\~}"
    t=$(printf "%s" "${t//$'\n'/ }" | sed "s#[0-9]\{1,3\}\(\.[0-9]\{1,3\}\)\{3\}#x.x.x.x#g")
    os=$(. /etc/os-release 2>/dev/null && echo "${PRETTY_NAME:-${NAME:-linux}}")
    ver=$(cat "$GAME/VERSION" 2>/dev/null || echo none)
    printf 'source: s25coop %s, %s, %s\n\n%s\n' "$ver" "${REPORT_PART:-installer}" "${os:-linux}" "$(printf "%s" "$2" | tail -c 3500)" |
        sed "s#$HOME#~#g; s#[0-9]\{1,3\}\(\.[0-9]\{1,3\}\)\{3\}#x.x.x.x#g" |
        curl -fsS --max-time 8 -o /dev/null -H "Title: ${t:0:120}" -H "Tags: s25coop" \
             --data-binary @- "https://ntfy.sh/$REPORT_TOPIC" 2>/dev/null
    return 0
}

say()  { echo -e "$*"; }
fail() { say "❌ $*"; report "installer: ${1%%\\n*}" "$*"; dialog error "$*"; exit 1; }
fail_user() { say "❌ $*"; dialog error "$*"; exit 1; } # the user's own mistake, not a fault
dialog() { # dialog error|info|question <text>
    local kind=$1 text=$2
    if command -v zenity >/dev/null; then
        zenity --"$kind" --no-wrap --text="$text" 2>/dev/null
    elif command -v kdialog >/dev/null; then
        case $kind in error) kdialog --error "$text";; info) kdialog --msgbox "$text";; question) kdialog --yesno "$text";; esac
    else
        [ "$kind" = question ] && { read -rp "$text [y/N] " a; [[ "$a" == [yYjJ]* ]]; }
    fi
}

latest_release() { # prints "<tag> <tarball url>"
    curl -fsL --max-time 15 "https://api.github.com/repos/$REPO/releases/latest" |
        tr ',' '\n' | sed -n 's/.*"tag_name": *"\(.*\)".*/\1/p; s/.*"browser_download_url": *"\(.*linux-x86_64\.tar\.gz\)".*/\1/p' |
        paste -sd' '
}

installed_version() { cat "$GAME/VERSION" 2>/dev/null || echo none; }

download_game() { # <url>
    local tmp="$BASE/download"
    rm -rf "$tmp" && mkdir -p "$tmp" || return 1
    curl -fL --progress-bar "$1" -o "$tmp/game.tar.gz" && tar -xzf "$tmp/game.tar.gz" -C "$tmp" || return 1
    [ -x "$tmp/s25coop/s25coop.sh" ] || return 1
    rm -rf "$GAME.old" && { [ ! -d "$GAME" ] || mv "$GAME" "$GAME.old"; } && mv "$tmp/s25coop" "$GAME" || return 1
    rm -rf "$tmp" "$GAME.old"
    # Keep the updater itself current, so fixes to it reach existing installs.
    # Replaced by rename, never rewritten in place: this very script may be the one running, and bash
    # reads a running script as it goes.
    [ -f "$GAME/install.sh" ] && cp "$GAME/install.sh" "$SELF.new" && chmod +x "$SELF.new" && mv -f "$SELF.new" "$SELF"
}

have_s2() { [ -d "$1/DATA" ] && [ -d "$1/GFX" ]; }

downloads_dir() { # the user's Downloads folder, localised name included (e.g. ~/Downloads, ~/Téléchargements)
    local d
    d=$(xdg-user-dir DOWNLOAD 2>/dev/null)
    [ -n "$d" ] && [ "$d" != "$HOME" ] && [ -d "$d" ] && { echo "$d"; return; }
    echo "$HOME/Downloads"
}

# Takes DATA, GFX and VIDEO out of a Settlers II download (the GOG installer or a copy of the CD) in the
# Downloads folder. Deliberately no search of the whole disk: it is slow and finds half-copies.
# Without a usable download it opens a help page saying what is needed, and offers to look again.
setup_s2() {
    have_s2 "$S2" && return 0
    local dl used rc folders
    dl=$(downloads_dir)
    folders=("$dl"); [ "$dl" != "$HOME/Downloads" ] && folders+=("$HOME/Downloads")
    while true; do
        say "🔍 Looking for Settlers II in $dl (this can take a minute when it is found)"
        used=$(python3 "$GAME/s2-extract.py" auto "$S2" "${folders[@]}" 2> >(tee "$BASE/s2-extract.log" >&2))
        rc=$?
        if [ $rc -eq 0 ] && have_s2 "$S2"; then
            say "✅ Settlers II files taken from ${used##*/}"
            return 0
        fi
        [ $rc -eq 1 ] && report "extracting Settlers II failed" "$(tail -n 30 "$BASE/s2-extract.log")"
        cp -f "$GAME/s2-help.html" "$BASE/settlers2-needed.html" 2>/dev/null
        xdg-open "$BASE/settlers2-needed.html" >/dev/null 2>&1 &
        dialog question "s25coop needs the files of the original Settlers II Gold Edition,\nand no usable download was found in\n$dl\n\nA help page has opened in your browser: it says which file is needed and where to get it\n($BASE/settlers2-needed.html).\n\nPut the file into $dl, then choose Yes to look again." ||
            fail_user "Settlers II is still missing.\nPut the GOG installer (setup_the_settlers_2_gold_….exe) into $dl and start s25coop again.\nHelp: $BASE/settlers2-needed.html"
    done
}

add_menu_entry() {
    local apps="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
    mkdir -p "$apps"
    cat > "$apps/s25coop.desktop" <<DESKTOP
[Desktop Entry]
Name=s25coop
Comment=Settlers II coop (Return to the Roots)
Exec=$SELF run
Icon=$GAME/s25coop.png
Terminal=false
Type=Application
Categories=Game;StrategyGame;
DESKTOP
    say "✅ Added to the application menu"
}

add_to_steam() {
    [ -d "$HOME/.steam/steam/userdata" ] || return 0
    local f old=0 missing=0
    for f in "$HOME"/.steam/steam/userdata/*/config/shortcuts.vdf; do
        if grep -qs "s25coop/install.sh" "$f"; then grep -qsF "$STEAM_NAME" "$f" || old=1; else missing=1; fi
    done
    if [ $missing -eq 0 ] && [ $old -eq 0 ] && ls "$HOME"/.steam/steam/userdata/*/config/shortcuts.vdf >/dev/null 2>&1; then
        say "✅ Already in Steam"; add_steam_artwork; return 0
    elif [ $old -eq 1 ]; then
        # Entries from before 0.1.4 are called "s25coop" and have no artwork: rename them once
        dialog question "Give the s25coop entry in Steam the Settlers II name and pictures?\n\nSteam will be closed for a moment and started again." || { add_steam_artwork; return 0; }
    else
        dialog question "Add s25coop to Steam, so it can be started from Game Mode?\n\nSteam will be closed for a moment and started again." || return 0
    fi
    say "⏳ Closing Steam to add the shortcut…"
    steam -shutdown >/dev/null 2>&1
    for _ in $(seq 60); do pgrep -x steam >/dev/null || break; sleep 1; done
    # Steam writes shortcuts.vdf when it exits: an edit made while it still runs would be lost
    if pgrep -x steam >/dev/null; then say "⚠️ Steam did not close, the Steam entry is left as it is"; return 0; fi
    for f in "$HOME"/.steam/steam/userdata/*/config/shortcuts.vdf; do
        [ -f "$f" ] && cp -n "$f" "$f.s25coop-backup"
    done
    if python3 "$GAME/add-to-steam.py" "$STEAM_NAME" "$SELF" "$BASE" "run" 2>"$BASE/add-to-steam.log"; then
        say "✅ Added to Steam"
    else
        report "adding to Steam failed" "$(tail -n 40 "$BASE/add-to-steam.log" 2>&1; ls -la "$HOME"/.steam/steam/userdata/*/config/shortcuts.vdf 2>&1)"; say "⚠️ Adding to Steam failed (backup of the old shortcuts: shortcuts.vdf.s25coop-backup)"
    fi
    nohup steam >/dev/null 2>&1 &
}

# Settlers II box art for the Steam entry (library cover, banner, hero, logo, icon), fetched from
# SteamGridDB. Also run at every start so existing entries get it; files already there are kept.
add_steam_artwork() {
    [ -d "$HOME/.steam/steam/userdata" ] && [ -f "$GAME/add-to-steam.py" ] || return 0
    grep -qs "s25coop/install.sh" "$HOME"/.steam/steam/userdata/*/config/shortcuts.vdf || return 0
    python3 "$GAME/add-to-steam.py" --artwork-only "$STEAM_NAME" "$SELF" >/dev/null 2>>"$BASE/add-to-steam.log" &
}

run_game() {
    local rel tag url
    rel=$(latest_release)
    tag=${rel%% *}; url=${rel#* }
    if [ -n "$tag" ] && [ -n "$url" ] && [ "$url" != "$tag" ] && [ "${tag#v}" != "$(installed_version)" ]; then
        say "⏳ Updating s25coop $(installed_version) → ${tag#v}"
        download_game "$url" || { say "⚠️ Update failed, starting the installed version"; report "update failed" "$(installed_version) -> ${tag#v} from $url"; }
    fi
    [ -x "$GAME/s25coop.sh" ] || fail "s25coop is not installed. Run the installer again."
    setup_s2
    add_steam_artwork
    S25COOP_S2_DIR="$S2" exec "$GAME/s25coop.sh" "$@"
}

install() {
    say "🏰 s25coop installer\n"
    dialog question "This installs s25coop (Settlers II coop, based on Return to the Roots) into\n$BASE\nand adds it to your menu and to Steam.\n\nYou need the original Settlers II Gold files.\n\nInstall?" || exit 0
    mkdir -p "$BASE" || fail "Cannot create $BASE"
    local rel tag url
    rel=$(latest_release)
    tag=${rel%% *}; url=${rel#* }
    [ -n "$tag" ] && [ "$url" != "$tag" ] || fail "Could not find a release on github.com/$REPO."
    say "⏳ Downloading s25coop ${tag#v}"
    download_game "$url" || fail "Downloading s25coop failed."
    { cp "$GAME/install.sh" "$SELF.new" 2>/dev/null || cp "$0" "$SELF.new"; } && chmod +x "$SELF.new" && mv -f "$SELF.new" "$SELF"
    say "✅ s25coop ${tag#v} installed in $GAME"
    add_menu_entry
    add_to_steam
    setup_s2
    say "\n🎉 Done. Start s25coop from the menu, or from Steam in Game Mode."
    dialog info "s25coop is installed.\n\nStart it from the menu, or in Game Mode from your Steam library (Non-Steam)."
}

case "${1:-}" in
    run) shift; run_game "$@";;
    *) install;;
esac
# END OF FILE (the .desktop launcher checks for this line to detect a truncated download)
