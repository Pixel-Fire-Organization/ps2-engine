# PS2 Engine Allocation Strategies

This document explains the architectural design behind our memory management system. The PlayStation 2 has a unified but limited **32 MB Main RAM** and no hardware memory protection. We use three distinct allocation models to eliminate fragmentation and maximize performance.

---

## 1. Specialized Arenas (Segmented Slots)
**Best For**: Engine-internal data — Lua VMs, configuration, level caches.

### The Model:
Each internal subsystem is assigned a dedicated "Segment" of the 7MB engine arena. These segments are partitioned into **Fixed-Capacity Slots** aligned to **16 KB**.

- **Logic**: Linear Stack Allocation within a slot.
- **Replacement**: Instant O(1) overwriting of a slot.
- **Freeing**: Individual assets cannot be "freed." Use `Engine_ClearSlot` to wipe a bucket or `Engine_ArenaReset` to wipe an entire segment (e.g., on level transition).
- **Hardware Optimization**: Every slot start is 16KB aligned for PS2 DMAC and VIF transfers.

**Usage Example**:
```c
// Store config data in Config slot 0
Engine_LoadToSlot(ARENA_CONFIG, 0, configData, size);
```

---

## 2. Resource Manager (Raylib Delegation)
**Best For**: GFX and audio resources — textures, models, sounds, fonts.

### The Model:
Raylib owns all GFX/audio allocation via `RL_MALLOC`/`RL_FREE`. The engine's **Resource Manager** (`EngineResource.h`) wraps this with a **64-entry handle table** that provides:

- **Async Loading**: Files are streamed from disc via `Engine_IO_ReadAsync`, then decoded via Raylib's `*FromMemory` APIs.
- **Reference Counting**: Resources track dependencies. A texture referenced by a model has `refCount > 0` and cannot be auto-evicted.
- **LRU Eviction**: When the table is full, the least recently used unpinned resource with `refCount == 0` is evicted.
- **Pinning**: Critical resources (UI fonts, HUD) can be pinned to prevent eviction.
- **Sync Exception**: Models are loaded synchronously via `LoadModel()` (no `FromMemory` variant exists in Raylib).

**Usage Example**:
```c
int32_t tex = Engine_Resource_Load(RES_TEXTURE, "cdrom0:\\RASSETS\\PLAYER_TEX.PS2A;1");
// ... later in game loop ...
if (Engine_Resource_IsReady(tex)) {
    Texture2D *t = (Texture2D *)Engine_Resource_Get(tex);
    DrawTexture(*t, 0, 0, WHITE);
}
```

See [RESOURCE_MANAGER.md](RESOURCE_MANAGER.md) for full documentation.

---

## 3. Memory Pool (`g_MainPool`)
**Best For**: Small, short-lived, temporary objects — IO metadata, decode contexts, particles, entities.

### The Model:
A **Fixed-Size Block Allocator** (256 bytes per chunk, 1 MB total). Uses a free-list for O(1) alloc/free.

- **Logic**: Linked-list of free chunks.
- **Replacement**: Frequent allocation and deallocation in arbitrary order.
- **Freeing**: Full O(1) support via `Engine_PoolFreeMain`.
- **Fragmentation**: Zero — every chunk is identical in size.
- **Role**: Scratch allocator. Objects allocated from the pool are either moved into an arena or freed quickly. Never used for long-lived storage.

**Usage Example**:
```c
// Allocate a temporary context for an async load
ResourceLoadContext *ctx = (ResourceLoadContext *)Engine_PoolAllocMain();
// ... use it ...
Engine_PoolFreeMain(ctx);
```

---

## Summary Comparison Table

| Feature | **Specialized Arenas** | **Resource Manager** | **Memory Pool** |
| :--- | :--- | :--- | :--- |
| **Data Structure** | Segmented Stack / Slot | Handle Table + Raylib | Block-based Free-List |
| **Asset Size** | Variable (up to slot capacity) | Variable (Raylib-managed) | **Fixed** (256B) |
| **Manual Freeing** | No (Reset/Clear only) | **Yes** (Unload) | **Yes** (O(1)) |
| **Auto Eviction** | No | **Yes** (LRU) | No |
| **Alignment** | **16 KB** (DMA Optimized) | Raylib-managed | 16 Byte (QW) |
| **Primary Goal** | Storing **Engine State** | Storing **Assets** | Storing **Temp Data** |

---

## Which one should I use?

1. **"I'm loading a level background texture."** → `Engine_Resource_Load(RES_TEXTURE, ...)`
2. **"I'm spawning 50 spark particles."** → `Engine_PoolAllocMain()`
3. **"I'm loading a new enemy model."** → `Engine_Resource_Load(RES_MODEL, ...)`
4. **"I need a place to store the scoreboard config."** → `ARENA_CONFIG`
5. **"I'm creating a temporary context for an async IO callback."** → `Engine_PoolAllocMain()`
6. **"I'm caching level entity spawn points."** → `ARENA_LEVEL_DATA`
7. **"I'm loading a Lua script for an NPC."** → `ARENA_SCRIPT` (via `Engine_Script_Load`)
