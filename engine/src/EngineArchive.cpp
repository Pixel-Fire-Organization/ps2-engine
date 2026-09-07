#include <cstring>
#include <utility>

#include "Engine.h"
#include "EngineMemory.h"
#include "platform/Platform.h"

typedef struct
{
    FileHandle file;
    PlatformArray<ArchiveTocEntry> toc; // [entryCount], released with the mount
    PlatformArray<char> strings; // string table [stringsSize]
    uint32_t entryCount;
    bool inUse;
    char path[IO_FILE_MAX_PATH];
} MountedArchive;

static MountedArchive s_Mounts[ARCH_MAX_MOUNTED];

// FNV-1a 32. Must match tools/pack_archive.py exactly.
static uint32_t Internal_Fnv1a32(const char* s)
{
    uint32_t h = 2166136261u;
    for (; *s; ++s)
    {
        h ^= static_cast<uint8_t>(*s);
        h *= 16777619u;
    }
    return h;
}

bool Engine_Archive_Init()
{
    // Not memset: a mount owns its TOC, and overwriting the object
    // representation would leak it and corrupt the handle.
    for (int32_t i = 0; i < ARCH_MAX_MOUNTED; ++i)
        Engine_Archive_Unmount(i);
    return true;
}

int32_t Engine_Archive_Mount(const char* discPath)
{
    if (!discPath)
        return -1;

    int32_t slot = -1;
    for (int32_t i = 0; i < ARCH_MAX_MOUNTED; ++i)
    {
        if (!s_Mounts[i].inUse)
        {
            slot = i;
            break;
        }
    }
    if (slot < 0)
    {
        Engine_LogError("Archive: no free mount slot for '%s' (max %d)", discPath, ARCH_MAX_MOUNTED);
        return -1;
    }

    Platform* platform = Engine_GetPlatform();

    Engine_IO_AcquireFileAccess();
    FileHandle f = platform->FileOpen(discPath, FileMode::Read);
    if (!f)
    {
        Engine_IO_ReleaseFileAccess();
        Engine_LogError("Archive: cannot open '%s'", discPath);
        return -1;
    }

    ArchiveFileHeader header;
    bool ok = (platform->FileRead(f, &header, sizeof(header)) == sizeof(header));
    PlatformArray<ArchiveTocEntry> toc;
    PlatformArray<char> strings;

    if (ok && (header.magic != ARCH_FILE_MAGIC || header.version != ARCH_FILE_VERSION))
    {
        Engine_LogError("Archive: bad magic/version in '%s'", discPath);
        ok = false;
    }

    if (ok && header.entryCount > 0)
    {
        const size_t tocBytes = static_cast<size_t>(header.entryCount) * sizeof(ArchiveTocEntry);
        toc = Engine_PlatformArray<ArchiveTocEntry>(header.entryCount);
        strings = Engine_PlatformArray<char>(header.stringsSize ? header.stringsSize : 1);
        if (!toc || !strings)
        {
            Engine_LogError("Archive: out of memory mounting '%s'", discPath);
            ok = false;
        }
        // TOC immediately follows the header; string table is at stringsOffset.
        if (ok && !platform->FileSeek(f, sizeof(ArchiveFileHeader)))
            ok = false;
        if (ok && platform->FileRead(f, toc.get(), tocBytes) != tocBytes)
            ok = false;
        if (ok && header.stringsSize > 0)
        {
            if (!platform->FileSeek(f, header.stringsOffset))
                ok = false;
            else if (platform->FileRead(f, strings.get(), header.stringsSize) != header.stringsSize)
                ok = false;
        }
    }
    Engine_IO_ReleaseFileAccess();

    if (!ok)
    {
        platform->FileClose(f);
        return -1;
    }

    s_Mounts[slot].file = f;
    s_Mounts[slot].toc = std::move(toc);
    s_Mounts[slot].strings = std::move(strings);
    s_Mounts[slot].entryCount = header.entryCount;
    strncpy(s_Mounts[slot].path, discPath, IO_FILE_MAX_PATH - 1);
    s_Mounts[slot].path[IO_FILE_MAX_PATH - 1] = '\0';
    s_Mounts[slot].inUse = true;

    Engine_LogInfo("Archive: mounted '%s' (slot %d, %u entries)", discPath, slot, header.entryCount);
    return slot;
}

