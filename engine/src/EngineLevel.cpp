#include <cstdio>
#include <cstring>

#include "Engine.h"
#include "EngineSector.h"
#include "GameAPI.h" // game::EntitySpawn + Engine_Game_DispatchSpawn

static Level* s_CurrentLevel = nullptr;

const Level* Engine_Level_Current() { return s_CurrentLevel; }

// Read the compiled level core (.ps2l) into ARENA_LEVEL_DATA slot 0 and point the
// Level's chunk views at it. Returns false on any format/size error.
static bool Internal_ReadCore(Level* level)
{
    char coreKey[IO_FILE_MAX_PATH];
    std::snprintf(coreKey, sizeof(coreKey), "%s.PS2L", level->name);

    ArchiveLocator loc;
    if (!Engine_Archive_Find(coreKey, &loc))
    {
        Engine_LogError("Level '%s': core '%s' not found in archive", level->name, coreKey);
        return false;
    }

    const size_t cap = Engine_GetSlotCapacity(ARENA_LEVEL_DATA, 0);
    if (loc.size > cap)
    {
        Engine_LogError("Level '%s': core is %u bytes, exceeds core slot capacity %zu", level->name, loc.size, cap);
        return false;
    }

    uint8_t* core = static_cast<uint8_t*>(Engine_GetSlot(ARENA_LEVEL_DATA, 0));
    if (!core || !Engine_Archive_ReadSync(&loc, 0, core, loc.size))
    {
        Engine_LogError("Level '%s': failed to read core", level->name);
        return false;
    }

    const LevelFileHeaderV2* hdr = reinterpret_cast<const LevelFileHeaderV2*>(core);
    if (hdr->magic != LEVEL_FILE_MAGIC || hdr->version != LEVEL_FILE_VERSION)
    {
        Engine_LogError("Level '%s': bad .ps2l magic/version", level->name);
        return false;
    }

    const LevelChunkEntry* table = reinterpret_cast<const LevelChunkEntry*>(core + sizeof(LevelFileHeaderV2));
    for (uint32_t i = 0; i < hdr->chunkCount; ++i)
    {
        const uint8_t* chunk = core + table[i].offset;
        switch (table[i].type)
        {
        case LEVEL_CHUNK_INFO:
            level->info = reinterpret_cast<const LevelInfoChunk*>(chunk);
            break;
        case LEVEL_CHUNK_MATERIALS:
            level->materials = reinterpret_cast<const LevelMaterialEntry*>(chunk);
            break;
        case LEVEL_CHUNK_GRID:
            level->grid = reinterpret_cast<const LevelGridCell*>(chunk);
            break;
        case LEVEL_CHUNK_ENTITIES:
            level->entsChunk = chunk;
            break;
        case LEVEL_CHUNK_FARFIELD:
            level->farfieldChunk = chunk;
            break;
        case LEVEL_CHUNK_BSP:
            // Reserved for indoor BSP — forward-compat hook, nothing to do yet.
            break;
        default:
            Engine_LogInfo("Level '%s': skipping unknown chunk 0x%08X", level->name, table[i].type);
            break;
        }
    }

    if (!level->info || !level->grid)
    {
        Engine_LogError("Level '%s': missing INFO or SGRD chunk", level->name);
        return false;
    }
    return true;
}

// Load + pin every material texture. Async: handles are stored now and the
// textures stream in over the next frames (resolved at draw time, like models).
static void Internal_PinMaterials(Level* level)
{
    const uint16_t count = level->info->materialCount;
    for (uint16_t i = 0; i < LEVEL_MAX_MATERIALS; ++i)
        level->materialTex[i] = -1;

    for (uint16_t i = 0; i < count && i < LEVEL_MAX_MATERIALS; ++i)
    {
        const char* key = level->materials[i].assetKey;
        if (key[0] == '\0')
            continue;
        int32_t handle = Engine_Resource_LoadAuto(key);
        if (handle >= 0)
        {
            Engine_Resource_Pin(handle);
            level->materialTex[i] = handle;
        }
        else
        {
            Engine_LogError("Level '%s': failed to load material '%s'", level->name, key);
        }
    }
}

