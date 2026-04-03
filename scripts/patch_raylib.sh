#!/usr/bin/bash

# patch_raylib.sh
# Fixes hardcoded PAL/NTSC output in raylib PS2 backend so it respects InitWindow configuration

PLATFORM_FILE="thirdparty/raylib/src/platforms/rcore_playstation2.c"

# Make sure the file exists
if [[ ! -f "$PLATFORM_FILE" ]]; then
    echo "Error: Could not find $PLATFORM_FILE"
    exit 1
fi

# Check if already patched
if grep -q "PGL_PATCHED_DYNAMIC_REGION" "$PLATFORM_FILE"; then
    echo "Raylib PS2 backend is already patched."
    exit 0
fi

echo "Patching raylib PS2 backend for dynamic region support..."

# 1. Replace SetGsCrt hardcoding
sed -i 's|SetGsCrt(1 \/\* interlaced \*\/,2 \/\* ntsc \*\/, 1 \/\* frame \*\/);|SetGsCrt(1 /* interlaced */, (CORE.Window.screen.height == 512) ? 1 : 2, 1 /* frame */); /* PGL_PATCHED_DYNAMIC_REGION */|g' "$PLATFORM_FILE"

# 2. Replace initGsMemoryForRaylib hardcoding
sed -i 's|initGsMemoryForRaylib(false);|initGsMemoryForRaylib(CORE.Window.screen.height == 512);|g' "$PLATFORM_FILE"

# 3. Remove window size overrides to respect InitWindow parameters
sed -i 's|CORE.Window.display.width = 640;//CORE.Window.screen.width;            // User desired width|CORE.Window.display.width = CORE.Window.screen.width; /* PGL_PATCHED_DYNAMIC_REGION */|g' "$PLATFORM_FILE"
sed -i 's|CORE.Window.display.height = 448;//CORE.Window.screen.height;          // User desired height|CORE.Window.display.height = CORE.Window.screen.height; /* PGL_PATCHED_DYNAMIC_REGION */|g' "$PLATFORM_FILE"
sed -i 's|CORE.Window.screen.width=640;|// PGL_PATCHED_DYNAMIC_REGION|g' "$PLATFORM_FILE"
sed -i 's|CORE.Window.screen.height=448;|// PGL_PATCHED_DYNAMIC_REGION|g' "$PLATFORM_FILE"

echo "Raylib patched successfully! Deleting standard libraylib.a cache to force rebuild."
rm -f thirdparty/raylib/src/libraylib.a

exit 0
