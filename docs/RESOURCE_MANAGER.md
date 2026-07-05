# Resource Manager (`EngineResource`)

The Resource Manager provides a centralized system for loading, caching, tracking, and unloading Raylib-managed assets (
textures, models, sounds, fonts) on the PS2.

---

## Architecture Overview

Raylib owns all GFX/audio resource allocation via its internal `RL_MALLOC`/`RL_FREE` macros. The engine does **not**
duplicate this with custom arenas. Instead, the Resource Manager acts as a **handle table** that:

1. Streams raw file data from disc via `Engine_IO_ReadAsync`.
2. Hands the bytes to Raylib's `*FromMemory` decoders.
3. Tracks the resulting Raylib handle (Texture2D, Model, Sound, Font).
4. Manages lifecycle via reference counting, LRU eviction, and pinning.

---

## Resource Table

The table has a fixed capacity of **`RES_MAX_ENTRIES` (64)** slots, defined in `Constants.h`.

Each entry stores:

| Field           | Type            | Description                                                                       |
|:----------------|:----------------|:----------------------------------------------------------------------------------|
| `type`          | `ResourceType`  | `RES_TEXTURE`, `RES_MODEL`, `RES_SOUND`, `RES_FONT`                               |
| `state`         | `ResourceState` | `RES_STATE_EMPTY`, `RES_STATE_LOADING`, `RES_STATE_READY`                         |
| `key`           | `char[256]`     | Disc path used to identify the resource                                           |
| `refCount`      | `uint32_t`      | How many other loaded resources depend on this one                                |
| `lastUsedFrame` | `uint32_t`      | Frame counter updated on every `Engine_Resource_Get` call                         |
| `pinned`        | `bool`          | If true, the resource is never auto-evicted                                       |
| `generation`    | `uint16_t`      | Incremented every time this slot is freed; used to detect stale dep references    |
| `deps`          | `DepHandle[8]`  | Generation-safe dep handles `{index, generation}` for resources this one requires |
| `depCount`      | `uint8_t`       | Number of active dependencies                                                     |
| `handle`        | union           | The actual Raylib resource (`Texture2D`, `Model`, `Sound`, `Font`)                |

---

## API Reference

### `Engine_Resource_Load(ResourceType type, const char *path)` → `int32_t`

Load a resource from a `.ps2a` file. Returns a handle ≥ 0 on success, or -1 on failure.

- **Textures, Sounds, Fonts**: Loaded asynchronously via `Engine_IO_ReadAsync`, then decoded via `LoadImageFromMemory` /
  `LoadWaveFromMemory` / `LoadFontFromMemory`.
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

Returns `true` only when the resource has finished loading and is ready to use. Use this to implement loading screens or
deferred rendering.

### `Engine_Resource_Pin(int32_t handle)` / `Engine_Resource_Unpin(int32_t handle)`

Pinned resources are **never auto-evicted** by the LRU system. Use for critical assets like UI fonts, HUD textures, or
player models.

### `Engine_Resource_Unload(int32_t handle)`

Explicitly unload a resource. Decrements `refCount` on all its dependencies. Calls the matching Raylib `Unload*`
function.

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

### Generation safety

