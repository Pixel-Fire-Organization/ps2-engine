#include "EngineApp.h"
#include "Engine.h"
#include "EngineCore.h"
#include "EngineInput.h"
#include "GameAPI.h"

#include <cstdio>
#include <cstring>
#include <ctime>

// ---------------------------------------------------------------------------
// Internal exit flag — set by EngineApp_OnExitRequested (called by game::Exit()).
// ---------------------------------------------------------------------------
static bool s_ExitRequested = false;
static const char* s_cdRomResourceLocationToken = "cdrom0:";
static const char* s_massResourceLocationToken = "mass0:";
static const char* s_hddResourceLocationToken = "hdd0:";
static const char* s_hostResourceLocationToken = "host:";

const char* FormatResourceLocationToken(const char* locationToken);

void EngineApp_OnExitRequested() { s_ExitRequested = true; }

// ---------------------------------------------------------------------------
// Public shell API
// ---------------------------------------------------------------------------

bool EngineStart(const char* resourceLocationToken)
{
    if (resourceLocationToken == NULL)
        resourceLocationToken = s_cdRomResourceLocationToken;
    else
        resourceLocationToken = FormatResourceLocationToken(resourceLocationToken);

    EngineConfig config;
    config.windowTitle = "PS2 Engine";
    config.resourceLocationToken = resourceLocationToken;
    config.enablePerfLogger = true;

    if (!Engine_Init(config))
    {
        Engine_Panic("Engine_Init failed — hardware or memory error");
        return false;
    }

    s_ExitRequested = false;

    // Gameplay (and UI) is authored in C++: hand control to the game module.
    Engine_LogInfo("EngineStart: token='%s' — starting C++ game module", resourceLocationToken);
    GameInit();
    return true;
}

void EngineUpdate()
{
    static double frameStartTime = 0;
    static double scriptEndTime = 0;
    static double renderEndTime = 0;

    frameStartTime = (double)clock() / CLOCKS_PER_SEC;

    Engine_Update();
    float dt = Engine_GetDeltaTime();

    // 1. Gameplay Phase (C++). Measured as the "Logic" bucket — the same slot
    //    the Lua OnUpdate used to occupy, so the perf dump stays comparable.
    GameUpdate(dt);
    scriptEndTime = (double)clock() / CLOCKS_PER_SEC;

    // 2. Renderer Phase (CPU-side transforms)
    Renderer* r = Engine_GetRenderer();
    if (r && r->IsInitialized())
    {
        r->BeginFrame();
        r->Render();
        Engine_DrawDebugOverlay();
        r->EndFrame();
    }

    renderEndTime = (double)clock() / CLOCKS_PER_SEC;

    double frameEndTime = (double)clock() / CLOCKS_PER_SEC;

    // Detailed Stats Reporting
    // CPU Logic = Start to ScriptEnd
    // CPU Render = ScriptEnd to RenderEnd (Mega-batching)
    // GS Wait = RenderEnd to Vblank/Finish
    Engine_ReportFrameStats((float)(scriptEndTime - frameStartTime), // Logic
                            (float)(renderEndTime - scriptEndTime), // Render
                            (float)(frameEndTime - renderEndTime) // Wait
    );

    Engine_PerfLogger_Tick();
}

bool EngineExited() { return s_ExitRequested; }

void EngineStop() { Engine_Close(); }

const char* FormatResourceLocationToken(const char* locationToken)
{
    /**
     *  Normalise an argv[0]-style path to the appropriate storage token.
     *  argv[0] on PS2 typically looks like "cdrom0:\MAIN.ELF;1" or "host:MAIN.ELF".
     *  We match only the leading device name, not the full path.
     *
     *  Supported storage locations:
     *  ---
     *  cdrom  → "cdrom0:"
     *  mass   → "mass0:"
     *  hdd    → "hdd0:"
     *  host   → "host:"
     */

    if (locationToken[0] == 'c') // cdrom
        return s_cdRomResourceLocationToken;

    if (locationToken[0] == 'm' && locationToken[1] == 'a') // mass
        return s_massResourceLocationToken;

    if (locationToken[0] == 'h' && locationToken[1] == 'd') // hdd
        return s_hddResourceLocationToken;

    if (locationToken[0] == 'h' && locationToken[1] == 'o') // host
        return s_hostResourceLocationToken;

    char buff[LOG_STRING_MAX_SIZE] = {0};
    snprintf(buff, LOG_STRING_MAX_SIZE, "Invalid resource location! `%s`", locationToken);
    Engine_Panic(buff);

    return NULL; // This won't be hit.
}
