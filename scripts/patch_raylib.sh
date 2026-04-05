#!/usr/bin/bash

# patch_raylib.sh
# Patches Raylib for PS2 compatibility. Patches are idempotent — safe to re-run.
#
# Applied patches:
#   [1] PGL_PATCHED_DYNAMIC_REGION — respect InitWindow PAL/NTSC configuration
#   [2] PGL_PATCHED_FONT_ALIGN     — 16-byte aligned font atlas allocation for GS DMA

PLATFORM_FILE="thirdparty/raylib/src/platforms/rcore_playstation2.c"
RTEXT_FILE="thirdparty/raylib/src/rtext.c"

# Verify source files exist
if [[ ! -f "$PLATFORM_FILE" ]]; then
    echo "Error: Could not find $PLATFORM_FILE"
    exit 1
fi
if [[ ! -f "$RTEXT_FILE" ]]; then
    echo "Error: Could not find $RTEXT_FILE"
    exit 1
fi

PATCHED=0

# ---------------------------------------------------------------------------
# Patch 1: Dynamic PAL/NTSC region support
# ---------------------------------------------------------------------------
if grep -q "PGL_PATCHED_DYNAMIC_REGION" "$PLATFORM_FILE"; then
    echo "[1/2] Dynamic region patch already applied — skipping."
else
    echo "[1/2] Patching raylib PS2 backend for dynamic region support..."

    sed -i 's|SetGsCrt(1 \/\* interlaced \*\/,2 \/\* ntsc \*\/, 1 \/\* frame \*\/);|SetGsCrt(1 /* interlaced */, (CORE.Window.screen.height == 512) ? 1 : 2, 1 /* frame */); /* PGL_PATCHED_DYNAMIC_REGION */|g' "$PLATFORM_FILE"
    sed -i 's|initGsMemoryForRaylib(false);|initGsMemoryForRaylib(CORE.Window.screen.height == 512);|g' "$PLATFORM_FILE"
    sed -i 's|CORE.Window.display.width = 640;//CORE.Window.screen.width;            // User desired width|CORE.Window.display.width = CORE.Window.screen.width; /* PGL_PATCHED_DYNAMIC_REGION */|g' "$PLATFORM_FILE"
    sed -i 's|CORE.Window.display.height = 448;//CORE.Window.screen.height;          // User desired height|CORE.Window.display.height = CORE.Window.screen.height; /* PGL_PATCHED_DYNAMIC_REGION */|g' "$PLATFORM_FILE"
    sed -i 's|CORE.Window.screen.width=640;|// PGL_PATCHED_DYNAMIC_REGION|g' "$PLATFORM_FILE"
    sed -i 's|CORE.Window.screen.height=448;|// PGL_PATCHED_DYNAMIC_REGION|g' "$PLATFORM_FILE"

    echo "[1/2] Dynamic region patch applied."
    PATCHED=1
fi

# ---------------------------------------------------------------------------
# Patch 2: 16-byte aligned font atlas allocation for GS DMA
#
# ps2gl's glTexImage2D requires texture pixel data to be 16-byte aligned so
# it can DMA the data directly into GS VRAM (see ps2gl/src/texture.cpp line
# ~609: "texture data needs to be aligned to at least 16 bytes").
# RL_CALLOC gives no alignment guarantee, causing garbled text on PS2 because
# the DMA transfer produces corrupted texels.  memalign(16, ...) fixes this.
# ---------------------------------------------------------------------------
if grep -q "PGL_PATCHED_FONT_ALIGN" "$RTEXT_FILE"; then
    echo "[2/2] Font alignment patch already applied — skipping."
else
    echo "[2/2] Patching rtext.c: 16-byte aligned font atlas for GS DMA..."

    sed -i 's|\.data = RL_CALLOC(128\*128, 4),.*$|.data = memalign(16, 128*128*4), /* PGL_PATCHED_FONT_ALIGN: 16-byte aligned for GS DMA */|' "$RTEXT_FILE"

    echo "[2/2] Font alignment patch applied."
    PATCHED=1
fi

# Force a rebuild of libraylib.a if any patch was newly applied
if [[ $PATCHED -eq 1 ]]; then
    echo "Patches applied. Deleting libraylib.a cache to force rebuild."
    rm -f thirdparty/raylib/src/libraylib.a
else
    echo "All patches already applied — nothing to do."
fi

exit 0
