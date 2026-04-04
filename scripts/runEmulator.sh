#!/usr/bin/bash

# runEmulator.sh
# Usage: ./scripts/runEmulator.sh [[FilePath]] [[Pcsx2Path]]
# FilePath can be an ISO or an ELF — PCSX2 auto-detects the format.

FILE_IN=${1:-"./exec/engine.iso"}
FILE_PATH=$(realpath "$FILE_IN")

OS_TYPE=$(uname -s)
IS_WSL=false
if grep -qi microsoft /proc/version 2>/dev/null; then
    IS_WSL=true
fi

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
    echo "Usage: ./scripts/runEmulator.sh [FilePath] [Pcsx2Path]"
    exit 1
fi

FILE_FINAL="$FILE_PATH"
if [[ "$IS_WSL" == true ]]; then
    if command -v wslpath > /dev/null; then
        FILE_FINAL=$(wslpath -w "$FILE_PATH" 2>/dev/null || echo "$FILE_PATH")
    fi
fi

echo "=== Launching PCSX2 ($OS_TYPE) ==="
echo "PCSX2: $PCSX2_PATH"
echo "File:  $FILE_FINAL"

if [[ "$OS_TYPE" == "Darwin" ]]; then
    "$PCSX2_PATH" -batch "$FILE_FINAL" &
else
    "$PCSX2_PATH" -portable -batch "$FILE_FINAL" &
fi