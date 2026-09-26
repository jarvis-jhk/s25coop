#!/bin/bash
# s25coop launcher. Runs the bundled game from wherever this folder was unpacked.
# The original Settlers II Gold files (DATA + GFX folders) are looked for, in this order:
#   $S25COOP_S2_DIR, ~/.local/share/s25coop/S2, share/s25rttr/S2 next to this script.
# Keeping them outside the install folder means an update never touches them.
DIR=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
export LD_LIBRARY_PATH="$DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

for cand in "${S25COOP_S2_DIR:-}" "${XDG_DATA_HOME:-$HOME/.local/share}/s25coop/S2" "$DIR/share/s25rttr/S2"; do
    if [ -n "$cand" ] && [ -d "$cand/DATA" ] && [ -d "$cand/GFX" ]; then
        export RTTR_GAME_DIR="$cand"
        break
    fi
done
if [ -z "${RTTR_GAME_DIR:-}" ]; then
    msg="Settlers II Gold files not found. Copy the folders DATA and GFX of your Settlers II Gold installation into ${XDG_DATA_HOME:-$HOME/.local/share}/s25coop/S2/"
    echo "$msg" >&2
    if command -v kdialog >/dev/null; then kdialog --error "$msg"
    elif command -v zenity >/dev/null; then zenity --error --no-wrap --text="$msg"
    fi 2>/dev/null
    exit 1
fi
cd "$DIR" && exec "$DIR/bin/s25client" "$@"
