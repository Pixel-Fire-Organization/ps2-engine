#include <cstdlib>
#include <malloc.h>
#include "Engine.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include "EngineAchievement.h"
#include "EngineInput.h"
#include "graphics/Renderer.h"
#include "platform/Platform.h"

static Renderer* g_Renderer = nullptr;
static Platform* g_Platform = nullptr;
static const char* s_ResourceLocationToken = NULL;

// Installed by Engine_Main before Engine_Init, so every subsystem below can
// reach the OS through one instance instead of a compile-time #ifdef.
Platform* Engine_GetPlatform() { return g_Platform; }
void Engine_SetPlatform(Platform* platform) { g_Platform = platform; }

bool Engine_InitMemory(Platform* platform)
{
    if (!platform)
        return false;

    g_Platform = platform;

    // The platform owns the memory map: it knows its own RAM ceiling, enforces it,
    // and allocates with the alignment its DMA hardware needs.
    EngineMemoryMap memory;
    if (!platform->GetMemory().Reserve(&memory))
    {
        Engine_LogError("Engine memory map could not be reserved");
        return false;
    }

    Engine_ArenasInitSegmented(memory.arenaBlock);
    Engine_PoolInitMain(memory.poolBlock, memory.poolSize, memory.poolChunkSize);
    Engine_LogInfo("Segmented Arena Allocated: %zu bytes", memory.arenaBlockSize);
    return true;
}

bool Engine_Init(EngineConfig config, Platform* platform, Renderer* renderer)
{
    if (!platform || !renderer)
        return false;

    g_Platform = platform;
    g_Renderer = renderer;

    Engine_InitDebug();

    // The game chose the set before we got here; refuse an incoherent one now,
    // while the message can still name what is missing.
    Engine_Subsystems_Validate();

    Engine_PerfLogger_Init(Engine_Subsystem_IsEnabled(EngineSubsystem::PerfLogger));
    s_ResourceLocationToken = config.resourceLocationToken;

    if (!g_Renderer->IsInitialized())
    {
        Engine_Panic("Renderer failed to initialize");
    }

    if (Engine_Subsystem_IsEnabled(EngineSubsystem::Io) && !Engine_IO_Init())
    {
        Engine_Panic("Async IO subsystem failed to initialize");
    }

    // Archive subsystem, then mount the boot archive (RASSETS.PS2R) if present.
    // Missing archive is non-fatal: assets then resolve as loose files on disc
    // (host: dev builds and the loose->archive transition rely on this fallback).
    if (Engine_Subsystem_IsEnabled(EngineSubsystem::Archive))
    {
        Engine_Archive_Init();

        char bootArchive[IO_FILE_MAX_PATH];
        const char* token = s_ResourceLocationToken ? s_ResourceLocationToken : "cdrom0:";
        if (Engine_BuildPath(token, ARCH_BOOT_ARCHIVE_NAME, bootArchive, sizeof(bootArchive)))
        {
            if (Engine_Archive_Mount(bootArchive) < 0)
                Engine_LogInfo("No boot archive at '%s' — assets will resolve as loose files.", bootArchive);
        }
    }

    if (Engine_Subsystem_IsEnabled(EngineSubsystem::Resource) && !Engine_Resource_Init())
    {
        Engine_Panic("Resource Manager failed to initialize");
    }

    if (Engine_Subsystem_IsEnabled(EngineSubsystem::Input))
        Engine_Input_Init();

    if (Engine_Subsystem_IsEnabled(EngineSubsystem::Achievement))
        Engine_Achievement_Init(nullptr);

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
static uint32_t s_FrameCount = 0;

void Engine_Update()
{
    double currentTime = g_Platform ? g_Platform->GetTimeSeconds() : 0.0;

    // Initialise start/last time on first frame
    if (s_EngineLastTime == 0.0)
    {
        s_EngineLastTime = currentTime;
        s_EngineStartTime = currentTime;
    }

    s_EngineDeltaTime = (float)(currentTime - s_EngineLastTime);
    s_EngineLastTime = currentTime;

    // Platform::GetTimeSeconds is monotonic (the PS2 backend absorbs the ~71.6
    // min clock() wrap), but clamp anyway so a misbehaving platform can never
    // teleport anything that integrates dt.
    if (s_EngineDeltaTime < 0.0f)
        s_EngineDeltaTime = 0.0f;

    if (s_EngineDeltaTime > 0)
    {
        s_EngineFPS = 1.0f / s_EngineDeltaTime;
    }

    // One input read per frame, before anything can query it.
    if (g_Platform)
        g_Platform->PollInput();

    Engine_IO_Update();
    Engine_Resource_Update();
    s_FrameCount++;
}

uint32_t Engine_GetFrameCount() { return s_FrameCount; }

float Engine_GetDeltaTime() { return s_EngineDeltaTime; }
float Engine_GetFPS() { return s_EngineFPS; }
float Engine_GetTotalTime() { return g_Platform ? (float)(g_Platform->GetTimeSeconds() - s_EngineStartTime) : 0.0f; }

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
    // Reverse of bring-up. Each subsystem is torn down only if it was brought
    // up, so an absent one is not shut down twice or shut down never-started.
    if (Engine_Subsystem_IsEnabled(EngineSubsystem::Achievement))
        Engine_Achievement_Close();

    if (Engine_Subsystem_IsEnabled(EngineSubsystem::Input))
        Engine_Input_Shutdown();

    if (Engine_Subsystem_IsEnabled(EngineSubsystem::Resource))
        Engine_Resource_Shutdown();

    // Close archive file descriptors while the IO file semaphore is still valid.
    if (Engine_Subsystem_IsEnabled(EngineSubsystem::Archive))
        Engine_Archive_Shutdown();

    if (Engine_Subsystem_IsEnabled(EngineSubsystem::Io))
        Engine_IO_Shutdown();

    if (g_Platform && g_Renderer)
    {
        g_Platform->DestroyRenderer(g_Renderer);
        g_Renderer = nullptr;
    }

    // The platform allocated the arena and pool blocks, so it frees them.
    if (g_Platform)
        g_Platform->GetMemory().Release();
}

