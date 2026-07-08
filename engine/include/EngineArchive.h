#pragma once

#include <cstdint>

#include "Constants.h"

// ---------------------------------------------------------------------------
// Game archives (.PS2R). See Constants.ARCH.h for the rationale.
//
// This is the runtime READ side. The producer is tools/pack_archive.py; both
// sides agree on the canonical asset key (Engine_Path_Canonical) and the FNV-1a
// name hash, so a lookup path in any form (device path, baked dependency path)
// resolves to the same TOC entry.
// ---------------------------------------------------------------------------

// --- On-disc format (little-endian) ---

typedef struct
{
    uint32_t magic; // ARCH_FILE_MAGIC
    uint16_t version; // ARCH_FILE_VERSION
    uint16_t flags; // reserved, 0
    uint32_t entryCount;
    uint32_t stringsOffset; // byte offset of the string table
    uint32_t stringsSize; // string table size in bytes
    uint32_t dataOffset; // first payload byte (multiple of ARCH_SECTOR_ALIGN)
    uint32_t reserved[2];
} ArchiveFileHeader; // 32 bytes

typedef struct
{
    uint32_t nameHash; // FNV-1a 32 of the canonical key
    uint32_t nameOffset; // offset into the string table; hash hits confirmed by strcmp
    uint32_t offset; // absolute byte offset of the payload (ARCH_SECTOR_ALIGN-aligned)
    uint32_t size; // payload size in bytes
} ArchiveTocEntry; // 16 bytes

// Where a resolved asset lives: a mounted slot + its byte span in that archive.
typedef struct
{
    int32_t archive; // mounted slot index, or -1 = not found
    uint32_t offset;
    uint32_t size;
} ArchiveLocator;

// --- Runtime API ---

bool Engine_Archive_Init();

// Mount a .PS2R by disc path (build it with Engine_BuildPath). Reads the header,
// TOC, and string table into heap memory and keeps the FILE* open for streaming.
// Blocking — call from a load screen, not mid-frame. Returns a mount handle
// (>= 0) or -1 on failure (missing file / bad magic / no free slot).
int32_t Engine_Archive_Mount(const char* discPath);

// Unmount: close the FILE* and free the TOC. This is the fast level-switch drop.
void Engine_Archive_Unmount(int32_t handle);

// Resolve an asset path (any form) to a span in a mounted archive. Returns true
// and fills outLoc on a hit. Later mount slots are searched first so a level
// asset shadows a boot asset of the same canonical key.
bool Engine_Archive_Find(const char* assetPath, ArchiveLocator* outLoc);

// Read `bytes` from a located span at span-relative `spanOffset` into dst.
// Bounds-checked against the span. Takes the IO file-access semaphore, so it is
// safe to call from the main thread while the IO worker may also touch a file.
bool Engine_Archive_ReadSync(const ArchiveLocator* loc, uint32_t spanOffset, void* dst, uint32_t bytes);

void Engine_Archive_Shutdown();
