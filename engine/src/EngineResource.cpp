#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Engine.h"

// Stable per-dependency reference: packs a slot index and a generation counter
// into 4 bytes (same width as the old int32_t). The generation must match the
// target slot's generation at unload time; a mismatch means the slot was reused
// for a different resource, so the decrement is skipped.
typedef struct
{
    int16_t index; // slot index, or -1 for "none"
    uint16_t generation; // slot generation when this dep was bound
} DepHandle;

// Internal resource entry — holds a Raylib resource handle + metadata
typedef struct
{
    ResourceType type;
    ResourceState state;
    uint32_t refCount;
    uint32_t lastUsedFrame;
    uint32_t gsPages; // GS VRAM pages consumed (RES_TEXTURE only; 0 otherwise)
    char key[IO_FILE_MAX_PATH];
    bool pinned;
    // Generation counter — incremented every time this slot is cleared.
    // DepHandle.generation is compared against this value on unload to detect
    // stale references caused by slot reuse.
    uint16_t generation;
    DepHandle deps[RES_MAX_DEPENDENCIES];
    uint8_t depCount;

    // Raylib resource storage (only one is active based on type)
    union
    {
        Texture2D texture;
        Model model;
#if defined(SUPPORT_MODULE_RAUDIO)
        Sound sound;
#endif
        Font font;
    } handle;
} ResourceEntry;

static ResourceEntry s_Entries[RES_MAX_ENTRIES];
static uint32_t s_CurrentFrame = 0;
// Shadow counter: total GS VRAM pages occupied by all currently READY textures.
// ps2gl has no public query API for this; we maintain it ourselves.
// When the budget is exceeded ps2gl silently LRU-evicts the oldest texture slot
// from GS VRAM — the texture object stays in CPU RAM and is re-uploaded on demand.
static uint32_t s_AllocatedGsPages = 0;

// --- Internal helpers ---

static int32_t Internal_FindByKey(const char* key)
{
    for (int32_t i = 0; i < RES_MAX_ENTRIES; i++)
    {
        if (s_Entries[i].state != RES_STATE_EMPTY && strncmp(s_Entries[i].key, key, IO_FILE_MAX_PATH) == 0)
        {
            return i;
        }
    }
    return -1;
}

static int32_t Internal_FindFreeSlot(void)
{
    for (int32_t i = 0; i < RES_MAX_ENTRIES; i++)
    {
        if (s_Entries[i].state == RES_STATE_EMPTY)
        {
            return i;
        }
    }
    return -1;
}

static void Internal_UnloadEntry(int32_t index);

static bool Internal_ParseHeaderAndLoadDeps(const void* data, size_t size, AssetFileHeader* outHeader,
                                            int32_t entryIndex);

// Sum the GS pages that COULD be freed right now (non-pinned, reference-free,
// READY textures). Used only to produce an actionable error message when the
// budget is exceeded — does NOT perform any eviction.
static void Internal_CalcEvictablePages(uint32_t* outPages, int32_t* outCount)
{
    *outPages = 0;
    *outCount = 0;
    for (int32_t i = 0; i < RES_MAX_ENTRIES; i++)
    {
        if (s_Entries[i].state != RES_STATE_READY)
            continue;
        if (s_Entries[i].type != RES_TEXTURE)
            continue;
        if (s_Entries[i].pinned)
            continue;
        if (s_Entries[i].refCount > 0)
            continue;
        *outPages += s_Entries[i].gsPages;
        (*outCount)++;
    }
}

static int32_t Internal_EvictLRU(void)
{
    int32_t bestIndex = -1;
    uint32_t bestFrame = UINT32_MAX;

    for (int32_t i = 0; i < RES_MAX_ENTRIES; i++)
    {
        if (s_Entries[i].state == RES_STATE_EMPTY || s_Entries[i].state == RES_STATE_LOADING)
            continue;
        if (s_Entries[i].pinned)
            continue;
        if (s_Entries[i].refCount > 0)
            continue;
        if (s_Entries[i].lastUsedFrame < bestFrame)
        {
            bestFrame = s_Entries[i].lastUsedFrame;
            bestIndex = i;
        }
    }

    if (bestIndex >= 0)
    {
        Engine_LogInfo("Resource LRU evict: slot %d (%s)", bestIndex, s_Entries[bestIndex].key);
        Internal_UnloadEntry(bestIndex);
    }

    return bestIndex;
}

