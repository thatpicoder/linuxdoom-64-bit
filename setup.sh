#!/usr/bin/env bash
# run this once and then after that run run.sh to build and play.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WAD_DIR="$SCRIPT_DIR/wad"
WAD_PATH="$WAD_DIR/doom1.wad"
WAD_ZIP_URL="https://archive.org/download/doom_20230531/doom_dos.ZIP"
WAD_MD5="f0cefca49926d00903cf57551d901abe"

echo "==> Installing build dependencies"

install_deps() {
    if command -v dnf >/dev/null 2>&1; then
        sudo dnf install -y gcc make libX11-devel libXext-devel alsa-lib-devel curl unzip
    elif command -v apt-get >/dev/null 2>&1; then
        sudo apt-get update
        sudo apt-get install -y build-essential libx11-dev libxext-dev libasound2-dev curl unzip
    elif command -v pacman >/dev/null 2>&1; then
        sudo pacman -Sy --needed --noconfirm base-devel libx11 libxext alsa-lib curl unzip
    elif command -v zypper >/dev/null 2>&1; then
        sudo zypper install -y gcc make libX11-devel libXext-devel alsa-devel curl unzip
    else
        echo "Could not detect a supported package manager (dnf/apt/pacman/zypper)." >&2
        echo "Please install manually: a C compiler, make, libX11 + libXext +" >&2
        echo "ALSA (libasound) development headers, curl, and unzip." >&2
        exit 1
    fi
}

install_deps

echo "==> Fetching the shareware IWAD (doom1.wad)"

mkdir -p "$WAD_DIR"

wad_is_valid() {
    [ -f "$WAD_PATH" ] || return 1
    if command -v md5sum >/dev/null 2>&1; then
        [ "$(md5sum "$WAD_PATH" | cut -d' ' -f1)" = "$WAD_MD5" ]
    else
        true
    fi
}

if wad_is_valid; then
    echo "doom1.wad already present and verified, skipping download."
else
    TMPDIR="$(mktemp -d)"
    trap 'rm -rf "$TMPDIR"' EXIT

    echo "Downloading the shareware episode from archive.org..."
    curl -sL -o "$TMPDIR/doom_dos.zip" "$WAD_ZIP_URL"
    unzip -j -o "$TMPDIR/doom_dos.zip" DOOM1.WAD -d "$TMPDIR"
    mv "$TMPDIR/DOOM1.WAD" "$WAD_PATH"

    if command -v md5sum >/dev/null 2>&1; then
        ACTUAL_MD5="$(md5sum "$WAD_PATH" | cut -d' ' -f1)"
        if [ "$ACTUAL_MD5" != "$WAD_MD5" ]; then
            echo "WARNING: doom1.wad checksum mismatch" >&2
            echo "  expected: $WAD_MD5" >&2
            echo "  got:      $ACTUAL_MD5" >&2
            echo "The file may be corrupted, or archive.org served a different" >&2
            echo "version. The game may still run, but proceed with caution." >&2
        else
            echo "Checksum verified: $ACTUAL_MD5"
        fi
    fi
fi

echo
echo "==> Setup complete."
echo "If you own the full game, drop doom.wad (or doomu.wad) into"
echo "  $WAD_DIR/"
echo "and it will be used automatically instead of the shareware episode."
echo
echo "Run ./run.sh to build and start DOOM."
