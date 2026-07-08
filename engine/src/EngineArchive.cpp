#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>

#include "Engine.h"

// A mounted archive: the open file plus its TOC and string table in heap memory.
// The FILE* stays open for the archive's lifetime so streaming reads are a bare
// fseek + fread with no per-file open cost. Unmounting closes it — the fast drop.
typedef struct
{
    FILE* file;
    ArchiveTocEntry* toc; // heap array [entryCount]
    char* strings; // heap string table [stringsSize]
    uint32_t entryCount;
    bool inUse;
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
    memset(s_Mounts, 0, sizeof(s_Mounts));
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

    Engine_IO_AcquireFileAccess();
    FILE* f = fopen(discPath, "rb");
    if (!f)
    {
        Engine_IO_ReleaseFileAccess();
        Engine_LogError("Archive: cannot open '%s'", discPath);
        return -1;
    }

    ArchiveFileHeader header;
    bool ok = (fread(&header, 1, sizeof(header), f) == sizeof(header));
    ArchiveTocEntry* toc = nullptr;
    char* strings = nullptr;

    if (ok && (header.magic != ARCH_FILE_MAGIC || header.version != ARCH_FILE_VERSION))
    {
        Engine_LogError("Archive: bad magic/version in '%s'", discPath);
        ok = false;
    }

    if (ok && header.entryCount > 0)
    {
        const size_t tocBytes = static_cast<size_t>(header.entryCount) * sizeof(ArchiveTocEntry);
        toc = static_cast<ArchiveTocEntry*>(memalign(16, tocBytes));
        strings = static_cast<char*>(malloc(header.stringsSize ? header.stringsSize : 1));
        if (!toc || !strings)
        {
            Engine_LogError("Archive: out of memory mounting '%s'", discPath);
            ok = false;
        }
        // TOC immediately follows the header; string table is at stringsOffset.
        if (ok && fseek(f, static_cast<long>(sizeof(ArchiveFileHeader)), SEEK_SET) != 0)
            ok = false;
        if (ok && fread(toc, 1, tocBytes, f) != tocBytes)
            ok = false;
        if (ok && header.stringsSize > 0)
        {
            if (fseek(f, static_cast<long>(header.stringsOffset), SEEK_SET) != 0)
                ok = false;
            else if (fread(strings, 1, header.stringsSize, f) != header.stringsSize)
                ok = false;
        }
    }
    Engine_IO_ReleaseFileAccess();

    if (!ok)
    {
        free(toc);
        free(strings);
        fclose(f);
        return -1;
    }

    s_Mounts[slot].file = f;
    s_Mounts[slot].toc = toc;
    s_Mounts[slot].strings = strings;
    s_Mounts[slot].entryCount = header.entryCount;
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
        fclose(s_Mounts[handle].file);
    Engine_IO_ReleaseFileAccess();

    free(s_Mounts[handle].toc);
    free(s_Mounts[handle].strings);
    memset(&s_Mounts[handle], 0, sizeof(MountedArchive));
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
            if (e->nameHash == hash && strcmp(key, m->strings + e->nameOffset) == 0)
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

    bool ok = false;
    Engine_IO_AcquireFileAccess();
    if (fseek(m->file, static_cast<long>(loc->offset + spanOffset), SEEK_SET) == 0)
        ok = (fread(dst, 1, bytes, m->file) == bytes);
    Engine_IO_ReleaseFileAccess();
    return ok;
}

void Engine_Archive_Shutdown()
{
    for (int32_t i = 0; i < ARCH_MAX_MOUNTED; ++i)
        Engine_Archive_Unmount(i);
}