Each slot carries a `generation` counter that is incremented every time the slot is freed. When A's dependency on B is
recorded, the current `generation` of B's slot is snapshotted alongside the slot index into a `DepHandle
{index, generation}`. On unload, the engine checks that the slot's live generation still matches the stored one before
decrementing `refCount`. If the slot was freed and reused for a different resource since the dependency was bound, the
generation will have advanced and the decrement is skipped — preventing silent `refCount` corruption on the new
occupant.

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

### Header (`AssetFileHeader` — 2080 bytes)

| Offset | Size | Field          | Description                                                                                                     |
|:-------|:-----|:---------------|:----------------------------------------------------------------------------------------------------------------|
| 0      | 4    | `magic`        | `0x50533241` ("PS2A" LE)                                                                                        |
| 4      | 4    | `type`         | `0`=TEXTURE, `1`=MODEL, `2`=SOUND, `3`=FONT                                                                     |
| 8      | 1    | `depCount`     | Number of dependency paths (0–8)                                                                                |
| 9      | 3    | `reserved`     | Padding                                                                                                         |
| 12     | 16   | `ext`          | Source file extension written by the packer (e.g. `".qoi"`, `".png"`) — used by Raylib's `*FromMemory` decoders |
| 28     | 2048 | `deps[8][256]` | Null-terminated dependency paths                                                                                |
| 2076   | 4    | `dataSize`     | Byte count of the payload                                                                                       |

### Payload

Immediately follows the header. Contains the raw source file bytes (PNG, OBJ, OGG, TTF, etc.).

---

## PS2 GS VRAM Texture Size Limits

The PS2 GS has 4 MB of VRAM (512 pages at PSM32) shared between framebuffers, the depth buffer, and textures. Raylib's `initGsMemoryForRaylib()` allocates all the GS pages it needs at startup and registers the remaining free space as texture slots:

| Mode | Pages used (frames + depth) | Last texture slot ends | Free pages |
|:-----|:---------------------------:|:----------------------:|:----------:|
| PAL  | 0–239 (3 × 80)              | 505                    | 6          |
| NTSC | 0–209 (3 × 70)              | 475                    | 36         |

The **largest texture slot Raylib registers is 64 pages = 512×256 px** (two slots of this size in both modes). This is the true hardware maximum — no larger slot can be added without going out of range.

> ⚠️ **Do NOT call `pglAddGsMemSlot` with a starting page ≥ 512.** The GS TBP field is 14 bits wide; page 512 wraps to page 0 (framebuffer area), aliasing the screen buffer and causing visible VRAM corruption.

### How the budget is calculated

For the `kPsm32` (32-bit RGBA) format one GS "page" holds **64×32 pixels**. The number of pages a texture requires is:

```
pages = ceil(W / 64) × ceil(H / 32)
```

| Texture size | Pages needed | Fits in Raylib's slots? |
|:-------------|:------------:|:-----------------------:|
| 64×64        | 2            | ✅ yes                   |
| 128×128      | 8            | ✅ yes                   |
| 256×256      | 32           | ✅ yes                   |
| 512×256      | 64           | ✅ yes — **maximum**     |
| **512×512**  | **128**      | ❌ **rejected**          |
| 1024×512     | 256          | ❌ **rejected**          |

### What happens if a texture is too large?

`Internal_OnAsyncLoadComplete` in `EngineResource.c` checks the decoded image against three conditions before uploading:

1. `width > GFX_MAX_TEXTURE_WIDTH` (512) — absolute dimension cap
2. `height > GFX_MAX_TEXTURE_HEIGHT` (512) — absolute dimension cap
3. `ceil(W/64) × ceil(H/32) > GFX_MAX_TEXTURE_GS_PAGES` (64) — **GS slot budget** (the binding constraint)

If any condition is true the load is **hard-rejected**: the image is freed, the resource slot is reset to `RES_STATE_EMPTY`, an error is logged, and `-1` is effectively returned to the caller.

> **No silent downscaling occurs.** If a texture load fails due to size, `resources.is_ready(handle)` will never return `true` and `resources.load` will return `-1`.

> **Best practice**: Author textures at **256×256 or smaller**. 512×256 (or any 64-page combination) is the hardware maximum and should be used only when the full width is truly needed.

---

## Allowed Texture Resolutions

PS2 textures **must use power-of-2 (PoT) dimensions** in both width and height. NPOT textures are not supported by the
GS hardware and will fail to render correctly.

The engine hard limit is **64 GS pages** (`GFX_MAX_TEXTURE_GS_PAGES`), derived from the largest slot Raylib registers (512×256 px). Any texture whose page count exceeds this is rejected at load time with an error. Individual dimensions are also capped at 512 px each.

### GS Page Sizes by Format

Every format uses 8 192-byte pages regardless of pixel depth. The page *footprint in pixels* changes.

| Format              |  BPP   | Page dimensions | Notes                                                   |
|:--------------------|:------:|:----------------|:--------------------------------------------------------|
| **PSM32 / PSMCT32** | **32** | **64 × 32 px**  | **Used by Raylib — engine default**                     |
| PSM16 / PSMCT16     |   16   | 64 × 64 px      | Requires custom upload path (not implemented)           |
| PSMT8               |   8    | 128 × 64 px     | Paletted; requires custom upload path (not implemented) |
| PSMT4               |   4    | 128 × 128 px    | Paletted; requires custom upload path (not implemented) |

### Valid Resolutions (PSM32 — Raylib default)

Every combination of power-of-2 width and height where the **page count ≤ 64** is valid — including rectangular
textures such as `16×128` sprite sheets or `512×8` font atlases. The page count is the binding constraint, not the
individual dimensions.

Pages formula: `ceil(W / 64) × ceil(H / 32)`. **Maximum: 64 pages** (`GFX_MAX_TEXTURE_GS_PAGES`).  
Max simultaneous formula: `min(floor(128 / pages), 64)` — based on Raylib's two 64-page slots (128 pages total).

Any texture with **page count > 64**, or **either dimension > 512**, is hard-rejected at load time (returns `-1`).

> **Non-power-of-2 (NPOT) textures** are not supported by the PS2 GS and will fail to render correctly. Always use
> power-of-2 dimensions.

#### GS Pages consumed (PSM32, 32 bpp)

*How many GS VRAM pages does one texture of this size occupy? Cells marked ❌ exceed the 64-page budget and are rejected.*

| Height (px) ↓ \ Width (px) → | **8** | **16** | **32** | **64** | **128** | **256** | **512** |
|:----------------------------:|:-----:|:------:|:------:|:------:|:-------:|:-------:|:-------:|
|            **8**             |   1   |   1    |   1    |   1    |    2    |    4    |    8    |
|            **16**            |   1   |   1    |   1    |   1    |    2    |    4    |    8    |
|            **32**            |   1   |   1    |   1    |   1    |    2    |    4    |    8    |
|            **64**            |   2   |   2    |   2    |   2    |    4    |    8    |   16    |
|           **128**            |   4   |   4    |   4    |   4    |    8    |   16    |   32    |
|           **256**            |   8   |   8    |   8    |   8    |   16    |   32    |   64    |
|           **512**            |  16   |   16   |   16   |   16   |   32    |   64    |  ❌ 128  |

#### Textures per GS page — small textures only (pages = 1)

*For textures that occupy exactly one page (W ≤ 64, H ≤ 32), multiple distinct textures share that page in GS VRAM.
Formula: `(64 / W) × (32 / H)`.*

| Height (px) ↓ \ Width (px) → | **8** | **16** | **32** | **64** |
|:----------------------------:|:-----:|:------:|:------:|:------:|
|            **8**             |  32   |   16   |   8    |   4    |
|            **16**            |  16   |   8    |   4    |   2    |
|            **32**            |   8   |   4    |   2    |   1    |

> Textures larger than 64×32 each occupy their own page(s) exclusively — this table does not apply to them.

#### Max textures of this size in VRAM simultaneously

*How many textures of this size can be resident in GS VRAM at the same time?
Formula: `min(floor(128 / pages), 64)` — based on Raylib's two 64-page slots = 128 pages total.
Cells marked ❌ cannot be loaded at all (exceed the 64-page budget).*

| Height (px) ↓ \ Width (px) → | **8** | **16** | **32** | **64** | **128** | **256** | **512** |
|:----------------------------:|:-----:|:------:|:------:|:------:|:-------:|:-------:|:-------:|
|            **8**             |  64   |   64   |   64   |   64   |   64    |   32    |   16    |
|            **16**            |  64   |   64   |   64   |   64   |   64    |   32    |   16    |
|            **32**            |  64   |   64   |   64   |   64   |   64    |   32    |   16    |
|            **64**            |  64   |   64   |   64   |   64   |   32    |   16    |    8    |
|           **128**            |  32   |   32   |   32   |   32   |   16    |    8    |    4    |
|           **256**            |  16   |   16   |   16   |   16   |    8    |    4    |  **2**  |
|           **512**            |   8   |    8   |    8   |    8   |    4    |  **2**  |   ❌    |

> **512×256 (64 pages) is the absolute maximum** — it uses one of Raylib's two 64-page slots. Only two such textures can be resident simultaneously.

> **Non-square textures with 64 pages are valid** (e.g. `256×512`, `512×256`) and share the same slot pool.

---

## Asset Authoring Workflow

### Source Files

Place raw assets in `game/cd_files/RAYLIB/` as pairs:

- `<name>.json` — metadata descriptor
- `<name>.<ext>` — raw source file (`.jpg`, `.png`, `.obj`, `.gltf`, `.ogg`, `.ttf`, etc.)

**Texture format note**: The packer (`pack_assets.py`) automatically transcodes any TEXTURE source image to **QOI**
format using Pillow before embedding it in the `.ps2a` file. This means:

- You can author textures in any format Pillow supports (JPG, PNG, TGA, BMP, etc.)
- The on-disc payload is always `.qoi` and the `ext` field is `".qoi"`
- `qoi_decode` (raylib's built-in QOI decoder) is simpler and more reliable on PS2 MIPS than stb_image's JPEG decoder
- Requires `python3-pil` (Pillow) to be installed: `sudo apt-get install python3-pil`

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
  "deps": [
    "enemy_tex"
  ]
}
```

