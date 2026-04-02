# PS2 Engine Allocation Strategies

This document explains the architectural design behind our memory management system. Because the PlayStation 2 has a unified but limited **32 MB Main RAM** and no hardware memory protection, we rely on two distinct allocation models to eliminate fragmentation and maximize performance.

---

## 1. Specialized Arenas (Segmented Slots)
**Best For**: Large, long-lived, and immutable assets (Textures, Meshes, Audio).

### The Model:
Each major asset type is assigned a specialized "Segment" of the 30MB engine arena. These segments are intentionally partitioned into **Fixed-Capacity Slots** (Buckets) aligned to **16 KB**.

- **Logic**: Linear Stack Allocation within a slot.
- **Replacement**: Instant O(1) overwriting of a slot (e.g., swapping a character model).
- **Freeing**: Individual assets cannot be "freed." You must `Engine_ClearSlot` to wipe a bucket or `Engine_ArenaReset` to wipe an entire segment.
- **Hardware Optimization**: Every slot start is 16KB aligned, which is the hardware "sweet spot" for for PS2 DMAC and VIF transfers.

**Usage Example**:
```c
// Replacing the primary character mesh in Slot 0
Engine_LoadToSlot(ARENA_MESH, 0, newMeshData, size);
```

---

## 2. Global Memory Pool (`g_MainPool`)
**Best For**: Small, short-lived, and dynamic objects (Projectiles, Particles, UI Widgets, Entities).

### The Model:
The Memory Pool is a **Fixed-Size Block Allocator** (Default: 256 bytes per chunk). It uses a "Free List" to manage available space.

- **Logic**: Linked-list of free chunks.
- **Replacement**: Frequent allocation and deallocation in arbitrary order.
- **Freeing**: Full O(1) support for `Engine_PoolFree`.
- **Fragmentation**: Zero. Because every chunk is identical in size, any "hole" left by a deleted object is always perfectly shaped for a new one.

**Usage Example**:
```c
// Creating a transient particle effect
Particle* p = (Particle*)Engine_PoolAlloc(&g_MainPool);
// ... simulate ...
Engine_PoolFree(&g_MainPool, p);
```

---

## Summary Comparison Table

| Feature | **Specialized Arenas** | **Memory Pool (g_MainPool)** |
| :--- | :--- | :--- |
| **Data Structure** | Segmented Stack / Slot | Block-based Free-List |
| **Asset Size** | Variable (up to slot capacity) | **Fixed** (Default: 256B) |
| **Manual Freeing** | No (Reset/Clear only) | **Yes** (Instant O(1)) |
| **Alignment** | **16 KB** (DMA Optimized) | 16 Byte (QW Aligned) |
| **Primary Goal** | Storing the **World** | Storing the **Action** |

---

## Which one should I use?

1. **"I'm loading a level background texture."** -> `ARENA_TEXTURE`
2. **"I'm spawning 50 spark particles."** -> `g_MainPool`
3. **"I'm loading a new enemy model."** -> `ARENA_MESH`
4. **"I need a place to store the scoreboard's current string."** -> `ARENA_UI` or `ARENA_SYSTEM`
5. **"I'm creating a temporary task for an async IO read."** -> `g_MainPool`
