#pragma once

#include <cstddef>
#include <cstdint>
#include "graphics/Renderer.h"

// Renderer backend selection. Chosen at build time via a CMake compile define
// (-DRENDERER_BACKEND_PS2GL or -DRENDERER_BACKEND_GIFTAG). Default to the PS2GL
// backend if neither was supplied so a bare compile still links a renderer.
#if !defined(RENDERER_BACKEND_PS2GL) && !defined(RENDERER_BACKEND_GIFTAG)
    #define RENDERER_BACKEND_PS2GL 1
#endif

typedef struct
{
    const char* windowTitle;
    const char* resourceLocationToken;
    bool enablePerfLogger;
} EngineConfig;

bool Engine_Init(EngineConfig config);
bool Engine_Is_GFX_Initialized();
Renderer* Engine_GetRenderer();

void Engine_Update();

void Engine_Close();

const char* Engine_GetResourceLocationToken();

// Performance metrics
float Engine_GetDeltaTime();
float Engine_GetFPS();
float Engine_GetTotalTime();

// Frame profiling
void Engine_ReportFrameStats(float logicTime, float renderTime, float waitTime);
float Engine_GetLogicTime();
float Engine_GetRenderTime();
float Engine_GetWaitTime();

// Frame counter, incremented once per Engine_Update() call. Used by the
// heartbeat log and perf snapshot to report frame number / detect hangs.
uint32_t Engine_GetFrameCount();

// Constructs a full filesystem path by combining the active resource location
// token with a relative path, inserting the correct separator and version
// suffix for the device type (e.g. cdrom0:\\FOLDER\\FILE;1, host:FILE).
// Returns false if any argument is NULL or bufSize is 0.
bool Engine_BuildPath(const char* token, const char* relativePath, char* outBuf, size_t bufSize);

// Canonicalise an asset path into the stable key used for resource dedup and
// archive lookup: strips the device token (cdrom0:/mass0:/hdd0:/host:) and any
// ";N" version suffix, converts '\\' to '/', upper-cases, and drops leading
// slashes. e.g. "cdrom0:/RASSETS/BOX.PS2A;1" and the baked dependency string
// "RASSETS/BOX.PS2A" both canonicalise to "RASSETS/BOX.PS2A". out must hold at
// least IO_FILE_MAX_PATH bytes.
void Engine_Path_Canonical(const char* in, char* out);
