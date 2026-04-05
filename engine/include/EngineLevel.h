#ifndef ENGINE_LEVEL_H
#define ENGINE_LEVEL_H

#include <stdbool.h>
#include <stdint.h>

#include "Constants.h"

// On-disc header for .ps2l files.
// Must be read and validated before deserializing the Level struct.
typedef struct {
  uint32_t magic;      // Must equal LEVEL_FILE_MAGIC
  uint8_t  version;    // Must equal LEVEL_FILE_VERSION
  uint8_t  reserved[3];
} LevelFileHeader;

// Minimal level descriptor.
// Only stores the paths of required (pinned) resources.
// Packed to eliminate host/target padding divergence when serializing to .ps2l.
typedef struct __attribute__((packed)) {
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

