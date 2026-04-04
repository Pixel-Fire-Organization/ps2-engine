# Resource Manager (`EngineResource`)

The Resource Manager provides a centralized system for loading, caching, tracking, and unloading Raylib-managed assets (textures, models, sounds, fonts) on the PS2.

---

## Architecture Overview

Raylib owns all GFX/audio resource allocation via its internal `RL_MALLOC`/`RL_FREE` macros. The engine does **not** duplicate this with custom arenas. Instead, the Resource Manager acts as a **handle table** that:

1. Streams raw file data from disc via `Engine_IO_ReadAsync`.
2. Hands the bytes to Raylib's `*FromMemory` decoders.
3. Tracks the resulting Raylib handle (Texture2D, Model, Sound, Font).
4. Manages lifecycle via reference counting, LRU eviction, and pinning.

---

## Resource Table

The table has a fixed capacity of **`RES_MAX_ENTRIES` (64)** slots, defined in `Constants.h`.

Each entry stores:

| Field | Type | Description |
| :--- | :--- | :--- |
| `type` | `ResourceType` | `RES_TEXTURE`, `RES_MODEL`, `RES_SOUND`, `RES_FONT` |
| `state` | `ResourceState` | `RES_STATE_EMPTY`, `RES_STATE_LOADING`, `RES_STATE_READY` |
| `key` | `char[256]` | Disc path used to identify the resource |
| `refCount` | `uint32_t` | How many other loaded resources depend on this one |
| `lastUsedFrame` | `uint32_t` | Frame counter updated on every `Engine_Resource_Get` call |
| `pinned` | `bool` | If true, the resource is never auto-evicted |
| `deps` | `int32_t[8]` | Handles of resources this entry depends on |
| `depCount` | `uint8_t` | Number of active dependencies |
| `handle` | union | The actual Raylib resource (`Texture2D`, `Model`, `Sound`, `Font`) |

---

## API Reference

### `Engine_Resource_Load(ResourceType type, const char *path)` → `int32_t`

Load a resource from a `.ps2a` file. Returns a handle ≥ 0 on success, or -1 on failure.

- **Textures, Sounds, Fonts**: Loaded asynchronously via `Engine_IO_ReadAsync`, then decoded via `LoadImageFromMemory` / `LoadWaveFromMemory` / `LoadFontFromMemory`.
- **Models**: Loaded **synchronously** via Raylib's `LoadModel()` (no `FromMemory` variant exists).
- If the resource is already loaded, returns the existing handle without reloading.
- If the table is full, triggers LRU eviction (see below).

### `Engine_Resource_Get(int32_t handle)` → `void*`

Returns a pointer to the Raylib resource. Caller casts to the appropriate type:

```c
Texture2D *tex = (Texture2D *)Engine_Resource_Get(texHandle);
if (tex) DrawTexture(*tex, 0, 0, WHITE);
```

Returns `NULL` if the handle is invalid or the resource is still loading.

### `Engine_Resource_IsReady(int32_t handle)` → `bool`

Returns `true` only when the resource has finished loading and is ready to use. Use this to implement loading screens or deferred rendering.

### `Engine_Resource_Pin(int32_t handle)` / `Engine_Resource_Unpin(int32_t handle)`

Pinned resources are **never auto-evicted** by the LRU system. Use for critical assets like UI fonts, HUD textures, or player models.

### `Engine_Resource_Unload(int32_t handle)`

Explicitly unload a resource. Decrements `refCount` on all its dependencies. Calls the matching Raylib `Unload*` function.

### `Engine_Resource_UnloadAll()`

Force-unloads every resource in the table, including pinned ones. Used during shutdown and full level transitions.

### `Engine_Resource_Update()`

Called once per frame (from `Engine_Update`) to advance the internal frame counter used for LRU tracking.

---

## Reference Counting

Resources track how many other loaded resources depend on them via `refCount`.

