#ifndef CONSTANTS_H
#define CONSTANTS_H

// --- GFX (Graphics) ---
#define GFX_SCREEN_REGION_PAL 0
#define GFX_SCREEN_REGION_NTSC 1

// PS2 GS VRAM texture page budget (kPsm32 / 32-bit colour).
// One GS page = 64×32 pixels = 2048 bytes at kPsm32.
// Pages needed = ceil(W/64) * ceil(H/32).
// Raylib's initGsMemoryForRaylib() uses GS pages 0-505 (PAL) / 0-475 (NTSC).
// The largest free slot it registers is 64 pages = 512×256 px.
// Pages beyond 511 alias the framebuffer area (GS TBP is 14-bit, wraps at 512).
#define GFX_GS_PAGE_WIDTH_PSM32 64 // pixels per page (width)
#define GFX_GS_PAGE_HEIGHT_PSM32 32 // pixels per page (height)

// SCE_GS_PSMCT32 = 0x00 — numeric constant for 32-bit colour GS pixel format.
// Used in pglAddGsMemSlot() calls from C code (GS::kPsm32 is C++ only).
#define GFX_GS_PSM_CT32 0u

// Engine hard cap: textures exceeding this page count are rejected at load time.
// Derived from the largest slot Raylib registers (512×256 = 64 pages).
#define GFX_MAX_TEXTURE_GS_PAGES 64

// Total GS VRAM pages available for PSM32 textures across ALL Raylib texture slots.
// Raylib registers the same layout for both PAL and NTSC:
//   8×1-page (64×32)  + 8×2-page (64×64)  + 6×8-page (128×128)
//   + 2×32-page (256×256) + 2×64-page (512×256) = 264 pages.
// If all pages are occupied, ps2gl LRU-evicts the oldest texture from GS VRAM to
// make room (the GL texture object is preserved in CPU RAM and re-uploaded on next use).
#define GFX_GS_TEXTURE_PAGE_BUDGET 264

// Absolute dimension caps. Both must be power-of-2 and ≤ 512.
// The real binding constraint is GFX_MAX_TEXTURE_GS_PAGES (page count), not these,
// since a 512×512 texture (128 pages) would be rejected even though each dimension is ≤ 512.
#define GFX_MAX_TEXTURE_WIDTH 512
#define GFX_MAX_TEXTURE_HEIGHT 512
#define GFX_SCREEN_PAL_WIDTH 640
#define GFX_SCREEN_PAL_HEIGHT 512
#define GFX_SCREEN_NTSC_WIDTH 640
#define GFX_SCREEN_NTSC_HEIGHT 448

// Automatic selection based on CMake definitions
#ifdef REGION_PAL
#define GFX_SCREEN_WIDTH GFX_SCREEN_PAL_WIDTH
#define GFX_SCREEN_HEIGHT GFX_SCREEN_PAL_HEIGHT
#define GFX_SCREEN_REGION GFX_SCREEN_REGION_PAL
#define GFX_SCREEN_REGION_STR "PAL"
#else
#define GFX_SCREEN_WIDTH GFX_SCREEN_NTSC_WIDTH
#define GFX_SCREEN_HEIGHT GFX_SCREEN_NTSC_HEIGHT
#define GFX_SCREEN_REGION GFX_SCREEN_REGION_NTSC
#define GFX_SCREEN_REGION_STR "NTSC"
#endif

// --- MEM (Memory) ---
// Hard limit for engine-managed memory (PS2 has 32MB total)
#define MEM_LIMIT_MAX_EE_RAM (30 * 1024 * 1024)

// Segmented Arena Map (Fixed Sizes)
// GFX resources are managed by Raylib — only engine-internal arenas remain.
#define MEM_BLOCK_SCRIPT_SIZE (4 * 1024 * 1024) // 256 KB per unit (4 MB / 16 slots)
#define MEM_BLOCK_SCRIPT_SLOTS 16

#define MEM_BLOCK_CONFIG_SIZE (1 * 1024 * 1024)
#define MEM_BLOCK_CONFIG_SLOTS 4

#define MEM_BLOCK_LEVEL_DATA_SIZE (4 * 1024 * 1024)
#define MEM_BLOCK_LEVEL_DATA_SLOTS 8

