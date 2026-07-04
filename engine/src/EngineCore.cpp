#include <cstdlib>
#include <malloc.h>
#include "Engine.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include "EngineInput.h"
#include "graphics/Renderer.h"

// Select the renderer backend at compile time. EngineCore.h (via Engine.h above)
// defaults RENDERER_BACKEND_PS2GL when no backend define was supplied.
#ifdef RENDERER_BACKEND_GIFTAG
    #include "graphics/TagRenderer.h"
#else
    #include "graphics/GLRenderer.h"
#endif

static void* s_UnifiedArenaBlock = nullptr;
static Renderer* g_Renderer = nullptr;
static const char* s_ResourceLocationToken = NULL;

bool Engine_Init(EngineConfig config)
{
    Engine_InitDebug();
    Engine_PerfLogger_Init(config.enablePerfLogger);
    s_ResourceLocationToken = config.resourceLocationToken;

    // Calculate total arena size from centralized constants
    constexpr size_t totalArenaSize = MEM_BLOCK_SCRIPT_SIZE + MEM_BLOCK_CONFIG_SIZE + MEM_BLOCK_LEVEL_DATA_SIZE + MEM_BLOCK_RENDERER_SIZE;
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

#ifdef RENDERER_BACKEND_GIFTAG
    g_Renderer = new TagRenderer(config);
    Engine_LogInfo("Engine initialized with GIFTAG renderer.");
#else
    g_Renderer = new GLRenderer(config);
    Engine_LogInfo("Engine initialized with PS2GL renderer.");
#endif
    if (!g_Renderer || !g_Renderer->IsInitialized())
    {
        Engine_Panic("Renderer failed to initialize");
        return false;
    }

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

static float s_EngineDeltaTime = 0.0f;
static float s_EngineFPS = 0.0f;
static double s_EngineLastTime = 0.0;
static double s_EngineStartTime = 0.0;
static float s_LogicTime = 0.0f;
static float s_RenderTime = 0.0f;
static float s_WaitTime = 0.0f;

void Engine_Update()
{
    double currentTime = (double)clock() / CLOCKS_PER_SEC;

    // Initialise start/last time on first frame
    if (s_EngineLastTime == 0.0)
    {
        s_EngineLastTime = currentTime;
        s_EngineStartTime = currentTime;
    }

    s_EngineDeltaTime = (float)(currentTime - s_EngineLastTime);
    s_EngineLastTime = currentTime;

    // clock() is 32-bit microseconds and wraps to 0 after ~71.6 min, which would
    // otherwise make one frame's dt a huge negative number (teleporting anything
    // that integrates dt). Clamp to a non-negative, sane step across the wrap.
    if (s_EngineDeltaTime < 0.0f)
        s_EngineDeltaTime = 0.0f;

    if (s_EngineDeltaTime > 0)
    {
        s_EngineFPS = 1.0f / s_EngineDeltaTime;
    }

    Engine_IO_Update();
    Engine_Resource_Update();
}

float Engine_GetDeltaTime() { return s_EngineDeltaTime; }
float Engine_GetFPS() { return s_EngineFPS; }
float Engine_GetTotalTime() { return (float)(((double)clock() / CLOCKS_PER_SEC) - s_EngineStartTime); }

void Engine_ReportFrameStats(float logicTime, float renderTime, float waitTime)
{
    s_LogicTime = logicTime;
    s_RenderTime = renderTime;
    s_WaitTime = waitTime;
}

float Engine_GetLogicTime() { return s_LogicTime; }
float Engine_GetRenderTime() { return s_RenderTime; }
float Engine_GetWaitTime() { return s_WaitTime; }

void Engine_Close()
{
    Engine_Resource_Shutdown();
    Engine_IO_Shutdown();
    Engine_Script_Close();
    // CloseAudioDevice();
    if (g_Renderer)
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