| Field    | Type     | Description                                           |
|:---------|:---------|:------------------------------------------------------|
| `type`   | string   | `"TEXTURE"`, `"MODEL"`, `"SOUND"`, `"FONT"`           |
| `source` | string   | Filename of the raw source file in the same directory |
| `deps`   | string[] | Names of other assets this one depends on (max 8)     |

### Packing

Run the packer (automatically called by CMake during build):

```bash
python3 tools/pack_assets.py
```

This reads all `.json` files from `game/cd_files/RAYLIB/`, compiles each into a `.ps2a` file, and writes them to
`game/cd_files/rassets/`. The ISO build pipeline (`app/CMakeLists.txt`) copies `cd_files/` into the ISO root, so
`rassets/*.ps2a` files are available at `cdrom0:\RASSETS\<NAME>.PS2A;1` on the PS2.

---

## Level Integration

The `Level` struct (see `EngineLevel.h`) lists up to **16 required resource paths**. When `Engine_Level_Load` is called:

1. Each required resource is loaded via `Engine_Resource_LoadAuto` — the type is inferred from each `.ps2a` header, so
   the list may freely mix textures, models, fonts, and sounds.
2. Each successfully loaded handle is **pinned** so it cannot be evicted during gameplay.
3. If any required resource fails to load, the function **rolls back**: every handle that was already pinned in this
   call
   is unpinned and unloaded, `s_LoadedCount` is reset to `0`, and `false` is returned. The resource table and GS VRAM
   budget are left in the same state as before the call — there are no partial or leaked pins.

When `Engine_Level_Unload` is called:

- If `keepPinned == false`: all required resources are unpinned and unloaded, and `ARENA_LEVEL_DATA` is bulk-cleared.
- If `keepPinned == true`: required resources stay pinned (useful for shared UI/fonts across level transitions).

---

## Usage Example

> **App layer**: Do not call `Engine_Resource_Load` / `Engine_Resource_Get` directly from game code. Use the friendly
`game::LoadResource` / `game::IsResourceReady` wrappers (`engine/include/GameAPI.h`) instead.

```cpp
// game/src/Game.cpp
int handle = game::LoadResource("TEXTURE", game::MakePath("RASSETS\\PLAYER.PS2A"));

void GameUpdate(float dt)
{
    if (game::IsResourceReady(handle))
    {
        // handle is valid; passed to future game::DrawTexture-style calls
    }
}
```

Engine-internal C usage (subsystems only):

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

