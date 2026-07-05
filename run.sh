#!/usr/bin/env bash
#
# run.sh - builds DOOM and launches it
#
# arguments get passed to the game
#   ./run.sh -fullscreen (start with fullscreen)
#   ./run.sh -2            (start at 2x window size)
#   ./run.sh -geom +100+50 (start at 100x50 on your screen)
#
# run setup.sh first

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/linuxdoom-1.10"
WAD_DIR="$SCRIPT_DIR/wad"

if ! ls "$WAD_DIR"/*.wad >/dev/null 2>&1; then
    echo "No IWAD found in $WAD_DIR." >&2
    echo "Run ./setup.sh first to install dependencies and fetch the" >&2
    echo "shareware doom1.wad (or drop your own doom.wad/doomu.wad in" >&2
    echo "that directory)." >&2
    exit 1
fi

echo "==> Building DOOM"
make -C "$SRC_DIR"

BIN="$SRC_DIR/linux/linuxxdoom"

if [ ! -x "$BIN" ]; then
    echo "Build failed: $BIN was not produced." >&2
    exit 1
fi

echo "==> Launching DOOM"
echo "    (Alt+Enter toggles fullscreen; drag an edge to resize the window)"
exec env DOOMWADDIR="$WAD_DIR" "$BIN" "$@"
