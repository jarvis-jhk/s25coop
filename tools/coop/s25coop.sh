#!/bin/bash
# s25coop launcher. Runs the bundled game from wherever this folder was unpacked.
# The original Settlers II Gold files (DATA + GFX folders) are looked for, in this order:
#   $S25COOP_S2_DIR, ~/.local/share/s25coop/S2, share/s25rttr/S2 next to this script.
# Keeping them outside the install folder means an update never touches them.
DIR=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
BASE="${XDG_DATA_HOME:-$HOME/.local/share}/s25coop"
GAME="$DIR"
REPORT_PART=game
export LD_LIBRARY_PATH="$DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# Faults are reported anonymously to a public ntfy.sh topic; a GitHub Action in this repo
# (.github/workflows/fault-reports.yml) turns them into issues. Opt out: S25COOP_NO_REPORTS=1
# or an empty file $BASE/no-reports. Keep this function identical in install.sh.
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

find_s2() {
    for cand in "${S25COOP_S2_DIR:-}" "$BASE/S2" "$DIR/share/s25rttr/S2"; do
        if [ -n "$cand" ] && [ -d "$cand/DATA" ] && [ -d "$cand/GFX" ]; then
            export RTTR_GAME_DIR="$cand"
            return 0
        fi
    done
    return 1
}
# Not set up yet (started from the tarball, without install.sh): take the files from a Settlers II
# download in the Downloads folder, like the installer does.
if ! find_s2 && [ -f "$DIR/s2-extract.py" ] && command -v python3 >/dev/null; then
    dl=$(xdg-user-dir DOWNLOAD 2>/dev/null); { [ -n "$dl" ] && [ "$dl" != "$HOME" ]; } || dl="$HOME/Downloads"
    echo "Looking for Settlers II in $dl" >&2
    python3 "$DIR/s2-extract.py" auto "$BASE/S2" "$dl" "$HOME/Downloads" >/dev/null && find_s2
fi
if [ -z "${RTTR_GAME_DIR:-}" ]; then
    msg="The files of the original Settlers II Gold Edition are missing.\nPut the GOG installer (setup_the_settlers_2_gold_….exe) or a copy of the CD into your Downloads folder and start again.\nHelp: $BASE/settlers2-needed.html"
    mkdir -p "$BASE" && cp -f "$DIR/s2-help.html" "$BASE/settlers2-needed.html" 2>/dev/null && xdg-open "$BASE/settlers2-needed.html" >/dev/null 2>&1 &
    echo -e "$msg" >&2
    if command -v kdialog >/dev/null; then kdialog --error "$(echo -e "$msg")"
    elif command -v zenity >/dev/null; then zenity --error --no-wrap --text="$msg"
    fi 2>/dev/null
    exit 1
fi

# Not exec'd: the exit status and the tail of the output are what a crash report is made of.
cd "$DIR" || exit 1
out=$(mktemp "${TMPDIR:-/tmp}/s25coop.XXXXXX") || exec "$DIR/bin/s25client" "$@"
"$DIR/bin/s25client" "$@" 2>&1 | tee "$out"
rc=${PIPESTATUS[0]}
# 130/143: closed with Ctrl+C or by Steam, not a fault.
if [ "$rc" -ne 0 ] && [ "$rc" -ne 130 ] && [ "$rc" -ne 143 ]; then
    logs="${RTTR_USERDATA_DIR:-$HOME/.s25rttr}/LOGS"
    log=$(ls -t "$logs"/*.log 2>/dev/null | head -n 1)
    first=$(grep -m1 -iE 'error|exception|crash|fail' "$out" | cut -c1-100)
    report "game exited with $rc${first:+: $first}" "$(printf 'exit: %s\n\n--- output (tail)\n%s\n\n--- %s (tail)\n%s' \
        "$rc" "$(tail -n 40 "$out")" "${log:+${log##*/}}${log:-no RttR log}" "$(tail -n 40 "$log" 2>/dev/null)")"
fi
rm -f "$out"
exit "$rc"
