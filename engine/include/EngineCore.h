#pragma once

#include "graphics/Renderer.h"

typedef struct
{
    const char* windowTitle;
    const char* resourceLocationToken;
} EngineConfig;

bool Engine_Init(EngineConfig config);
bool Engine_Is_GFX_Initialized();
Renderer* Engine_GetRenderer();

void Engine_Update();

void Engine_Close();
void Engine_Close(void);

const char* Engine_GetResourceLocationToken(void);

// Constructs a full filesystem path by combining the active resource location
// token with a relative path, inserting the correct separator and version
// suffix for the device type (e.g. cdrom0:\\FOLDER\\FILE;1, host:FILE).
// Returns false if any argument is NULL or bufSize is 0.
bool Engine_BuildPath(const char* token, const char* relativePath, char* outBuf, size_t bufSize);