- When resource A lists resource B as a dependency, loading A increments B's `refCount`.
- When A is unloaded, B's `refCount` is decremented.
- A resource with `refCount > 0` is **never evicted** by LRU (another resource still needs it).

Dependencies are declared in the `.ps2a` file header (see Asset Format below).

---

## LRU Eviction

When `Engine_Resource_Load` finds no free slot, it scans for the best eviction candidate:

1. Must have `state != RES_STATE_EMPTY`.
2. Must NOT be `pinned`.
3. Must have `refCount == 0`.
4. Among candidates, the one with the **lowest `lastUsedFrame`** is evicted.

If no candidate is found (all slots are pinned or ref-held), the load fails and returns -1.

---

## `.ps2a` Asset File Format

Each compiled asset is a binary `.ps2a` file with a fixed-size header followed by raw data.

### Header (`AssetFileHeader` — 2064 bytes)

| Offset | Size | Field | Description |
| :--- | :--- | :--- | :--- |
| 0 | 4 | `magic` | `0x50533241` ("PS2A" LE) |
| 4 | 4 | `type` | `0`=TEXTURE, `1`=MODEL, `2`=SOUND, `3`=FONT |
| 8 | 1 | `depCount` | Number of dependency paths (0–8) |
| 9 | 3 | `reserved` | Padding |
| 12 | 2048 | `deps[8][256]` | Null-terminated dependency paths |
| 2060 | 4 | `dataSize` | Byte count of the payload |

### Payload

Immediately follows the header. Contains the raw source file bytes (PNG, OBJ, OGG, TTF, etc.).

---

## Asset Authoring Workflow

### Source Files

Place raw assets in `app/cd_files/RAYLIB/` as pairs:

- `<name>.json` — metadata descriptor
- `<name>.<ext>` — raw source file (`.png`, `.obj`, `.gltf`, `.ogg`, `.ttf`, etc.)

### JSON Schema

```json
{
    "type": "TEXTURE",
    "source": "player_tex.png",
    "deps": []
}
```

```json
{
    "type": "MODEL",
    "source": "enemy_model.obj",
    "deps": ["enemy_tex"]
}
```

| Field | Type | Description |
| :--- | :--- | :--- |
| `type` | string | `"TEXTURE"`, `"MODEL"`, `"SOUND"`, `"FONT"` |
| `source` | string | Filename of the raw source file in the same directory |
| `deps` | string[] | Names of other assets this one depends on (max 8) |

### Packing

Run the packer (automatically called by CMake during build):

```bash
python3 scripts/pack_assets.py
```

This reads all `.json` files from `app/cd_files/RAYLIB/`, compiles each into a `.ps2a` file, and writes them to `app/cd_files/rassets/`. The ISO build pipeline (`app/CMakeLists.txt`) copies `cd_files/` into the ISO root, so `rassets/*.ps2a` files are available at `cdrom0:\RASSETS\<NAME>.PS2A;1` on the PS2.

---

## Level Integration

The `Level` struct (see `EngineLevel.h`) lists up to **16 required resource paths**. When `Engine_Level_Load` is called:

1. Each required resource is loaded via `Engine_Resource_Load`.
2. Each is **pinned** so it cannot be evicted during gameplay.

When `Engine_Level_Unload` is called:

- If `keepPinned == false`: all required resources are unpinned and unloaded, and `ARENA_LEVEL_DATA` is bulk-cleared.
- If `keepPinned == true`: required resources stay pinned (useful for shared UI/fonts across level transitions).

---

## Usage Example

```c
// Load a texture from a packed asset
int32_t texHandle = Engine_Resource_Load(RES_TEXTURE,
    "cdrom0:\\RASSETS\\PLAYER_TEX.PS2A;1");

// In game loop
if (Engine_Resource_IsReady(texHandle)) {
    Texture2D *tex = (Texture2D *)Engine_Resource_Get(texHandle);
    DrawTexture(*tex, 100, 100, WHITE);
}

// When no longer needed
Engine_Resource_Unload(texHandle);
```