static void Internal_UnloadRaylibHandle(const ResourceEntry* entry)
{
    if (entry->state != RES_STATE_READY)
        return;

    switch (entry->type)
    {
    case RES_TEXTURE:
        UnloadTexture(entry->handle.texture);
        break;
    case RES_MODEL:
        UnloadModel(entry->handle.model);
        break;
    case RES_SOUND:
#if defined(SUPPORT_MODULE_RAUDIO)
        UnloadSound(entry->handle.sound);
#endif
        break;
    case RES_FONT:
        UnloadFont(entry->handle.font);
        break;
    }
}

static void Internal_UnloadEntry(int32_t index)
{
    if (index < 0 || index >= RES_MAX_ENTRIES)
        return;

    ResourceEntry* entry = &s_Entries[index];
    if (entry->state == RES_STATE_EMPTY)
        return;

    // Decrement refCount on all dependencies.
    // Validate the generation before touching the slot — if the dep was already
    // unloaded and its slot reused for a different resource, the generation will
    // have advanced and we must NOT decrement the new resource's refCount.
    for (uint8_t d = 0; d < entry->depCount; d++)
    {
        int16_t depIdx = entry->deps[d].index;
        uint16_t depGen = entry->deps[d].generation;
        if (depIdx >= 0 && depIdx < RES_MAX_ENTRIES && s_Entries[depIdx].state != RES_STATE_EMPTY &&
            s_Entries[depIdx].generation == depGen)
        {
            if (s_Entries[depIdx].refCount > 0)
            {
                s_Entries[depIdx].refCount--;
            }
        }
    }

    Internal_UnloadRaylibHandle(entry);

    // Release shadow GS page accounting for textures
    if (entry->type == RES_TEXTURE && entry->gsPages > 0)
    {
        s_AllocatedGsPages = (s_AllocatedGsPages >= entry->gsPages) ? s_AllocatedGsPages - entry->gsPages : 0;
    }

    // Bump the generation BEFORE clearing the slot so any parent whose async
    // unload races with a new load into this slot will see the mismatch.
    uint16_t nextGeneration = (uint16_t)(entry->generation + 1u);

    // Clear the slot
    memset(entry, 0, sizeof(ResourceEntry));
    entry->state = RES_STATE_EMPTY;
    // Restore the incremented generation so future DepHandle bindings get the
    // new value and old stale bindings remain detectable.
    entry->generation = nextGeneration;
}

// Callback context for async IO loads
typedef struct
{
    int32_t entryIndex;
    ResourceType type;
} ResourceLoadContext;

