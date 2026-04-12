#!/usr/bin/bash

# patch_raylib.sh
# Patches Raylib for PS2 compatibility. Patches are idempotent — safe to re-run.
#
# Applied patches:
#   [1] PGL_PATCHED_DYNAMIC_REGION  — respect InitWindow PAL/NTSC configuration
#   [2] PGL_PATCHED_FONT_ALIGN      — 16-byte aligned font atlas allocation for GS DMA
#   [3] PGL_PATCHED_ATLAS_LIFETIME  — static font atlas buffer; prevents dangling-pointer
#                                     race between UnloadImage and ps2gl's lazy GS DMA
#   [4] PGL_PATCHED_NO_GAMEPAD      — remove libpad init/poll from raylib; EngineInput owns the pad

PLATFORM_FILE="thirdparty/raylib/src/platforms/rcore_playstation2.c"
RTEXT_FILE="thirdparty/raylib/src/rtext.c"
# Applied patches:
#   [4] PGL_PATCHED_NO_GAMEPAD       — remove libpad init/poll from raylib; EngineInput owns the pad

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
    echo "[1/4] Dynamic region patch already applied — skipping."
else
    echo "[1/4] Patching raylib PS2 backend for dynamic region support..."

    sed -i 's|SetGsCrt(1 \/\* interlaced \*\/,2 \/\* ntsc \*\/, 1 \/\* frame \*\/);|SetGsCrt(1 /* interlaced */, (CORE.Window.screen.height == 512) ? 1 : 2, 1 /* frame */); /* PGL_PATCHED_DYNAMIC_REGION */|g' "$PLATFORM_FILE"
    sed -i 's|initGsMemoryForRaylib(false);|initGsMemoryForRaylib(CORE.Window.screen.height == 512);|g' "$PLATFORM_FILE"
    sed -i 's|CORE.Window.display.width = 640;//CORE.Window.screen.width;            // User desired width|CORE.Window.display.width = CORE.Window.screen.width; /* PGL_PATCHED_DYNAMIC_REGION */|g' "$PLATFORM_FILE"
    sed -i 's|CORE.Window.display.height = 448;//CORE.Window.screen.height;          // User desired height|CORE.Window.display.height = CORE.Window.screen.height; /* PGL_PATCHED_DYNAMIC_REGION */|g' "$PLATFORM_FILE"
    sed -i 's|CORE.Window.screen.width=640;|// PGL_PATCHED_DYNAMIC_REGION|g' "$PLATFORM_FILE"
    sed -i 's|CORE.Window.screen.height=448;|// PGL_PATCHED_DYNAMIC_REGION|g' "$PLATFORM_FILE"

    echo "[1/4] Dynamic region patch applied."
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
    echo "[2/4] Font alignment patch already applied — skipping."
else
    echo "[2/4] Patching rtext.c: 16-byte aligned font atlas for GS DMA..."

    sed -i 's|\.data = RL_CALLOC(128\*128, 4),.*$|.data = memalign(16, 128*128*4), /* PGL_PATCHED_FONT_ALIGN: 16-byte aligned for GS DMA */|' "$RTEXT_FILE"

    echo "[2/4] Font alignment patch applied."
    PATCHED=1
fi

# ---------------------------------------------------------------------------
# Patch 3: Static font atlas buffer — eliminate dangling-pointer / cache race
#
# ps2gl stores only a POINTER to the pixel data passed to glTexImage2D.
# The actual GS DMA upload is deferred until the first draw that uses the
# texture (CMMTexture::Load → FlushCache(0) → SendImage).
#
# Raylib calls UnloadImage(imFont) immediately after LoadTextureFromImage,
# freeing that pointer.  On PS2's write-back EE cache, the arena / pool
# allocations made by Engine_Init between UnloadImage and the first draw
# can EVICT the atlas's dirty cache lines.  If those evictions happen before
# FlushCache(0) inside Load(), the DMA reads stale (garbage) data from main
# memory for the evicted GS-page rows, producing the characteristic pattern
# of specific glyphs being blank or garbled while neighbours are correct.
#
# Fix: declare a file-scope static buffer for the PS2 atlas.  The static
# buffer lives in BSS for the entire application lifetime — it is never
# freed, so the pointer stored by ps2gl is always valid.  The #if/#else
# preserves the memalign path for Dreamcast / N64 which share the same
# #if block and do not have the same lazy-DMA architecture.
#
# The two UnloadImage(imFont) calls inside LoadFontDefault are guarded
# with #if !defined(PLATFORM_PLAYSTATION2) to prevent freeing the static
# buffer (which would be a no-op anyway, but triggers UB).
# ---------------------------------------------------------------------------
if grep -q "PGL_PATCHED_ATLAS_LIFETIME" "$RTEXT_FILE"; then
    echo "[3/4] Atlas lifetime patch already applied — skipping."
else
    echo "[3/4] Patching rtext.c: static font atlas buffer for lazy GS DMA..."

    python3 - "$RTEXT_FILE" << 'PYEOF'
import sys, re

path = sys.argv[1]
with open(path, 'r') as f:
    content = f.read()

