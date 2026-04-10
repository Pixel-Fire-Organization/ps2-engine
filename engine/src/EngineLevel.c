#include <string.h>
#include "Engine.h"

// Tracks the handles loaded by Engine_Level_Load so we can unpin them later.
// This is internal — the Level struct itself stays minimal.
static int32_t s_LoadedHandles[LEVEL_MAX_RESOURCES_COUNT];
static uint32_t s_LoadedCount = 0;

bool Engine_Level_Load(Level* level)
{
    if (!level)
        return false;

    s_LoadedCount = 0;

    // Clamp requiredCount to LEVEL_MAX_RESOURCES_COUNT to prevent out-of-bounds
    // access to s_LoadedHandles[] and to align the success check logic.
    uint32_t effectiveCount = level->requiredCount;
    if (effectiveCount > LEVEL_MAX_RESOURCES_COUNT)
    {
        Engine_LogError("Level '%s': requiredCount %u exceeds LEVEL_MAX_RESOURCES_COUNT (%d), clamping", level->name,
                        level->requiredCount, LEVEL_MAX_RESOURCES_COUNT);
        effectiveCount = LEVEL_MAX_RESOURCES_COUNT;
    }

    for (uint32_t i = 0; i < effectiveCount; i++)
    {

        const char* path = level->requiredResources[i];
        if (path[0] == '\0')
            continue;

        // Let the .ps2a header's type field drive the decode path.
        // Passing a hardcoded RES_TEXTURE would silently misroute MODEL/FONT/SOUND
        // assets through the wrong decoder and fail; Engine_Resource_LoadAuto peeks
        // the header to get the real type before dispatching.
        int32_t handle = Engine_Resource_LoadAuto(path);
        if (handle >= 0)
        {
            Engine_Resource_Pin(handle);
            s_LoadedHandles[s_LoadedCount] = handle;
            s_LoadedCount++;
        }
        else
        {
            Engine_LogError("Level '%s': failed to load required resource '%s'", level->name, path);
        }
    }

    if (s_LoadedCount != effectiveCount)
    {
        // Partial failure — rollback every handle already pinned so we don't
        // leak pinned resources or corrupt the GS VRAM / resource-table budget.
        Engine_LogError("Level '%s': only %u/%u required resources loaded — rolling back", level->name, s_LoadedCount,
                        effectiveCount);

        for (uint32_t i = 0; i < s_LoadedCount; i++)
        {
            Engine_Resource_Unpin(s_LoadedHandles[i]);
            Engine_Resource_Unload(s_LoadedHandles[i]);
        }

        s_LoadedCount = 0;
        return false;
    }

    Engine_LogInfo("Level '%s' loaded: %u/%u required resources pinned", level->name, s_LoadedCount,
                   level->requiredCount);
    return true;
}

void Engine_Level_Unload(Level* level, bool keepPinned)
{
    UNUSED_VAR(level);

    if (!keepPinned)
    {
        // Unpin and unload all resources that were loaded for this level
        for (uint32_t i = 0; i < s_LoadedCount; i++)
        {
            int32_t h = s_LoadedHandles[i];
            Engine_Resource_Unpin(h);
            Engine_Resource_Unload(h);
        }
    }

    s_LoadedCount = 0;

    // Bulk-clear level-specific cached data (entity tables, nav data, etc.)
    // We need access to the arena — use Engine_AddToArena's underlying arena.
    // For now, clear all slots in ARENA_LEVEL_DATA.
    for (uint32_t i = 0; i < MEM_BLOCK_LEVEL_DATA_SLOTS; i++)
    {
        Engine_ClearSlot(ARENA_LEVEL_DATA, i);
    }

    Engine_LogInfo("Level unloaded (keepPinned=%s)", keepPinned ? "true" : "false");
}