static void Internal_OnAsyncLoadComplete(const void* data, size_t size, void* userData)
{
    ResourceLoadContext* ctx = (ResourceLoadContext*)userData;
    if (!ctx)
        return;

    int32_t idx = ctx->entryIndex;
    ResourceEntry* entry = &s_Entries[idx];

    if (!data || size == 0)
    {
        Engine_LogError("Resource async load failed for slot %d (%s)", idx, entry->key);
        // Unload the entry to clear metadata, decrement any partial dep refs, and
        // bump generation so stale DepHandles won't point to a future occupant.
        Internal_UnloadEntry(idx);
        Engine_PoolFreeMain(ctx);
        return;
    }

    // Parse the .ps2a header and load any declared dependencies
    AssetFileHeader header;
    if (!Internal_ParseHeaderAndLoadDeps(data, size, &header, idx))
    {
        // Internal_ParseHeaderAndLoadDeps only returns false before the dep-loading
        // loop, so no refCounts can have been bumped yet. Internal_UnloadEntry is
        // still used for a consistent cleanup path.
        Internal_UnloadEntry(idx);
        Engine_PoolFreeMain(ctx);
        return;
    }

    const unsigned char* payload = (const unsigned char*)data + sizeof(AssetFileHeader);
    int32_t payloadSize = (int32_t)(size - sizeof(AssetFileHeader));

    if (payloadSize <= 0 || (uint32_t)payloadSize < header.dataSize)
    {
        Engine_LogError("Resource payload mismatch for slot %d (%s)", idx, entry->key);
        // Deps were already loaded and their refCounts bumped; roll back via
        // Internal_UnloadEntry so they are properly decremented.
        Internal_UnloadEntry(idx);
        Engine_PoolFreeMain(ctx);
        return;
    }

    // Use the type declared in the .ps2a header as the authoritative decode type.
    // ctx->type is the caller-supplied hint; the header's type is what the asset
    // packer stamped and should always be preferred. Update entry->type so that
    // later unload / get calls use the correct Raylib handle union member.

    // Validate header.type is within the supported ResourceType enum range.
    // A corrupt asset or out-of-date packer can produce invalid type values.
    if (header.type >= 4) // RES_TEXTURE=0, RES_MODEL=1, RES_SOUND=2, RES_FONT=3
    {
        Engine_LogError("Resource: invalid type %u in .ps2a header for slot %d (%s)", header.type, idx, entry->key);
        Internal_UnloadEntry(idx);
        Engine_PoolFreeMain(ctx);
        return;
    }

    entry->type = (ResourceType)header.type;

    switch ((ResourceType)header.type)
    {
    case RES_TEXTURE:
        {
            Image img = LoadImageFromMemory(header.ext, payload, (int)header.dataSize);
            if (img.data != NULL)
            {
                // Hard-reject textures that exceed the GS VRAM slot budget.
                // Raylib registers slots up to 64 pages (512×256 at PSM32); anything larger
                // has no valid slot and LoadTextureFromImage would silently return id=0
                // while aliasing GS VRAM (TBP field is 14-bit, page 512 wraps to page 0).
                uint32_t pagesW = ((uint32_t)img.width + GFX_GS_PAGE_WIDTH_PSM32 - 1) / GFX_GS_PAGE_WIDTH_PSM32;
                uint32_t pagesH = ((uint32_t)img.height + GFX_GS_PAGE_HEIGHT_PSM32 - 1) / GFX_GS_PAGE_HEIGHT_PSM32;
                uint32_t pages = pagesW * pagesH;
                if (img.width > GFX_MAX_TEXTURE_WIDTH || img.height > GFX_MAX_TEXTURE_HEIGHT ||
                    pages > GFX_MAX_TEXTURE_GS_PAGES)
                {
                    Engine_LogError("Resource: texture rejected — %dx%d (%u pages) exceeds budget "
                                    "(max %u pages, dimension cap %dx%d) in slot %d '%s'",
                                    img.width, img.height, pages, GFX_MAX_TEXTURE_GS_PAGES, GFX_MAX_TEXTURE_WIDTH,
                                    GFX_MAX_TEXTURE_HEIGHT, idx, entry->key);
                    UnloadImage(img);
                    Internal_UnloadEntry(idx);
                    Engine_PoolFreeMain(ctx);
                    return;
                }

                // Hard-fail when the cumulative GS VRAM budget is exceeded.
                // GS VRAM management is the programmer's responsibility — the engine will
                // never silently evict a texture to make room (that would hide bugs).
                // Call Engine_Resource_Unload() on textures that are no longer needed,
                // then retry.
                if (s_AllocatedGsPages + pages > GFX_GS_TEXTURE_PAGE_BUDGET)
                {
                    uint32_t evictablePages = 0;
                    int32_t evictableCount = 0;
                    Internal_CalcEvictablePages(&evictablePages, &evictableCount);
                    Engine_LogError("Resource: GS VRAM full — cannot load %dx%d (%u pages). "
                                    "Usage: %u/%u pages. "
                                    "Call Engine_Resource_Unload() to free up to %u pages "
                                    "across %d unloaded texture(s), then retry.",
                                    img.width, img.height, pages, s_AllocatedGsPages, GFX_GS_TEXTURE_PAGE_BUDGET,
                                    evictablePages, evictableCount);
                    UnloadImage(img);
                    Internal_UnloadEntry(idx);
                    Engine_PoolFreeMain(ctx);
                    return;
                }

                entry->gsPages = pages;
                entry->handle.texture = LoadTextureFromImage(img);
                UnloadImage(img);
                // Guard against silent GPU upload failures (texture.id == 0 means
                // the hardware rejected the upload — treat as a decode failure).
                if (entry->handle.texture.id > 0)
                {
                    s_AllocatedGsPages += pages;
                    Engine_LogInfo("Texture loaded successfully. Remaining pages: %u",
                                   (uint32_t)GFX_GS_TEXTURE_PAGE_BUDGET - s_AllocatedGsPages);
                    entry->state = RES_STATE_READY;
                }
                else
                {
                    // entry->gsPages was written but s_AllocatedGsPages was NOT yet
                    // incremented. Zero gsPages before Internal_UnloadEntry to prevent
                    // the shadow counter from being incorrectly decremented.
                    entry->gsPages = 0;
                    Engine_LogError("Resource: GPU texture upload failed for slot %d (%s)", idx, entry->key);
                    Internal_UnloadEntry(idx);
                    Engine_PoolFreeMain(ctx);
                    return;
                }
            }
            else
            {
                Engine_LogError("Resource: failed to decode texture slot %d (%s)", idx, entry->key);
                Internal_UnloadEntry(idx);
                Engine_PoolFreeMain(ctx);
                return;
            }
        }
        break;
    case RES_SOUND:
#if defined(SUPPORT_MODULE_RAUDIO)
        {
            Wave wave = LoadWaveFromMemory(header.ext, payload, (int)header.dataSize);
            if (wave.data == NULL)
            {
                Engine_LogError("Resource: failed to decode sound slot %d (%s)", idx, entry->key);
                Internal_UnloadEntry(idx);
                Engine_PoolFreeMain(ctx);
                return;
            }
            entry->handle.sound = LoadSoundFromWave(wave);
            UnloadWave(wave);
            entry->state = RES_STATE_READY;
        }
        break;
#else
        Engine_LogError("Resource: RES_SOUND not supported (raudio module disabled) for slot %d", idx);
        Internal_UnloadEntry(idx);
        Engine_PoolFreeMain(ctx);
        return;
#endif
    case RES_FONT:
        {
            entry->handle.font = LoadFontFromMemory(header.ext, payload, (int)header.dataSize, 32, NULL, 0);
            if (entry->handle.font.texture.id > 0)
            {
                entry->state = RES_STATE_READY;
            }
            else
            {
                Engine_LogError("Resource: failed to decode font slot %d (%s)", idx, entry->key);
                Internal_UnloadEntry(idx);
                Engine_PoolFreeMain(ctx);
                return;
            }
        }
        break;
    case RES_MODEL:
        // Models should never arrive here — they are loaded synchronously
        Engine_LogError("Resource: unexpected async model load for slot %d", idx);
        Internal_UnloadEntry(idx);
        break;
    default:
        // Should never reach here if validation above is correct, but defensive
        Engine_LogError("Resource: unknown type %u for slot %d (%s)", header.type, idx, entry->key);
        Internal_UnloadEntry(idx);
        break;
    }

    Engine_PoolFreeMain(ctx);
}


