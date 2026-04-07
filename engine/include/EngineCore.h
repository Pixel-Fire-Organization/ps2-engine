#ifndef ENGINE_CORE_H
#define ENGINE_CORE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    const char *windowTitle;
} EngineConfig;

bool Engine_Init(EngineConfig config);

bool Engine_Is_GFX_Initialized();

void Engine_Update(void);

void Engine_Close(void);

#endif // ENGINE_CORE_H