// General Pool Configuration (scratch allocator for short-lived temp objects)
#define MEM_POOL_MAIN_SIZE (1 * 1024 * 1024)
#define MEM_POOL_CHUNK_SIZE 256

// Low-level Memory Helpers
#define MEM_ARENA_MAX_SLOTS 32
#define MEM_ARENA_SLOT_ALIGNMENT (16 * 1024)

// --- IO (Input/Output) ---
#define IO_FILE_MAX_PATH 256
#define IO_ASYNC_MAX_REQUESTS 16
#define IO_THREAD_SLEEP_USEC 1000

// Stack size for the background IO thread. Must be a power-of-2; 32KB is
// sufficient for a single fopen/fread/fclose + a 256-byte filepath local.
#define IO_THREAD_STACK_SIZE (32 * 1024)

// Maximum file size for a single async IO read.
// Files exceeding this limit are rejected at read-time with an error; the
// callback receives (NULL, 0, userData) so callers can handle the failure.
// A SINGLE shared buffer of this size lives in BSS (512 KB total).
// The IO thread and main thread take turns owning it via s_IOBufferSema:
//   IO thread acquires → reads file → main thread dispatches callback → releases.
// This serial ownership avoids a per-slot buffer array (which would be
// IO_ASYNC_MAX_REQUESTS × IO_READ_BUFFER_SIZE = 8 MB and pushes BSS beyond
// the PS2 EE TLB coverage window, causing TLB misses at startup).
#define IO_READ_BUFFER_SIZE (512 * 1024)

// --- RESOURCE (Resource Manager) ---
#define RES_MAX_ENTRIES 64
#define RES_MAX_DEPENDENCIES 8
#define RES_ASSET_MAGIC 0x50533241 /* "PS2A" in little-endian */

// --- LEVEL ---
#define LEVEL_MAX_RESOURCES_COUNT 16

// --- SCRIPTING (Lua Management) ---
#define SCRIPTING_LUA_MAX_UNITS 8
#define SCRIPTING_LUA_CODE_SLOT_SIZE (256 * 1024)

// Relative filename of the entry-point Lua script.
// The full path is constructed at runtime using Engine_BuildPath() with the
// active resource location token so it works across cdrom, host, and mass devices.
#define SCRIPTING_MAIN_SCRIPT_FILENAME "MAIN.LUA"

// Camera slot limits for the scripting camera registry.
#define SCRIPTING_MAX_CAMERAS_3D 4
#define SCRIPTING_MAX_CAMERAS_2D 4

// Frames a camera must remain unused before its slot is reclaimed (LFU eviction).
#define SCRIPTING_CAM_IDLE_FRAMES_EVICT 1000

// Minimum seconds between successive begin_mode_3d / begin_mode_2d re-entries
// when already inside the same mode. Prevents accidental rapid GPU overhead.
#define SCRIPTING_MODE_REENTRY_COOLDOWN_SEC 1.0

// --- APP (App-facing API) ---
// Maximum number of simultaneously open file descriptors via io.open / io.open_write.
#define APP_MAX_FILE_SLOTS MEM_BLOCK_CONFIG_SLOTS
// Maximum bytes that may be read into a single file slot (one config-arena slot).
#define APP_MAX_FILE_DATA_SIZE (MEM_BLOCK_CONFIG_SIZE / MEM_BLOCK_CONFIG_SLOTS)

// --- LEVEL FILE FORMAT (.ps2l) ---
// Binary format: LevelFileHeader (8 bytes) + packed Level struct.
#define LEVEL_FILE_MAGIC 0x4C325350u /* "PS2L" in little-endian */
#define LEVEL_FILE_VERSION 1u
#define LEVEL_FILE_EXT ".ps2l"

// --- PANIC (BSOD System) ---
#define PANIC_UI_PADDING 40
#define PANIC_UI_FONT_SIZE_TITLE 40
#define PANIC_UI_FONT_SIZE_SUBTITLE 20
#define PANIC_UI_FONT_SIZE_BODY 20
#define PANIC_UI_FONT_SIZE_FOOTER 10

// --- GAME PAD ---
#define MAX_GAME_PAD_PORTS 2
#define MAX_JOYSTICKS 2

#define LOG_STRING_MAX_SIZE 256

#endif // CONSTANTS_H
