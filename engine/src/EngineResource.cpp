#include <cstdio>
#include <cstring>
#include "Engine.h"
#include "graphics/ModelFormat.h"
#include "graphics/Renderer.h"
#include "graphics/Types.h"
#include "graphics/tim2.h"

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

    // Engine-native resource storage (only one is active based on type).
    // Fonts and sound were dropped with raylib; those types are unsupported and
    // load attempts are logged and rejected.
    union
    {
        Texture2D texture;
        Model model;
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

static int32_t Internal_FindFreeSlot()
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

static bool Internal_ParseHeaderAndLoadDeps(const void* data, size_t size, AssetFileHeader* outHeader, int32_t entryIndex);

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

static int32_t Internal_EvictLRU()
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

static void Internal_UnloadHandle(ResourceEntry* entry)
{
    if (entry->state != RES_STATE_READY)
        return;

    switch (entry->type)
    {
    case RES_TEXTURE:
        {
            Renderer* r = Engine_GetRenderer();
            if (r && entry->handle.texture.id != 0)
                r->ReleaseTexture(entry->handle.texture.id);
        }
        break;
    case RES_MODEL:
        Model_FreeBaked(&entry->handle.model);
        break;
    case RES_SOUND:
    case RES_FONT:
        break; // unsupported — nothing was allocated
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
        if (depIdx >= 0 && depIdx < RES_MAX_ENTRIES && s_Entries[depIdx].state != RES_STATE_EMPTY && s_Entries[depIdx].generation == depGen)
        {
            if (s_Entries[depIdx].refCount > 0)
            {
                s_Entries[depIdx].refCount--;
            }
        }
    }

    Internal_UnloadHandle(entry);

    // Release shadow GS page accounting for textures
    if (entry->type == RES_TEXTURE && entry->gsPages > 0)
    {
        s_AllocatedGsPages = (s_AllocatedGsPages >= entry->gsPages) ? s_AllocatedGsPages - entry->gsPages : 0;
    }

    // Bump the generation BEFORE clearing the slot so any parent whose async
    // unload races with a new load into this slot will see the mismatch.
    uint16_t nextGeneration = static_cast<uint16_t>(entry->generation + 1u);

    // Clear the slot
    memset(entry, 0, sizeof(ResourceEntry));
    entry->state = RES_STATE_EMPTY;
    // Restore the incremented generation so future DepHandle bindings get the
    // new value and old stale bindings remain detectable.
    entry->generation = nextGeneration;
}

// Resolve a baked model's diffuse texture reference (an index into the owning
// asset's dependency list) to a resource handle. Passed to Model_LoadBaked; the
// returned handle is stored in the material and resolved to a live Texture2D at
// draw time (the texture dependency may still be streaming in).
static int32_t Internal_ResolveModelTexture(uint32_t diffuseTexRef, void* user)
{
    const ResourceEntry* entry = static_cast<const ResourceEntry*>(user);
    if (!entry || diffuseTexRef >= entry->depCount)
        return -1;
    return entry->deps[diffuseTexRef].index; // resource handle, or -1 if unbound
}

// Callback context for async IO loads
typedef struct
{
    int32_t entryIndex;
    ResourceType type;
} ResourceLoadContext;

static void Internal_OnAsyncLoadComplete(const void* data, size_t size, void* userData)
{
    ResourceLoadContext* ctx = static_cast<ResourceLoadContext*>(userData);
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

    const unsigned char* payload = static_cast<const unsigned char*>(data) + sizeof(AssetFileHeader);
    int32_t payloadSize = static_cast<int32_t>(size - sizeof(AssetFileHeader));

    if (payloadSize <= 0 || static_cast<uint32_t>(payloadSize) < header.dataSize)
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

    entry->type = static_cast<ResourceType>(header.type);

    switch (static_cast<ResourceType>(header.type))
    {
    case RES_TEXTURE:
        {
            // Textures are baked to TIM2 (GS-native). Parse the header, budget-check
            // GS VRAM, then hand the pixels to the active renderer for upload.
            Tim2Image img;
            if (!Tim2_Parse(payload, static_cast<size_t>(payloadSize), &img))
            {
                Engine_LogError("Resource: TIM2 parse failed for slot %d (%s)", idx, entry->key);
                Internal_UnloadEntry(idx);
                Engine_PoolFreeMain(ctx);
                return;
            }

            // GS page dimensions vary by pixel storage mode: 64x32 @ PSMCT32,
            // 64x64 @ PSMCT16, 128x64 @ PSMT8. Sum pages over all mip levels;
            // PAL8 adds one page for the CLUT.
            uint32_t pageW, pageH;
            switch (img.format)
            {
            case PixelFormat::RGBA16:
                pageW = 64u;
                pageH = 64u;
                break;
            case PixelFormat::PAL8:
                pageW = 128u;
                pageH = 64u;
                break;
            default:
                pageW = GFX_GS_PAGE_WIDTH_PSM32;
                pageH = GFX_GS_PAGE_HEIGHT_PSM32;
                break;
            }
            uint32_t pages = 0;
            for (uint8_t lvl = 0; lvl < img.mipCount; ++lvl)
            {
                const uint32_t w = (img.width >> lvl) ? static_cast<uint32_t>(img.width >> lvl) : 1u;
                const uint32_t h = (img.height >> lvl) ? static_cast<uint32_t>(img.height >> lvl) : 1u;
                pages += ((w + pageW - 1) / pageW) * ((h + pageH - 1) / pageH);
            }
            if (img.format == PixelFormat::PAL8)
                pages += 1u; // CLUT

            if (img.width > GFX_MAX_TEXTURE_WIDTH || img.height > GFX_MAX_TEXTURE_HEIGHT || pages > GFX_MAX_TEXTURE_GS_PAGES)
            {
                Engine_LogError("Resource: texture rejected — %dx%d (%u pages) exceeds budget "
                                "(max %u pages, dimension cap %dx%d) in slot %d '%s'",
                                img.width, img.height, pages, GFX_MAX_TEXTURE_GS_PAGES, GFX_MAX_TEXTURE_WIDTH, GFX_MAX_TEXTURE_HEIGHT, idx, entry->key);
                Internal_UnloadEntry(idx);
                Engine_PoolFreeMain(ctx);
                return;
            }

            if (s_AllocatedGsPages + pages > GFX_GS_TEXTURE_PAGE_BUDGET)
            {
                uint32_t evictablePages = 0;
                int32_t evictableCount = 0;
                Internal_CalcEvictablePages(&evictablePages, &evictableCount);
                Engine_LogError("Resource: GS VRAM full — cannot load %dx%d (%u pages). Usage: %u/%u pages. "
                                "Call Engine_Resource_Unload() to free up to %u pages across %d texture(s), then retry.",
                                img.width, img.height, pages, s_AllocatedGsPages, GFX_GS_TEXTURE_PAGE_BUDGET, evictablePages, evictableCount);
                Internal_UnloadEntry(idx);
                Engine_PoolFreeMain(ctx);
                return;
            }

            TextureUpload upload{};
            for (int lvl = 0; lvl < TEX_MAX_MIP_LEVELS; ++lvl)
                upload.levelPtr[lvl] = img.levelPtr[lvl];
            upload.mipCount = img.mipCount;
            upload.width = img.width;
            upload.height = img.height;
            upload.format = img.format;
            upload.clut = img.clut;

            Renderer* renderer = Engine_GetRenderer();
            const uint32_t texId = renderer ? renderer->UploadTexture(upload) : 0u;
            if (texId == 0)
            {
                // gsPages was not yet committed to the shadow counter, so no rollback needed.
                Engine_LogError("Resource: GPU texture upload failed for slot %d (%s)", idx, entry->key);
                Internal_UnloadEntry(idx);
                Engine_PoolFreeMain(ctx);
                return;
            }

            entry->handle.texture.id = texId;
            entry->handle.texture.width = img.width;
            entry->handle.texture.height = img.height;
            entry->handle.texture.format = static_cast<int>(img.format);
            entry->gsPages = pages;
            s_AllocatedGsPages += pages;
            entry->state = RES_STATE_READY;
            Engine_LogInfo("Texture loaded (%dx%d). Remaining pages: %u", img.width, img.height, static_cast<uint32_t>(GFX_GS_TEXTURE_PAGE_BUDGET) - s_AllocatedGsPages);
        }
        break;
    case RES_MODEL:
        {
            // Models are baked to separated, unindexed vertex arrays. Their texture
            // dependencies were queued by Internal_ParseHeaderAndLoadDeps above;
            // materials store the dep resource handles and are resolved to live
            // textures at draw time.
            if (!Model_LoadBaked(payload, static_cast<size_t>(payloadSize), &entry->handle.model, Internal_ResolveModelTexture, entry))
            {
                Engine_LogError("Resource: baked model load failed for slot %d (%s)", idx, entry->key);
                Internal_UnloadEntry(idx);
                Engine_PoolFreeMain(ctx);
                return;
            }
            entry->state = RES_STATE_READY;
        }
        break;
    case RES_SOUND:
    case RES_FONT:
        Engine_LogError("Resource: type %u unsupported (fonts/sound were dropped with raylib) for slot %d (%s)", header.type, idx, entry->key);
        Internal_UnloadEntry(idx);
        Engine_PoolFreeMain(ctx);
        return;
    default:
        Engine_LogError("Resource: unknown type %u for slot %d (%s)", header.type, idx, entry->key);
        Internal_UnloadEntry(idx);
        Engine_PoolFreeMain(ctx);
        return;
    }

    Engine_PoolFreeMain(ctx);
}


// Read just the magic and type fields (first 8 bytes) of a .ps2a asset without
// loading its full payload. Resolves through a mounted archive first, falling
// back to a loose file on disc. Returns false if the asset can't be read or the
// magic is wrong.
static bool Internal_PeekAssetType(const char* path, ResourceType* outType)
{
    uint32_t peek[2]; // [0] = magic, [1] = type

    // ARCHIVE SEAM (header peek): prefer a mounted archive.
    ArchiveLocator loc;
    if (Engine_Archive_Find(path, &loc))
    {
        if (!Engine_Archive_ReadSync(&loc, 0, peek, sizeof(peek)))
            return false;
    }
    else
    {
        // Loose fallback — take the file-access semaphore so this raw read never
        // races the IO worker's file access.
        Engine_IO_AcquireFileAccess();
        FILE* f = fopen(path, "rb");
        size_t bytesRead = 0;
        if (f)
        {
            bytesRead = fread(peek, sizeof(uint32_t), 2, f);
            fclose(f);
        }
        Engine_IO_ReleaseFileAccess();
        if (!f || bytesRead < 2)
            return false;
    }

    if (peek[0] != RES_ASSET_MAGIC)
        return false;

    *outType = static_cast<ResourceType>(peek[1]);
    return true;
}

// Parse a .ps2a header from raw file data and load dependencies.
// Returns true if the header is valid.
static bool Internal_ParseHeaderAndLoadDeps(const void* data, size_t size, AssetFileHeader* outHeader, int32_t entryIndex)
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
            entry->deps[d].index = static_cast<int16_t>(depHandle);
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
                entry->deps[d].index = static_cast<int16_t>(newDep);
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

bool Engine_Resource_Init()
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

void Engine_Resource_Shutdown() { Engine_Resource_UnloadAll(); }

int32_t Engine_Resource_Load(ResourceType type, const char* path)
{
    if (!path)
        return -1;

    // Canonicalise the path into the dedup/lookup key so the same asset requested
    // as a device path ("cdrom0:/RASSETS/BOX.PS2A;1") and as a baked dependency
    // string ("RASSETS/BOX.PS2A") map to one slot instead of two.
    char canonicalKey[IO_FILE_MAX_PATH];
    Engine_Path_Canonical(path, canonicalKey);

    // Check if already loaded or loading
    int32_t existing = Internal_FindByKey(canonicalKey);
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
    // canonicalKey is fully defined and null-terminated across all IO_FILE_MAX_PATH
    // bytes by Engine_Path_Canonical, so copy the whole buffer.
    memcpy(entry->key, canonicalKey, IO_FILE_MAX_PATH);

    for (uint8_t d = 0; d < RES_MAX_DEPENDENCIES; d++)
    {
        entry->deps[d].index = -1;
        entry->deps[d].generation = 0;
    }

    // All resource types stream through the async IO path. The header's declared
    // type drives decoding (TIM2 for textures, baked blob for models); the .ps2a
    // dependency list is loaded first so a model's textures are already queued.
    // Async path: allocate a context from the pool, then stream
    ResourceLoadContext* ctx = static_cast<ResourceLoadContext*>(Engine_PoolAllocMain());
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
        return nullptr;

    ResourceEntry* entry = &s_Entries[handle];
    if (entry->state != RES_STATE_READY)
        return nullptr;

    entry->lastUsedFrame = s_CurrentFrame;

    switch (entry->type)
    {
    case RES_TEXTURE:
        return &entry->handle.texture;
    case RES_MODEL:
        return &entry->handle.model;
    case RES_SOUND:
    case RES_FONT:
        return nullptr; // unsupported (dropped with raylib)
    }
    return nullptr;
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

void Engine_Resource_UnloadAll()
{
    s_AllocatedGsPages = 0;
    for (int32_t i = 0; i < RES_MAX_ENTRIES; i++)
    {
        if (s_Entries[i].state != RES_STATE_EMPTY)
        {
            Internal_UnloadHandle(&s_Entries[i]);
            memset(&s_Entries[i], 0, sizeof(ResourceEntry));
            s_Entries[i].state = RES_STATE_EMPTY;
        }
    }
}

void Engine_Resource_Update() { s_CurrentFrame++; }

uint32_t Engine_Resource_GetAllocatedGsPages() { return s_AllocatedGsPages; }
uint32_t Engine_Resource_GetGsPageBudget() { return GFX_GS_TEXTURE_PAGE_BUDGET; }
