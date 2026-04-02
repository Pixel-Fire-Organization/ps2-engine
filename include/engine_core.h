#ifndef ENGINE_CORE_H
#define ENGINE_CORE_H

#include <stdbool.h>
#include <stddef.h>

#include "engine_memory.h"

typedef struct {
    int screenWidth;
    int screenHeight;
    const char* windowTitle;
    size_t memoryPoolSize;
    EngineMemoryMap memoryMap;
} EngineConfig;

bool Engine_Init(EngineConfig config);
void Engine_Update(void);
void Engine_Close(void);

#endif // ENGINE_CORE_H
