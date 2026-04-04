#ifndef ENGINE_LEVEL_H
#define ENGINE_LEVEL_H

#include <stdbool.h>
#include <stdint.h>

#include "Constants.h"

// Minimal level descriptor.
// Only stores the paths of required (pinned) resources.
// Kept as small as possible — no runtime resource handle tracking.
typedef struct {
  char name[64];
  char requiredResources[LEVEL_MAX_RESOURCES_COUNT][IO_FILE_MAX_PATH];
  uint32_t requiredCount;
} Level;

// Load a level: loads and pins all required resources.
// Returns true if all required resources were successfully queued/loaded.
bool Engine_Level_Load(Level *level);

// Unload a level: unpins required resources and resets ARENA_LEVEL_DATA.
// If keepPinned is true, required resources remain pinned (e.g. shared UI/fonts).
void Engine_Level_Unload(Level *level, bool keepPinned);

#endif // ENGINE_LEVEL_H

