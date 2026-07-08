# PS2 Engine Allocation Strategies

This document explains the architectural design behind our memory management system. The PlayStation 2 has a unified but
limited **32 MB Main RAM** and no hardware memory protection. We use three distinct allocation models to eliminate
fragmentation and maximize performance.

---

## 1. Specialized Arenas (Segmented Slots)

**Best For**: Engine-internal data — configuration, level caches.

### The Model:

Each internal subsystem is assigned a dedicated "Segment" of the ~7.25MB engine arena. These segments are partitioned
into **Fixed-Capacity Slots** aligned to **16 KB**. Current map (see `Constants.MEM.h`):

| Segment | Size | Slots | Slot capacity | Use |
|---------|------|-------|---------------|-----|
| `ARENA_CONFIG` | 256 KB | 4 | 64 KB | config/boot data |
| `ARENA_LEVEL_DATA` | 4 MB | 16 | 256 KB | level core (slots 0-1) + streamed sectors (2-10) + prefetch (11-15) |
| `ARENA_RENDERER` | 3 MB | 1 | 3 MB | renderer scratch |

- **Logic**: Linear Stack Allocation within a slot.
- **Replacement**: Instant O(1) overwriting of a slot.
- **Freeing**: Individual assets cannot be "freed." Use `Engine_ClearSlot` to wipe a bucket or `Engine_ArenaReset` to
  wipe an entire segment (e.g., on level transition).
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

Raylib owns all GFX/audio allocation via `RL_MALLOC`/`RL_FREE`. The engine's **Resource Manager** (`EngineResource.h`)
wraps this with a **64-entry handle table** that provides:

- **Async Loading**: Files are streamed from disc via `Engine_IO_ReadAsync`, then decoded via Raylib's `*FromMemory`
  APIs.
- **Reference Counting**: Resources track dependencies. A texture referenced by a model has `refCount > 0` and cannot be
  auto-evicted.
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
- **Role**: Scratch allocator. Objects allocated from the pool are either moved into an arena or freed quickly. Never
  used for long-lived storage.

**Usage Example**:

```c
// Allocate a temporary context for an async load
ResourceLoadContext *ctx = (ResourceLoadContext *)Engine_PoolAllocMain();
// ... use it ...
Engine_PoolFreeMain(ctx);
```

---

## 4. Async IO Read Buffer (Static, Single Shared)

**Best For**: The backing storage that the background IO thread reads raw file bytes into before the callback is
invoked.

### The Model:

`EngineIO.c` maintains a single `static uint8_t s_SharedReadBuffer[IO_READ_BUFFER_SIZE]`, 16-byte aligned (512 KB). Only
one request is ever reading into or decoding from this buffer at a time, enforced by a binary semaphore (
`s_IOBufferSema`).

- **No heap involvement**: `malloc`/`free` are never called for IO reads.
- **Bounded**: Files larger than `IO_READ_BUFFER_SIZE` are rejected immediately with a logged error; the callback
  receives `(NULL, 0, userData)` and handles the failure.
- **Race-safe (two guards)**:
    - `s_IOBufferSema` (binary, init=1): IO thread acquires it before reading; main thread releases it after the
      callback returns and the slot is `IDLE`. Prevents the IO thread from overwriting the buffer while the callback
      still holds a pointer to it.
    - `IO_STATE_DISPATCHING`: slot remains in this state during the callback so it cannot be re-queued and so the
      main-thread-held pointer stays unambiguously valid.
- **Memory cost**: `IO_READ_BUFFER_SIZE` = **512 KB in BSS**. A per-slot design (`16 × 512 KB = 8 MB`) pushed `.bss` to
  virtual address `0x30000000`, which has no TLB mapping on the PS2 EE, causing a store TLB miss cascade in the crt0
  BSS-zero loop at startup.
- **Throughput**: Reads are serialised through the single buffer (IO thread blocks on the sema until the previous
  callback finishes). On PS2 the CD-ROM bottleneck dominates latency, so this has no measurable impact.

**Constants (both in `Constants.h`)**:

```c
#define IO_ASYNC_MAX_REQUESTS 16    // max queued requests (metadata only — no per-slot buffer)
#define IO_READ_BUFFER_SIZE (512 * 1024)  // single shared buffer size / max file size per async read
```

---

## Summary Comparison Table

| Feature            | **Specialized Arenas**         | **Resource Manager**      | **Memory Pool**       | **IO Read Buffers**    |
|:-------------------|:-------------------------------|:--------------------------|:----------------------|:-----------------------|
| **Data Structure** | Segmented Stack / Slot         | Handle Table + Raylib     | Block-based Free-List | Fixed 2-D static array |
| **Asset Size**     | Variable (up to slot capacity) | Variable (Raylib-managed) | **Fixed** (256B)      | **Fixed** (512 KB max) |
| **Manual Freeing** | No (Reset/Clear only)          | **Yes** (Unload)          | **Yes** (O(1))        | No (static lifetime)   |
| **Auto Eviction**  | No                             | **Yes** (LRU)             | No                    | No                     |
| **Alignment**      | **16 KB** (DMA Optimized)      | Raylib-managed            | 16 Byte (QW)          | **16 Byte** (GS DMA)   |
| **Primary Goal**   | Storing **Engine State**       | Storing **Assets**        | Storing **Temp Data** | **Async file reads**   |

---

## Which one should I use?

1. **"I'm loading a level background texture."** → `Engine_Resource_Load(RES_TEXTURE, ...)`
2. **"I'm spawning 50 spark particles."** → `Engine_PoolAllocMain()`
3. **"I'm loading a new enemy model."** → `Engine_Resource_Load(RES_MODEL, ...)`
4. **"I need a place to store the scoreboard config."** → `ARENA_CONFIG`
5. **"I'm creating a temporary context for an async IO callback."** → `Engine_PoolAllocMain()`
6. **"I'm caching level entity spawn points."** → `ARENA_LEVEL_DATA`
7. **"I need to read a raw file asynchronously."** → `Engine_IO_ReadAsync()` — data arrives in a static
   `s_ReadBuffers[slot]` and is valid only for the duration of the callback. Files larger than `IO_READ_BUFFER_SIZE` (
   512 KB) are rejected.
