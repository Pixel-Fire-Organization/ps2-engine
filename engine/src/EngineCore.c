#include <malloc.h>
#include <raylib.h>
#include <stdlib.h>
#include "Engine.h"
#include "BuildEngineVersion.h"

// ps2gl C API — needed for pglAddGsMemSlot() after InitWindow.
// Must come after raylib.h (which sets up the PS2/GL include path).
#include <GL/ps2gl.h>

static void* s_UnifiedArenaBlock = NULL;
static bool s_IsGFXInitialized = false;

bool Engine_Init(EngineConfig config)
{
    Engine_InitDebug();
    Engine_LogInfo("Engine build version: %u", ENGINE_BUILD_VERSION);

    // Calculate total arena size from centralized constants
    size_t totalArenaSize = MEM_BLOCK_SCRIPT_SIZE + MEM_BLOCK_CONFIG_SIZE + MEM_BLOCK_LEVEL_DATA_SIZE;

    size_t totalRequiredMemory = totalArenaSize + MEM_POOL_MAIN_SIZE;

    // Safety Threshold Check (PS2 Hardware Limit)
    if (totalRequiredMemory > MEM_LIMIT_MAX_EE_RAM)
    {
        Engine_Panic("Engine Memory Map exceeds 30MB limit!");
        return false;
    }

    // Allocate unified arena block with 16KB alignment so that every arena slot
    // starts on a Quadword-aligned boundary without any initial padding waste.
    // memalign is used instead of malloc because malloc only guarantees 8-byte
    // alignment, which would force Internal_InitSlots to shift the first slot
    // forward and potentially overrun the allocated region.
    s_UnifiedArenaBlock = memalign(MEM_ARENA_SLOT_ALIGNMENT, totalArenaSize);
    void* poolMem = malloc(MEM_POOL_MAIN_SIZE);

    if (!s_UnifiedArenaBlock || !poolMem)
    {
        Engine_Panic("Failed to allocate engine memory — out of EE RAM");
        return false;
    }

    // Partition the unified block into specialized arenas
    Engine_ArenasInitSegmented(s_UnifiedArenaBlock);

    // Initialize the main memory pool
    Engine_PoolInitMain(poolMem, MEM_POOL_MAIN_SIZE, MEM_POOL_CHUNK_SIZE);

    Engine_LogInfo("Segmented Arena Allocated: %zu bytes", totalArenaSize);
    Engine_LogInfo("Video Mode: %dx%d (%s)", GFX_SCREEN_WIDTH, GFX_SCREEN_HEIGHT, GFX_SCREEN_REGION_STR);

    InitWindow(GFX_SCREEN_WIDTH, GFX_SCREEN_HEIGHT, config.windowTitle);
    s_IsGFXInitialized = IsWindowReady();

    // Initialize the specialized subsystems
    if (!Engine_Script_Init())
    {
        Engine_Panic("Lua Scripting subsystem failed to initialize");
        return false;
    }

    // Handle IO Subsystem automatically
    if (!Engine_IO_Init())
    {
        Engine_Panic("Async IO subsystem failed to initialize");
        return false;
    }

    // Initialize the Resource Manager
    if (!Engine_Resource_Init())
    {
        Engine_Panic("Resource Manager failed to initialize");
        return false;
    }

    return true;
}

bool Engine_Is_GFX_Initialized() { return s_IsGFXInitialized; }

void Engine_Update(void)
{
    Engine_IO_Update();
    Engine_Resource_Update();
    // Engine specific per-frame updates
}

void Engine_Close(void)
{
    Engine_Resource_Shutdown();
    Engine_IO_Shutdown();
    Engine_Script_Close();
    // CloseAudioDevice();
    CloseWindow();
    free(s_UnifiedArenaBlock);
    if (Engine_PoolGetBufferMain())
    {
        free(Engine_PoolGetBufferMain());
    }
}