// Read just the magic and type fields from a .ps2a file on disc without
// loading its full payload. Returns false if the file can't be opened or the
// magic is wrong.
static bool Internal_PeekAssetType(const char* path, ResourceType* outType)
{
    FILE* f = fopen(path, "rb");
    if (!f)
        return false;

    uint32_t peek[2]; // [0] = magic, [1] = type
    size_t bytesRead = fread(peek, sizeof(uint32_t), 2, f);
    fclose(f);

    if (bytesRead < 2 || peek[0] != RES_ASSET_MAGIC)
        return false;

    *outType = (ResourceType)peek[1];
    return true;
}

// Parse a .ps2a header from raw file data and load dependencies.
// Returns true if the header is valid.
static bool Internal_ParseHeaderAndLoadDeps(const void* data, size_t size, AssetFileHeader* outHeader,
                                            int32_t entryIndex)
{
    if (size < sizeof(AssetFileHeader))
        return false;

    memcpy(outHeader, data, sizeof(AssetFileHeader));

    // Force null-termination of header strings to guard against malformed/corrupt
    // assets. Without this, missing terminators can cause LoadImageFromMemory and
    // dependency lookups to read past the header into the payload or other memory.
    outHeader->ext[sizeof(outHeader->ext) - 1] = '\0';
    for (uint8_t d = 0; d < RES_MAX_DEPENDENCIES; d++)
    {
        outHeader->deps[d][IO_FILE_MAX_PATH - 1] = '\0';
    }

    if (outHeader->magic != RES_ASSET_MAGIC)
    {
        Engine_LogError("Resource: invalid .ps2a magic for slot %d", entryIndex);
        return false;
    }

    // Load each dependency (incrementing refCount if already loaded)
    ResourceEntry* entry = &s_Entries[entryIndex];
    entry->depCount = outHeader->depCount;
    if (entry->depCount > RES_MAX_DEPENDENCIES)
        entry->depCount = RES_MAX_DEPENDENCIES;

    for (uint8_t d = 0; d < entry->depCount; d++)
    {
        int32_t depHandle = Internal_FindByKey(outHeader->deps[d]);
        if (depHandle >= 0)
        {
            // Already loaded — just bump refCount
            s_Entries[depHandle].refCount++;
            entry->deps[d].index = (int16_t)depHandle;
            entry->deps[d].generation = s_Entries[depHandle].generation;
        }
        else
        {
            // Need to load the dependency first.
            // The dependency is its own .ps2a file with its own type field — peek
            // just its header to get the correct type rather than inheriting the
            // parent's type, which may be different.
            ResourceType depType;
            if (!Internal_PeekAssetType(outHeader->deps[d], &depType))
            {
                Engine_LogError("Resource: cannot determine type for dependency '%s'", outHeader->deps[d]);
                entry->deps[d].index = -1;
                entry->deps[d].generation = 0;
                continue;
            }
            int32_t newDep = Engine_Resource_Load(depType, outHeader->deps[d]);
            if (newDep >= 0)
            {
                s_Entries[newDep].refCount++;
                entry->deps[d].index = (int16_t)newDep;
                entry->deps[d].generation = s_Entries[newDep].generation;
            }
            else
            {
                Engine_LogError("Resource: failed to load dependency '%s'", outHeader->deps[d]);
                entry->deps[d].index = -1;
                entry->deps[d].generation = 0;
            }
        }
    }

    return true;
}

