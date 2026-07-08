#pragma once

#include <cstdint>

#include "Constants.h"
#include "EngineLevel.h"
#include "graphics/Frustum.h"
#include "graphics/Types.h"

// ---------------------------------------------------------------------------
// Sector residency manager. Keeps a 3x3 ring of sector geometry resident in
// ARENA_LEVEL_DATA slots around the streaming centre, recentered on crossings.
// Each resident sector exposes Mesh views pointing straight into its arena slot
// (zero-copy into the renderer's existing mesh path).
//
// Phase D1 loads the ring synchronously; Phase D2 swaps in async streaming.
// ---------------------------------------------------------------------------

typedef enum
{
    SECTOR_EMPTY = 0,
    SECTOR_LOADING,
    SECTOR_READY
} SectorState;

typedef struct
{
    int16_t cellX; // grid cell, or -1 when unused
    int16_t cellZ;
    uint8_t arenaSlot; // ARENA_LEVEL_DATA slot holding this sector's PSEC blob
    uint8_t state; // SectorState
    uint32_t meshCount;
    Mesh meshes[LEVEL_MAX_MESHES_PER_SECTOR]; // views into the slot geometry
    int32_t meshTexture[LEVEL_MAX_MESHES_PER_SECTOR]; // resolved texture handles
    Aabb3 bounds; // world-space sector AABB (from the PSEC header)
} SectorResident;

// Begin/end a level's sector residency. Begin resets the ring; End frees slots.
bool Engine_Sector_Begin(const Level* level);
void Engine_Sector_End();

// Recenter the ring on a world-space streaming centre (with hysteresis).
void Engine_Sector_Update(float worldX, float worldZ);

// The resident sector array (fixed capacity). `outCount` receives the number of
// entries whose state != SECTOR_EMPTY need not be contiguous — iterate all and
// skip SECTOR_EMPTY.
const SectorResident* Engine_Sector_GetResidents(uint32_t* outCount);
