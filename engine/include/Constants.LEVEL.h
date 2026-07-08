#pragma once

#define LEVEL_MAX_RESOURCES_COUNT 16
#define LEVEL_FILE_MAGIC 0x4C325350u /* "PS2L" in little-endian */
#define LEVEL_FILE_VERSION 2u /* v2: chunked, sectorized (see EngineLevelFormat.h) */
#define LEVEL_FILE_EXT ".ps2l"

// --- Sectorized level (v2) budgets ------------------------------------------

// Materials (textures) referenced by a level's geometry.
#define LEVEL_MAX_MATERIALS 64

// Meshes per streamed sector (one per material present in the sector's cell).
#define LEVEL_MAX_MESHES_PER_SECTOR 32

// A streamed sector payload (PSEC) must fit one ARENA_LEVEL_DATA slot; the
// compiler hard-fails any sector larger than this. 16KB-aligned slot capacity is
// MEM_BLOCK_LEVEL_DATA_SIZE / MEM_BLOCK_LEVEL_DATA_SLOTS; keep in sync.
#define LEVEL_SECTOR_MAX_BYTES (256 * 1024)

// GS page budget the compiler allows for a level's textures + far-field atlases,
// leaving headroom under GFX_GS_TEXTURE_PAGE_BUDGET for game/UI textures.
#define LEVEL_GS_PAGE_BUDGET 200

// Far-field impostor azimuth views baked per cluster (N/E/S/W). Data-driven at
// runtime via FarfieldHeader.azimuthCount; this is the compiler default.
#define LEVEL_FARFIELD_AZIMUTHS 4

// Max billboards drawn in a frame (far-field ring around the camera).
#define LEVEL_FARFIELD_MAX_DRAWN 256

// Sector residency: recenter the 3x3 ring only once the camera leaves the
// current cell by this fraction of a cell (hysteresis against boundary thrash).
#define LEVEL_SECTOR_HYSTERESIS 0.15f

// Max key/value properties read from one entity spawn record.
#define LEVEL_MAX_ENTITY_PROPS 32

// Resident sectors: a 3x3 ring around the camera cell.
#define LEVEL_RESIDENT_SECTORS 9

// ARENA_LEVEL_DATA slot assignment: slots [0..CORE_SLOTS) hold the resident
// level core; sectors stream into the slots after that.
#define LEVEL_CORE_SLOTS 2
#define LEVEL_SECTOR_SLOT_BASE LEVEL_CORE_SLOTS