// --- Public API ---

bool Engine_Resource_Init(void)
{
    memset(s_Entries, 0, sizeof(s_Entries));
    for (int32_t i = 0; i < RES_MAX_ENTRIES; i++)
    {
        s_Entries[i].state = RES_STATE_EMPTY;
    }
    s_CurrentFrame = 0;
    s_AllocatedGsPages = 0;
    Engine_LogInfo("Resource Manager Initialized (%d slots)", RES_MAX_ENTRIES);
    return true;
}

void Engine_Resource_Shutdown(void) { Engine_Resource_UnloadAll(); }

int32_t Engine_Resource_Load(ResourceType type, const char* path)
{
    if (!path)
        return -1;

    // Check if already loaded or loading
    int32_t existing = Internal_FindByKey(path);
    if (existing >= 0)
    {
        s_Entries[existing].lastUsedFrame = s_CurrentFrame;
        return existing;
    }

    // Find a free slot, evict if necessary
    int32_t slot = Internal_FindFreeSlot();
    if (slot < 0)
    {
        slot = Internal_EvictLRU();
    }
    if (slot < 0)
    {
        Engine_LogError("Resource: no free slots and nothing evictable for '%s'", path);
        return -1;
    }

    // Initialize the entry
    ResourceEntry* entry = &s_Entries[slot];
    // The generation must survive the memset: it was already incremented by
    // Internal_UnloadEntry (eviction path) or holds the boot-time 0 (fresh slot).
    // Saving it here and restoring it below keeps the counter monotonic.
    uint16_t savedGeneration = entry->generation;
    memset(entry, 0, sizeof(ResourceEntry));
    entry->generation = savedGeneration;
    entry->type = type;
    entry->state = RES_STATE_LOADING;
    entry->lastUsedFrame = s_CurrentFrame;
    entry->pinned = false;
    entry->refCount = 0;
    entry->depCount = 0;
    strncpy(entry->key, path, IO_FILE_MAX_PATH - 1);
    entry->key[IO_FILE_MAX_PATH - 1] = '\0';

    for (uint8_t d = 0; d < RES_MAX_DEPENDENCIES; d++)
    {
        entry->deps[d].index = -1;
        entry->deps[d].generation = 0;
    }

    // Synchronous path for models (no FromMemory variant in Raylib)
    if (type == RES_MODEL)
    {
        entry->handle.model = LoadModel(path);
        if (entry->handle.model.meshCount > 0)
        {
            entry->state = RES_STATE_READY;
        }
        else
        {
            Engine_LogError("Resource: LoadModel failed for '%s'", path);
            entry->state = RES_STATE_EMPTY;
            return -1;
        }
        return slot;
    }

    // Async path: allocate a context from the pool, then stream
    ResourceLoadContext* ctx = (ResourceLoadContext*)Engine_PoolAllocMain();
    if (!ctx)
    {
        Engine_LogError("Resource: pool exhausted, cannot create load context");
        entry->state = RES_STATE_EMPTY;
        return -1;
    }

    ctx->entryIndex = slot;
    ctx->type = type;


    if (!Engine_IO_ReadAsync(path, Internal_OnAsyncLoadComplete, ctx))
    {
        Engine_LogError("Resource: IO queue full for '%s'", path);
        Engine_PoolFreeMain(ctx);
        entry->state = RES_STATE_EMPTY;
        return -1;
    }

    return slot;
}

