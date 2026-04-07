#ifndef ENGINE_RESOURCE_H
#define ENGINE_RESOURCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "Constants.h"

// Resource types matching Raylib handle categories
// NOTE: RES_SOUND requires SUPPORT_MODULE_RAUDIO to be defined in raylib's config.h.
//       On PS2 the raudio module is disabled — Engine_Resource_Load(RES_SOUND, ...) will
//       log an error and return -1 at runtime. Use the PS2 native audio subsystem instead.
typedef enum {
    RES_TEXTURE,
    RES_MODEL,
    RES_SOUND,
    RES_FONT
} ResourceType;

// Lifecycle state of a resource entry
typedef enum {
    RES_STATE_EMPTY,
    RES_STATE_LOADING,
    RES_STATE_READY
} ResourceState;

// .ps2a file header (binary, written by pack_assets.py, read at runtime)
// Total size: 4+4+1+3+16+(8*256)+4 = 2080 bytes
typedef struct {
    uint32_t magic; // RES_ASSET_MAGIC
    uint32_t type; // ResourceType
    uint8_t depCount; // Number of dependencies
    uint8_t reserved[3]; // Padding
    char ext[16]; // Source file extension e.g. ".jpg", ".png"
    char deps[RES_MAX_DEPENDENCIES][IO_FILE_MAX_PATH]; // Dependency asset paths
    uint32_t dataSize; // Size of payload after header
} AssetFileHeader;

// Initialize the resource manager (call once during Engine_Init)
bool Engine_Resource_Init(void);

// Shut down and release all resources
void Engine_Resource_Shutdown(void);

// Load a resource from a .ps2a file on disc.
// For RES_MODEL: loads synchronously via Raylib's LoadModel.
// For all others: streams via Engine_IO_ReadAsync, then decodes via *FromMemory.
// Returns a handle >= 0 on success, or -1 on failure.
int32_t Engine_Resource_Load(ResourceType type, const char *path);

// Retrieve a pointer to the underlying Raylib resource.
// Returns NULL if the resource is not yet ready or the handle is invalid.
// Caller must cast to the appropriate Raylib type (Texture2D*, Model*, etc.).
void *Engine_Resource_Get(int32_t handle);

// Check if a resource has finished loading.
bool Engine_Resource_IsReady(int32_t handle);

// Pin a resource so it is never auto-evicted by LRU.
void Engine_Resource_Pin(int32_t handle);

// Unpin a resource, making it eligible for LRU eviction.
void Engine_Resource_Unpin(int32_t handle);

// Explicitly unload a single resource and decrement refCounts on its deps.
void Engine_Resource_Unload(int32_t handle);

// Force-unload all resources (including pinned). Used during shutdown.
void Engine_Resource_UnloadAll(void);

// Called once per frame (from Engine_Update) to advance the current frame counter.
void Engine_Resource_Update(void);

// Returns the number of GS VRAM pages currently occupied by loaded textures.
// Compared against GFX_GS_TEXTURE_PAGE_BUDGET (264) to detect VRAM pressure.
// When over budget ps2gl performs silent LRU eviction — textures are re-uploaded
// from CPU RAM on next use at a performance cost.
uint32_t Engine_Resource_GetAllocatedGsPages(void);

// Returns the compile-time GS VRAM texture page budget (GFX_GS_TEXTURE_PAGE_BUDGET).
uint32_t Engine_Resource_GetGsPageBudget(void);

#endif // ENGINE_RESOURCE_H
