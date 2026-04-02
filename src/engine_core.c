#include "engine_core.h"
#include "engine_debug.h"
#include "engine_memory.h"
#include "engine_script.h"
#include <raylib.h>
#include <stdlib.h>

static void* s_UnifiedArenaBlock = NULL;

bool Engine_Init(EngineConfig config) {
    Engine_InitDebug();
    
    // Calculate total arena size
    size_t totalArenaSize = config.memoryMap.textureSize + 
                            config.memoryMap.meshSize + 
                            config.memoryMap.audioSize + 
                            config.memoryMap.scriptSize + 
                            config.memoryMap.uiSize + 
                            config.memoryMap.systemSize;

    // Allocate unified arena block and the pool block using standard malloc
    s_UnifiedArenaBlock = malloc(totalArenaSize);
    void* poolMem = malloc(config.memoryPoolSize);

    if (!s_UnifiedArenaBlock || !poolMem) {
        Engine_LogError("Failed to allocate main memory blocks! Total Arena: %zu, Pool: %zu", totalArenaSize, config.memoryPoolSize);
        return false;
    }
    
    // Partition the unified block into specialized arenas through the memory segmenter
    Engine_ArenasInitSegmented(s_UnifiedArenaBlock, config.memoryMap);

    // Initialize the main memory pool
    Engine_PoolInitMain(poolMem, config.memoryPoolSize, 256); // 256 byte chunks

    Engine_LogInfo("Segmented Arena Allocated: %zu bytes", totalArenaSize);
    Engine_LogInfo("Textures: %zu, Scripts: %zu, Meshes: %zu", 
                    config.memoryMap.textureSize, 
                    config.memoryMap.scriptSize, 
                    config.memoryMap.meshSize);

    InitWindow(config.screenWidth, config.screenHeight, config.windowTitle);
    
    // Initialize the Lua scripting subsystem
    if (!Engine_Script_Init()) {
        Engine_LogError("Failed to initialize Lua Scripting subsystem!");
        return false;
    }
    
    return true;
}

void Engine_Update(void) {
    // Engine specific per-frame updates
}

void Engine_Close(void) {
    Engine_Script_Close();
    // CloseAudioDevice();
    CloseWindow();
    free(s_UnifiedArenaBlock);
    free(Engine_PoolGetBufferMain());
}
