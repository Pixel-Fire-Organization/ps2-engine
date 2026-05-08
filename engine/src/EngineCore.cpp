#include <cstdlib>
#include <malloc.h>
#include <raylib.h>
#include "Engine.h"

#include "EngineInput.h"
#include "graphics/RaylibRenderer.h"
#include "graphics/Renderer.h"
#include <cstring>
#include <cstdio>

static void* s_UnifiedArenaBlock = nullptr;
static Renderer* g_Renderer = nullptr;
static const char* s_ResourceLocationToken = NULL;

bool Engine_Init(EngineConfig config)
{
    Engine_InitDebug();
    Engine_PerfLogger_Init(config.enablePerfLogger);
    s_ResourceLocationToken = config.resourceLocationToken;

    // Calculate total arena size from centralized constants
    constexpr size_t totalArenaSize = MEM_BLOCK_SCRIPT_SIZE + MEM_BLOCK_CONFIG_SIZE + MEM_BLOCK_LEVEL_DATA_SIZE;
    constexpr size_t totalRequiredMemory = totalArenaSize + MEM_POOL_MAIN_SIZE;

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
        free(s_UnifiedArenaBlock);
        free(poolMem);
        return false;
    }

    // Partition the unified block into specialized arenas
    Engine_ArenasInitSegmented(s_UnifiedArenaBlock);

    // Initialize the main memory pool
    Engine_PoolInitMain(poolMem, MEM_POOL_MAIN_SIZE, MEM_POOL_CHUNK_SIZE);
    Engine_LogInfo("Segmented Arena Allocated: %zu bytes", totalArenaSize);

    g_Renderer = new RaylibRenderer(config);

    Engine_LogInfo("Initializing Game pad at port 0");
    if (!InitPad(0, true))
    {
        Engine_Panic("No gamepad at port 0!");
        return false;
    }

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

bool Engine_Is_GFX_Initialized() { return !g_Renderer ? false : g_Renderer->IsInitialized(); }

Renderer* Engine_GetRenderer() { return g_Renderer; }

void Engine_Update()
{
    Engine_IO_Update();
    Engine_Resource_Update();
}

void Engine_Close()
{
    Engine_Resource_Shutdown();
    Engine_IO_Shutdown();
    Engine_Script_Close();
    // CloseAudioDevice();
    g_Renderer->Shutdown();
    free(s_UnifiedArenaBlock);
    if (Engine_PoolGetBufferMain())
    {
        free(Engine_PoolGetBufferMain());
    }
}

const char* Engine_GetResourceLocationToken(void) { return s_ResourceLocationToken; }

bool Engine_BuildPath(const char* token, const char* relativePath, char* outBuf, size_t bufSize)
{
    int written = 0;

    if (!token || !relativePath || !outBuf || bufSize == 0)
        return false;

    size_t tokenLen = strlen(token);

    if (tokenLen >= 4) // smallest is host & hdd0
    {
        if (token[0] == 'c') // cdrom0: → "cdrom0:<PATH>;1"
            written = snprintf(outBuf, bufSize, "cdrom0:%s;1", relativePath);
        else if (token[0] == 'm') // mass0: → "mass0:<PATH>"
            written = snprintf(outBuf, bufSize, "mass0:%s", relativePath);
        else if (token[0] == 'h' && token[1] == 'd') // hdd0: → "hdd0:<PATH>"
            written = snprintf(outBuf, bufSize, "hdd0:%s", relativePath);
        else if (token[0] == 'h' && token[1] == 'o') // host: → "host:<DIR>/<PATH>"
        {
            // Extract the directory part from host:path/to/elf
            char baseDir[256];
            strncpy(baseDir, token, sizeof(baseDir));
            baseDir[sizeof(baseDir) - 1] = '\0';
            char* lastSlash = strrchr(baseDir, '/');
            char* lastBackslash = strrchr(baseDir, '\\');
            char* lastColon = strchr(baseDir, ':');
            
            char* splitPoint = (lastSlash > lastBackslash) ? lastSlash : lastBackslash;
            if (!splitPoint) splitPoint = lastColon;

            if (splitPoint)
            {
                *(splitPoint + 1) = '\0'; // keep the slash/colon
                written = snprintf(outBuf, bufSize, "%s%s", baseDir, relativePath);
            }
            else
            {
                written = snprintf(outBuf, bufSize, "%s%s", token, relativePath);
            }
        }
    }

    if (written >= 0 && (size_t)written < bufSize)
    {
        for (int i = 0; i < written; ++i)
        {
            if (outBuf[i] == '\\')
                outBuf[i] = '/';
        }
        return true;
    }
    return false;
}
