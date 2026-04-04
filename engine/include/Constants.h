#ifndef CONSTANTS_H
#define CONSTANTS_H

/**
 * @file Constants.h
 * @brief Centralized engine constants and configuration.
 * 
 * Naming convention: <ENGINE_CATEGORY>_<SUBMODULE>_<ID>
 */

// --- GFX (Graphics) ---
#define GFX_SCREEN_REGION_PAL         0
#define GFX_SCREEN_REGION_NTSC        1
#define GFX_SCREEN_PAL_WIDTH          640
#define GFX_SCREEN_PAL_HEIGHT         512
#define GFX_SCREEN_NTSC_WIDTH         640
#define GFX_SCREEN_NTSC_HEIGHT        448

// Automatic selection based on CMake definitions
#ifdef REGION_PAL
#define GFX_SCREEN_WIDTH              GFX_SCREEN_PAL_WIDTH
#define GFX_SCREEN_HEIGHT             GFX_SCREEN_PAL_HEIGHT
#define GFX_SCREEN_REGION             GFX_SCREEN_REGION_PAL
#define GFX_SCREEN_REGION_STR         "PAL"
#else
#define GFX_SCREEN_WIDTH              GFX_SCREEN_NTSC_WIDTH
#define GFX_SCREEN_HEIGHT             GFX_SCREEN_NTSC_HEIGHT
#define GFX_SCREEN_REGION             GFX_SCREEN_REGION_NTSC
#define GFX_SCREEN_REGION_STR         "NTSC"
#endif

// --- MEM (Memory) ---
// Hard limit for engine-managed memory (PS2 has 32MB total)
#define MEM_LIMIT_MAX_EE_RAM          (30 * 1024 * 1024)

// Segmented Arena Map (Fixed Sizes)
// GFX resources are managed by Raylib — only engine-internal arenas remain.
#define MEM_BLOCK_SCRIPT_SIZE         (2 * 1024 * 1024)
#define MEM_BLOCK_SCRIPT_SLOTS        16

#define MEM_BLOCK_CONFIG_SIZE         (1 * 1024 * 1024)
#define MEM_BLOCK_CONFIG_SLOTS        4

#define MEM_BLOCK_LEVEL_DATA_SIZE     (4 * 1024 * 1024)
#define MEM_BLOCK_LEVEL_DATA_SLOTS    8

// General Pool Configuration (scratch allocator for short-lived temp objects)
#define MEM_POOL_MAIN_SIZE            (1 * 1024 * 1024)
#define MEM_POOL_CHUNK_SIZE           256

// Low-level Memory Helpers
#define MEM_ARENA_MAX_SLOTS           32
#define MEM_ARENA_SLOT_ALIGNMENT      (16 * 1024)

// --- IO (Input/Output) ---
#define IO_FILE_MAX_PATH              256
#define IO_ASYNC_MAX_REQUESTS         16
#define IO_THREAD_SLEEP_USEC          1000
// Stack size for the background IO thread. Must be a power-of-2; 32KB is
// sufficient for a single fopen/fread/fclose + a 256-byte filepath local.
#define IO_THREAD_STACK_SIZE          (32 * 1024)

// --- RESOURCE (Resource Manager) ---
#define RES_MAX_ENTRIES               64
#define RES_MAX_DEPENDENCIES          8
#define RES_ASSET_MAGIC               0x50533241  /* "PS2A" in little-endian */

// --- LEVEL ---
#define LEVEL_MAX_RESOURCES_COUNT     16

// --- SCRIPTING (Lua Management) ---
#define SCRIPTING_LUA_MAX_UNITS       8
#define SCRIPTING_LUA_CODE_SLOT_SIZE  (256 * 1024)

// --- PANIC (BSOD System) ---
#define PANIC_UI_PADDING              40
#define PANIC_UI_FONT_SIZE_TITLE      40
#define PANIC_UI_FONT_SIZE_SUBTITLE   20
#define PANIC_UI_FONT_SIZE_BODY       20
#define PANIC_UI_FONT_SIZE_FOOTER     10

#endif // CONSTANTS_H