int32_t Engine_Resource_LoadAuto(const char* path)
{
    if (!path)
        return -1;

    ResourceType type;
    if (!Internal_PeekAssetType(path, &type))
    {
        Engine_LogError("Resource: cannot determine type for '%s' — bad magic or unreadable", path);
        return -1;
    }

    return Engine_Resource_Load(type, path);
}

void* Engine_Resource_Get(int32_t handle)
{
    if (handle < 0 || handle >= RES_MAX_ENTRIES)
        return NULL;

    ResourceEntry* entry = &s_Entries[handle];
    if (entry->state != RES_STATE_READY)
        return NULL;

    entry->lastUsedFrame = s_CurrentFrame;

    switch (entry->type)
    {
    case RES_TEXTURE:
        return &entry->handle.texture;
    case RES_MODEL:
        return &entry->handle.model;
    case RES_SOUND:
#if defined(SUPPORT_MODULE_RAUDIO)
        return &entry->handle.sound;
#else
        return NULL;
#endif
    case RES_FONT:
        return &entry->handle.font;
    }
    return NULL;
}

bool Engine_Resource_IsReady(int32_t handle)
{
    if (handle < 0 || handle >= RES_MAX_ENTRIES)
        return false;
    return s_Entries[handle].state == RES_STATE_READY;
}

void Engine_Resource_Pin(int32_t handle)
{
    if (handle < 0 || handle >= RES_MAX_ENTRIES)
        return;
    s_Entries[handle].pinned = true;
}

void Engine_Resource_Unpin(int32_t handle)
{
    if (handle < 0 || handle >= RES_MAX_ENTRIES)
        return;
    s_Entries[handle].pinned = false;
}

void Engine_Resource_Unload(int32_t handle)
{
    if (handle < 0 || handle >= RES_MAX_ENTRIES)
        return;
    Internal_UnloadEntry(handle);
}

void Engine_Resource_UnloadAll(void)
{
    s_AllocatedGsPages = 0;
    for (int32_t i = 0; i < RES_MAX_ENTRIES; i++)
    {
        if (s_Entries[i].state != RES_STATE_EMPTY)
        {
            Internal_UnloadRaylibHandle(&s_Entries[i]);
            memset(&s_Entries[i], 0, sizeof(ResourceEntry));
            s_Entries[i].state = RES_STATE_EMPTY;
        }
    }
}

void Engine_Resource_Update(void) { s_CurrentFrame++; }

uint32_t Engine_Resource_GetAllocatedGsPages(void) { return s_AllocatedGsPages; }
uint32_t Engine_Resource_GetGsPageBudget(void) { return GFX_GS_TEXTURE_PAGE_BUDGET; }
