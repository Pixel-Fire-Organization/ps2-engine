# PlayStation 2 Memory Map & Allocation Strategy

This document serves as the master record for the PS2's limited **32 MB (EE) Main RAM** usage. Each allocation must be strictly mapped to ensure no memory fragmentation and peak performance.

## Global Memory Layout (EE RAM: 0x0000_0000 - 0x01FF_FFFF)

| Region Start | Size (Approx) | Description |
| :--- | :--- | :--- |
| `0x0000_0000` | ~1.5 MB | PS2 Kernel Reserved (OS, stacks, etc.) |
| `0x0010_0000` | ~Varies | Application ELF Code (`.text`, `.data`, `.bss`) |
| `0x0080_0000` | ~Variable | Main Heap / `g_MainArena` |
| `0x01E0_0000` | ~Variable | Scrapyard / Temporary Pool / DMAC Buffers |

## Segmented Arena Layout (16 MB Total)

The arena starts at the base pointer allocated during `Engine_Init` (~0xXXXX_XXXX) and is explicitly partitioned as follows:

| Segment | Default Size | Default Slots | Purpose | Arena Global |
| :--- | :--- | :--- | :--- | :--- |
| **Textures** | 8 MB | 10 Slots | Texture data (PCX/BMP/TIM2 clones) | `ARENA_TEXTURE` |
| **Meshes** | 2 MB | 8 Slots | Vertex/Index buffers & Models | `ARENA_MESH` |
| **Audio** | 2 MB | 8 Slots | SPU2/VAG Audio sample cache | `ARENA_AUDIO` |
| **Scripts** | 2 MB | 16 Slots | Game logic, entity states, script VMs | `ARENA_SCRIPT` |
| **UI** | 1 MB | 4 Slots | Fonts, UI textures, menus | `ARENA_UI` |
| **System** | 1 MB | 4 Slots | Internal scratchpads, DMA chains | `ARENA_SYSTEM` |

---

## Slot System (O(1) Replacement)
The Segmented Slot System partitions each arena into fixed-capacity buckets aligned to **16KB boundaries** for peak PS2 DMA/VIF performance.

1. **Alignment**: Every slot start is 16KB aligned.
2. **Locking**: Use `Engine_LockSlot` to prevent overwriting assets during active VIF transfers or draw calls.
3. **Usage**:
   ```c
   Engine_LoadToSlot(ARENA_TEXTURE, 5, myData, dataSize);
   Engine_LockSlot(ARENA_TEXTURE, 5); 
   // ... Draw texture ...
   Engine_UnlockSlot(ARENA_TEXTURE, 5);
   ```

## Usage Guidelines
1. **Allocation**: Use `Engine_LoadToSlot` for replaceable assets, or `Engine_AddToArena` for linear stack allocation.

## Memory Pool
- **`g_MainPool`**: 1 MB (Default). Used for small, fixed-size objects (256 bytes) that need frequent allocation/deallocation. Manual defragmentation may be required if using handles.