# ---- Step A: wrap the Image imFont block in a PS2 / non-PS2 sub-branch ----
old_block = (
    '    Image imFont = {\n'
    '        .data = memalign(16, 128*128*4), /* PGL_PATCHED_FONT_ALIGN: 16-byte aligned for GS DMA */\n'
    '        .width = 128,\n'
    '        .height = 128,\n'
    '        .mipmaps = 1,\n'
    '        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8\n'
    '    };'
)
new_block = (
    '#if defined(PLATFORM_PLAYSTATION2) /* PGL_PATCHED_ATLAS_LIFETIME: static buffer, never freed */\n'
    '    /* ps2gl stores only a pointer; DMA is lazy (fires at first draw after FlushCache).      */\n'
    '    /* UnloadImage would free this pointer before the DMA runs.  Static storage prevents    */\n'
    '    /* the dangling-pointer / write-back cache eviction race that garbles specific glyphs.   */\n'
    '    static uint32_t pgl_font_atlas_buf[128*128] __attribute__((aligned(16)));\n'
    '    Image imFont = {\n'
    '        .data = (void*)pgl_font_atlas_buf,\n'
    '        .width = 128,\n'
    '        .height = 128,\n'
    '        .mipmaps = 1,\n'
    '        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8\n'
    '    };\n'
    '#else /* Dreamcast / N64: keep memalign path */\n'
    '    Image imFont = {\n'
    '        .data = memalign(16, 128*128*4), /* PGL_PATCHED_FONT_ALIGN: 16-byte aligned for GS DMA */\n'
    '        .width = 128,\n'
    '        .height = 128,\n'
    '        .mipmaps = 1,\n'
    '        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8\n'
    '    };\n'
    '#endif'
)

if old_block not in content:
    print("ERROR: Could not locate Image imFont block (PGL_PATCHED_ATLAS_LIFETIME)", file=sys.stderr)
    sys.exit(1)

content = content.replace(old_block, new_block, 1)

# ---- Step B: guard UnloadImage(imFont) so the static buffer is never freed ----
content = re.sub(
    r'^( {4,12})UnloadImage\(imFont\);',
    (r'\1#if !defined(PLATFORM_PLAYSTATION2) /* PGL_PATCHED_ATLAS_LIFETIME: static buffer, do not free */'
     r'\n\1UnloadImage(imFont);\n\1#endif'),
    content,
    flags=re.MULTILINE
)

with open(path, 'w') as f:
    f.write(content)

print("PGL_PATCHED_ATLAS_LIFETIME applied.")
PYEOF

    echo "[3/4] Atlas lifetime patch applied."
    PATCHED=1
fi

# ---------------------------------------------------------------------------
# Patch 4: Disable raylib's built-in libpad init and polling
#
# raylib's InitPlatform calls padInit/padPortOpen/initializePad and its
# PollInputEvents polls the pad every frame via padRead.  EngineInput owns
# the full pad lifecycle (loading SIO2MAN + PADMAN IOP modules, padInit,
# padPortOpen, and per-frame padRead).  Running both systems on the same
# port causes double-open conflicts and unpredictable button state.
#
# This patch strips the libpad init block from InitPlatform (SIO2MAN /
# PADMAN module loads, padInit, padPortOpen, initializePad) and the
# gamepad polling block from PollInputEvents, while preserving SifInitRpc
# which is required for all IOP services (not just pad).
# ---------------------------------------------------------------------------
if grep -q "PGL_PATCHED_NO_GAMEPAD" "$PLATFORM_FILE"; then
    echo "[4/4] No-gamepad patch already applied — skipping."
else
    echo "[4/4] Patching raylib PS2 backend: disabling built-in libpad init/polling..."

    python3 - "$PLATFORM_FILE" << 'PYEOF'
import sys, re

path = sys.argv[1]
with open(path, 'r') as f:
    content = f.read()

# ---- Step A: Remove libpad init block from InitPlatform ----
# Match from the first SifLoadModule(SIO2MAN) up through the closing brace of
# the initializePad error block, leaving SifInitRpc and the TRACELOG that follows.
old_init = re.compile(
    r'    int ret=SifLoadModule\("rom0:SIO2MAN".*?'
    r'if\(!initializePad\(port, slot\)\).*?\n    \}\n',
    re.DOTALL
)
new_init = (
    '    /* PGL_PATCHED_NO_GAMEPAD: IOP pad modules and libpad init removed.\n'
    '     * EngineInput owns all pad lifecycle (SIO2MAN + PADMAN + padInit +\n'
    '     * padPortOpen). SifInitRpc above is preserved for all IOP services. */\n'
)

if not old_init.search(content):
    print("ERROR: Could not locate libpad init block in InitPlatform (PGL_PATCHED_NO_GAMEPAD)", file=sys.stderr)
    sys.exit(1)

content = old_init.sub(new_init, content, count=1)

# ---- Step B: Remove gamepad polling block from PollInputEvents ----
# Match from the "PlayStation 2 provisional" comment through the closing brace
# of the "if (ret != 0)" block and the trailing blank lines/spaces.
old_poll = re.compile(
    r'    //PlayStation 2 provisional\n.*?'
    r'    \}\n\n        \n',
    re.DOTALL
)
new_poll = (
    '    /* PGL_PATCHED_NO_GAMEPAD: gamepad polling removed; EngineInput owns pad state. */\n\n'
)

if not old_poll.search(content):
    print("ERROR: Could not locate gamepad polling block in PollInputEvents (PGL_PATCHED_NO_GAMEPAD)", file=sys.stderr)
    sys.exit(1)

content = old_poll.sub(new_poll, content, count=1)

with open(path, 'w') as f:
    f.write(content)

print("PGL_PATCHED_NO_GAMEPAD applied.")
PYEOF

    echo "[4/4] No-gamepad patch applied."
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
