#!/usr/bin/bash

# runEmulator.sh
# Usage: ./scripts/runEmulator.sh [[IsoPath]] [[Pcsx2Path]]

# 1. Determine ISO Path
ISO_IN=${1:-"./exec/engine.iso"}
ISO_PATH=$(realpath "$ISO_IN")

# 2. Detect Operating System
OS_TYPE=$(uname -s)
IS_WSL=false
if grep -qi microsoft /proc/version 2>/dev/null; then
    IS_WSL=true
fi

# 3. Determine PCSX2 Path (User override first)
PCSX2_PATH=${2:-""}

if [[ -z "$PCSX2_PATH" ]]; then
    if [[ "$OS_TYPE" == "Darwin" ]]; then
        # macOS
        PCSX2_PATH="/Applications/PCSX2.app/Contents/MacOS/PCSX2"
    elif [[ "$IS_WSL" == true ]]; then
        # WSL - Support common Windows locations
        PCSX2_PATH="/c/PCSX2/pcsx2-qt.exe"
        if [[ ! -f "$PCSX2_PATH" ]]; then PCSX2_PATH="/mnt/c/PCSX2/pcsx2-qt.exe"; fi
        if [[ ! -f "$PCSX2_PATH" ]]; then PCSX2_PATH="/c/Program Files/PCSX2/pcsx2-qt.exe"; fi
        if [[ ! -f "$PCSX2_PATH" ]]; then PCSX2_PATH="/mnt/c/Program Files/PCSX2/pcsx2-qt.exe"; fi
    else
        # Linux / Other
        PCSX2_PATH=$(command -v pcsx2-qt)
        if [[ -z "$PCSX2_PATH" ]]; then PCSX2_PATH=$(command -v pcsx2); fi
    fi
fi

if [[ -z "$PCSX2_PATH" ]] || [[ ! -f "$PCSX2_PATH" && ! -x $(command -v "$PCSX2_PATH") ]]; then
    echo "Error: PCSX2 not found."
    echo "Usage: ./scripts/runEmulator.sh [IsoPath] [Pcsx2Path]"
    exit 1
fi

# 4. Handle Path Conversions (WSL Specific)
ISO_FINAL="$ISO_PATH"
if [[ "$IS_WSL" == true ]]; then
    if command -v wslpath > /dev/null; then
        ISO_FINAL=$(wslpath -w "$ISO_PATH" 2>/dev/null || echo "$ISO_PATH")
    fi
fi

echo "=== Launching PCSX2 ($OS_TYPE) ==="
echo "PCSX2: $PCSX2_PATH"
echo "ISO:   $ISO_FINAL"

# 5. Run PCSX2
if [[ "$OS_TYPE" == "Darwin" ]]; then
    # On macOS, we might need 'open' or just run it directly
    "$PCSX2_PATH" -batch "$ISO_FINAL" &
else
    "$PCSX2_PATH" -portable -batch "$ISO_FINAL" &
fi