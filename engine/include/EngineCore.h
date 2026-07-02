#pragma once

#include "graphics/Renderer.h"
#include <cstddef>

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

// Constructs a full filesystem path by combining the active resource location
// token with a relative path, inserting the correct separator and version
// suffix for the device type (e.g. cdrom0:\\FOLDER\\FILE;1, host:FILE).
// Returns false if any argument is NULL or bufSize is 0.
bool Engine_BuildPath(const char* token, const char* relativePath, char* outBuf, size_t bufSize);