// Walk the ENTS chunk and hand each record to the game's spawn dispatcher.
static void Internal_SpawnEntities(Level* level)
{
    const uint8_t* base = level->entsChunk;
    if (!base)
        return;

    uint32_t count, stringsOffset, stringsSize;
    std::memcpy(&count, base + 0, 4);
    std::memcpy(&stringsOffset, base + 4, 4);
    std::memcpy(&stringsSize, base + 8, 4);
    (void)stringsSize;

    const LevelEntityRecord* records = reinterpret_cast<const LevelEntityRecord*>(base + 12);
    const LevelEntityProp* props = reinterpret_cast<const LevelEntityProp*>(base + 12 + count * sizeof(LevelEntityRecord));
    const char* strings = reinterpret_cast<const char*>(base + stringsOffset);

    for (uint32_t r = 0; r < count; ++r)
    {
        const LevelEntityRecord& rec = records[r];

        game::EntityProp propBuf[LEVEL_MAX_ENTITY_PROPS];
        uint16_t pcount = rec.propCount;
        if (pcount > LEVEL_MAX_ENTITY_PROPS)
            pcount = LEVEL_MAX_ENTITY_PROPS;
        for (uint16_t p = 0; p < pcount; ++p)
        {
            const LevelEntityProp& lp = props[rec.propFirst + p];
            propBuf[p].key = strings + lp.keyOffset;
            propBuf[p].value = strings + lp.valueOffset;
        }

        game::EntitySpawn spawn;
        spawn.classname = strings + rec.classnameOffset;
        spawn.props = propBuf;
        spawn.propCount = pcount;
        spawn.x = rec.origin[0];
        spawn.y = rec.origin[1];
        spawn.z = rec.origin[2];
        Engine_Game_DispatchSpawn(spawn);
    }
    Engine_LogInfo("Level '%s': spawned %u entities", level->name, count);
}

bool Engine_Level_Load(Level* level)
{
    if (!level)
        return false;

    level->archiveHandle = -1;
    level->info = nullptr;
    level->materials = nullptr;
    level->grid = nullptr;
    level->entsChunk = nullptr;
    level->farfieldChunk = nullptr;

    // Mount LEVELS/<NAME>.PS2R.
    char rel[IO_FILE_MAX_PATH];
    std::snprintf(rel, sizeof(rel), "LEVELS/%s.PS2R", level->name);
    char disc[IO_FILE_MAX_PATH];
    const char* token = Engine_GetResourceLocationToken();
    if (!Engine_BuildPath(token ? token : "cdrom0:", rel, disc, sizeof(disc)))
    {
        Engine_LogError("Level '%s': could not build archive path", level->name);
        return false;
    }
    level->archiveHandle = Engine_Archive_Mount(disc);
    if (level->archiveHandle < 0)
    {
        Engine_LogError("Level '%s': could not mount '%s'", level->name, disc);
        return false;
    }

    if (!Internal_ReadCore(level))
    {
        Engine_Archive_Unmount(level->archiveHandle);
        level->archiveHandle = -1;
        return false;
    }

    Internal_PinMaterials(level);
    Internal_SpawnEntities(level);

    // Prime the resident sector ring around the grid centre.
    Engine_Sector_Begin(level);
    const float centerX = level->info->gridOriginX + level->info->cellsX * level->info->cellSize * 0.5f;
    const float centerZ = level->info->gridOriginZ + level->info->cellsZ * level->info->cellSize * 0.5f;
    Engine_Sector_Update(centerX, centerZ);

    s_CurrentLevel = level;
    Engine_LogInfo("Level '%s' loaded: %ux%u cells, %u materials", level->name, level->info->cellsX, level->info->cellsZ, level->info->materialCount);
    return true;
}

void Engine_Level_SetStreamingCenter(float worldX, float worldZ)
{
    Engine_Sector_Update(worldX, worldZ);
}

void Engine_Level_Unload(Level* level, bool keepPinned)
{
    Engine_Sector_End();

    if (level && !keepPinned)
    {
        const uint16_t count = level->info ? level->info->materialCount : 0;
        for (uint16_t i = 0; i < count && i < LEVEL_MAX_MATERIALS; ++i)
        {
            if (level->materialTex[i] >= 0)
            {
                Engine_Resource_Unpin(level->materialTex[i]);
                Engine_Resource_Unload(level->materialTex[i]);
                level->materialTex[i] = -1;
            }
        }
    }

    if (level && level->archiveHandle >= 0)
    {
        Engine_Archive_Unmount(level->archiveHandle);
        level->archiveHandle = -1;
    }

    for (uint32_t i = 0; i < MEM_BLOCK_LEVEL_DATA_SLOTS; ++i)
        Engine_ClearSlot(ARENA_LEVEL_DATA, i);

    s_CurrentLevel = nullptr;
    Engine_LogInfo("Level unloaded (keepPinned=%s)", keepPinned ? "true" : "false");
}
