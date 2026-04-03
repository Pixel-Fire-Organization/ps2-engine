#ifndef CONSTANTS_H
#define CONSTANTS_H

/**
 * @file constants.h
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
#define MEM_BLOCK_TEXTURE_SIZE        (10 * 1024 * 1024)
#define MEM_BLOCK_TEXTURE_SLOTS       10

#define MEM_BLOCK_MESH_SIZE           (2 * 1024 * 1024)
#define MEM_BLOCK_MESH_SLOTS          8

#define MEM_BLOCK_AUDIO_SIZE          (2 * 1024 * 1024)
#define MEM_BLOCK_AUDIO_SLOTS         8

#define MEM_BLOCK_SCRIPT_SIZE         (2 * 1024 * 1024)
#define MEM_BLOCK_SCRIPT_SLOTS        16

#define MEM_BLOCK_UI_SIZE             (1 * 1024 * 1024)
#define MEM_BLOCK_UI_SLOTS            4

#define MEM_BLOCK_SYSTEM_SIZE         (2 * 1024 * 1024)
#define MEM_BLOCK_SYSTEM_SLOTS        4

// General Pool Configuration
#define MEM_POOL_MAIN_SIZE            (1 * 1024 * 1024)
#define MEM_POOL_CHUNK_SIZE           256

// Low-level Memory Helpers
#define MEM_ARENA_MAX_SLOTS           32
#define MEM_ARENA_SLOT_ALIGNMENT      (16 * 1024)

// --- IO (Input/Output) ---
#define IO_FILE_MAX_PATH              256
#define IO_ASYNC_MAX_REQUESTS         16
#define IO_THREAD_SLEEP_USEC          1000

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