void Engine_Archive_Unmount(int32_t handle)
{
    if (handle < 0 || handle >= ARCH_MAX_MOUNTED || !s_Mounts[handle].inUse)
        return;

    Engine_IO_AcquireFileAccess();
    if (s_Mounts[handle].file)
        Engine_GetPlatform()->FileClose(s_Mounts[handle].file);
    Engine_IO_ReleaseFileAccess();

    s_Mounts[handle].toc.reset();
    s_Mounts[handle].strings.reset();
    s_Mounts[handle].file = nullptr;
    s_Mounts[handle].entryCount = 0;
    s_Mounts[handle].inUse = false;
}

bool Engine_Archive_Find(const char* assetPath, ArchiveLocator* outLoc)
{
    if (!assetPath || !outLoc)
        return false;

    char key[IO_FILE_MAX_PATH];
    Engine_Path_Canonical(assetPath, key);
    const uint32_t hash = Internal_Fnv1a32(key);

    // Search later mounts first (level shadows boot).
    for (int32_t s = ARCH_MAX_MOUNTED - 1; s >= 0; --s)
    {
        const MountedArchive* m = &s_Mounts[s];
        if (!m->inUse)
            continue;
        for (uint32_t i = 0; i < m->entryCount; ++i)
        {
            const ArchiveTocEntry* e = &m->toc[i];
            // Hash narrows the search; strcmp confirms (collisions are rare but must
            // never silently resolve to the wrong asset).
            if (e->nameHash == hash && strcmp(key, m->strings.get() + e->nameOffset) == 0)
            {
                outLoc->archive = s;
                outLoc->offset = e->offset;
                outLoc->size = e->size;
                return true;
            }
        }
    }
    return false;
}

bool Engine_Archive_ReadSync(const ArchiveLocator* loc, uint32_t spanOffset, void* dst, uint32_t bytes)
{
    if (!loc || !dst || loc->archive < 0 || loc->archive >= ARCH_MAX_MOUNTED)
        return false;
    MountedArchive* m = &s_Mounts[loc->archive];
    if (!m->inUse || !m->file)
        return false;
    // Bounds-check the requested span (overflow-safe).
    if (spanOffset > loc->size || bytes > loc->size - spanOffset)
        return false;

    Platform* platform = Engine_GetPlatform();

    bool ok = false;
    Engine_IO_AcquireFileAccess();
    if (platform->FileSeek(m->file, static_cast<uint64_t>(loc->offset) + spanOffset))
        ok = (platform->FileRead(m->file, dst, bytes) == bytes);
    Engine_IO_ReleaseFileAccess();
    return ok;
}

bool Engine_Archive_GetMount(int32_t slot, ArchiveMountInfo* outInfo)
{
    if (!outInfo || slot < 0 || slot >= ARCH_MAX_MOUNTED || !s_Mounts[slot].inUse)
        return false;

    const MountedArchive* m = &s_Mounts[slot];
    uint32_t payload = 0;
    for (uint32_t i = 0; i < m->entryCount; ++i)
        payload += m->toc[i].size;

    outInfo->path = m->path;
    outInfo->entryCount = m->entryCount;
    outInfo->payloadBytes = payload;
    return true;
}

bool Engine_Archive_GetEntry(int32_t slot, uint32_t index, ArchiveTocEntry* outEntry, char* outName, uint32_t nameSize)
{
    if (slot < 0 || slot >= ARCH_MAX_MOUNTED || !s_Mounts[slot].inUse)
        return false;

    const MountedArchive* m = &s_Mounts[slot];
    if (index >= m->entryCount)
        return false;

    if (outEntry)
        *outEntry = m->toc[index];
    if (outName && nameSize > 0)
    {
        strncpy(outName, m->strings.get() + m->toc[index].nameOffset, nameSize - 1);
        outName[nameSize - 1] = '\0';
    }
    return true;
}

void Engine_Archive_Shutdown()
{
    for (int32_t i = 0; i < ARCH_MAX_MOUNTED; ++i)
        Engine_Archive_Unmount(i);
}