const char* Engine_GetResourceLocationToken(void) { return s_ResourceLocationToken; }

bool Engine_BuildPath(const char* token, const char* relativePath, char* outBuf, size_t bufSize)
{
    // `token` is ignored: the active platform owns device-token grammar, and a
    // caller cannot know it (cdrom0: needs a ";1" suffix, host: is relative to
    // the executable, a desktop platform has neither). Kept in the signature so
    // the existing call sites are untouched.
    UNUSED_VAR(token);

    Platform* platform = Engine_GetPlatform();
    if (!platform)
        return false;
    return platform->BuildPath(relativePath, outBuf, bufSize);
}

void Engine_Path_Canonical(const char* in, char* out)
{
    if (!out)
        return;
    out[0] = '\0';
    if (!in)
        return;

    // Skip a leading device token ("cdrom0:", "mass0:", "host:", ...): everything
    // up to and including the first ':'. PS2 asset keys have no other colon.
    const char* p = strchr(in, ':');
    p = p ? p + 1 : in;

    // Copy, converting '\\' -> '/' and upper-casing.
    char tmp[IO_FILE_MAX_PATH];
    size_t n = 0;
    for (; *p && n < IO_FILE_MAX_PATH - 1; ++p)
    {
        char c = (*p == '\\') ? '/' : static_cast<char>(toupper(static_cast<unsigned char>(*p)));
        tmp[n++] = c;
    }
    tmp[n] = '\0';

    // Strip a trailing ";N" version suffix (cdrom paths end with ";1").
    char* semi = strrchr(tmp, ';');
    if (semi && semi[1] != '\0')
    {
        bool allDigits = true;
        for (char* q = semi + 1; *q; ++q)
        {
            if (!isdigit(static_cast<unsigned char>(*q)))
            {
                allDigits = false;
                break;
            }
        }
        if (allDigits)
            *semi = '\0';
    }

    // Drop leading slashes so device-relative and bare keys align.
    char* start = tmp;
    while (*start == '/')
        ++start;

    strncpy(out, start, IO_FILE_MAX_PATH - 1);
    out[IO_FILE_MAX_PATH - 1] = '\0';
}
