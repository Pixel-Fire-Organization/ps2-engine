# PlayStation 2 Memory Map & Allocation Strategy

This document serves as the master record for the PS2's limited **32 MB (EE) Main RAM** usage. Each allocation must be strictly mapped to ensure no memory fragmentation and peak performance.

## Global Memory Layout (EE RAM: 0x0000_0000 - 0x01FF_FFFF)

| Region Start | Size (Approx) | Description |
| :--- | :--- | :--- |
| `0x0000_0000` | ~1.5 MB | PS2 Kernel Reserved (OS, stacks, etc.) |
| `0x0010_0000` | ~Varies | Application ELF Code (`.text`, `.data`, `.bss`) |
| `0x0080_0000` | ~Variable | Heap — Raylib resources (`RL_MALLOC`), IO temp buffers |
| `0x01E0_0000` | ~Variable | Scrapyard / Temporary Pool / DMAC Buffers |

## Segmented Arena Layout (7 MB Total)

GFX resources (textures, meshes, audio) are **not** stored in engine arenas — they are managed entirely by Raylib's allocator (`RL_MALLOC`/`RL_FREE`) via the Resource Manager. The engine arenas serve only internal subsystems.

The arena starts at the base pointer allocated during `Engine_Init` and is partitioned as follows:

| Segment | Default Size | Default Slots | Purpose | Arena Enum |
| :--- | :--- | :--- | :--- | :--- |
| **Script** | 2 MB | 16 Slots | Lua VM heaps and bytecode storage | `ARENA_SCRIPT` |
| **Config** | 1 MB | 4 Slots | Configuration data, cached file reads | `ARENA_CONFIG` |
| **Level Data** | 4 MB | 8 Slots | Entity tables, nav data, spawn points | `ARENA_LEVEL_DATA` |

---

## Slot System (O(1) Replacement)

The Segmented Slot System partitions each arena into fixed-capacity buckets aligned to **16KB boundaries** for peak PS2 DMA/VIF performance.

1. **Alignment**: Every slot start is 16KB aligned.
2. **Locking**: Use `Engine_LockSlot` to prevent overwriting assets during active VIF transfers or draw calls.
3. **Usage**:
   ```c
   Engine_LoadToSlot(ARENA_CONFIG, 0, myData, dataSize);
   Engine_LockSlot(ARENA_CONFIG, 0);
   // ... Use data ...
   Engine_UnlockSlot(ARENA_CONFIG, 0);
   ```

## Memory Pool

- **`g_MainPool`**: 1 MB (Default), 256-byte chunks. Used as a **scratch allocator** for short-lived temporary objects: IO request metadata, transient decode contexts, particles, entities. Objects are either moved to an arena or freed quickly.

## Resource Manager (Raylib Resources)

Textures, models, sounds, and fonts are loaded through the **Resource Manager** (`EngineResource.h`), which delegates allocation to Raylib's `RL_MALLOC`/`RL_FREE`. The engine tracks these via a 64-entry handle table with reference counting and LRU eviction. See [RESOURCE_MANAGER.md](RESOURCE_MANAGER.md) for full details.

## Usage Guidelines

1. **Engine internals** (scripts, config, level data): Use `Engine_LoadToSlot` or `Engine_AddToArena`.
2. **GFX/Audio resources**: Use `Engine_Resource_Load` — never allocate these in engine arenas.
3. **Short-lived temp objects**: Use `Engine_PoolAllocMain` / `Engine_PoolFreeMain`.
4. **IO file buffers**: Managed by `EngineIO.c` using `malloc`/`free` (documented exception — freed after callback delivery).
