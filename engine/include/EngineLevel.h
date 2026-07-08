#pragma once

#include <cstdint>

#include "Constants.h"
#include "EngineLevelFormat.h"

// Runtime level descriptor. Filled by Engine_Level_Load from a compiled level
// archive (LEVELS/<name>.PS2R, produced by tools/compile_level.py). The chunk
// pointers are views into the resident level-core arena slot(s); the sector
// geometry streams separately (see EngineSector.h).
typedef struct
{
    char name[64]; // set by the caller; names the archive/core (e.g. "TEST")
    int32_t archiveHandle; // mounted level archive, or -1

    const LevelInfoChunk* info; // views into ARENA_LEVEL_DATA core slot
    const LevelMaterialEntry* materials;
    const LevelGridCell* grid;
    const uint8_t* entsChunk; // raw ENTS chunk (count/records/props/strings)
    const uint8_t* farfieldChunk; // raw FARF chunk, or null

    int32_t materialTex[LEVEL_MAX_MATERIALS]; // pinned texture resource handles
} Level;

// Load a compiled level: mount its archive, read the core into resident slots,
// pin material textures, spawn its entities via the game's spawn handler, and
// prime the resident sector ring. Blocking (call from a load screen).
// `level->name` must be set; other fields are filled in. Returns false on error.
bool Engine_Level_Load(Level* level);

// Unload: release the sector ring, unpin textures (unless keepPinned), unmount
// the archive, and clear the level-data arena.
void Engine_Level_Unload(Level* level, bool keepPinned);

// Update the streaming centre (world X/Z). Recenters the resident sector ring
// with hysteresis. Call each frame with the camera/player position.
void Engine_Level_SetStreamingCenter(float worldX, float worldZ);

// The currently loaded level, or null. Used by the renderer to draw sectors.
const Level* Engine_Level_Current();
