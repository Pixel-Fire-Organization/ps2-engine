# Asset Descriptor Directory (`game/cd_files/ASSETS/`)

This directory contains **source assets** (images, models, fonts, sounds) and their **JSON descriptor files**. During the build, `tools/pack_assets.py` reads every `.json` file here and compiles each pair into a binary `.ps2a` file written to `game/cd_files/rassets/`.

The generated `.ps2a` files are automatically included in the bootable ISO at `cdrom0:\RASSETS\`.

---

## Descriptor File Format

Each asset requires a `.json` file with the **same base name** as the source file:

```
BOX.JSON   ←  descriptor
BOX.PNG    ←  source image
```

The packer derives the output name from the JSON filename:
`BOX.JSON` → `rassets/BOX.ps2a` → `cdrom0:\RASSETS\BOX.PS2A;1`

### Schema

```json
{
    "type":   "TEXTURE",
    "source": "BOX.PNG",
    "deps":   []
}
```

| Field | Type | Required | Description |
| :--- | :--- | :--- | :--- |
| `type` | string | ✅ | Asset type (see table below) |
| `source` | string | ✅ | Source filename — must be in this same directory |
| `deps` | string[] | ✅ | Names of other assets this one depends on (max 8, omit extension) |

---

## Asset Types

| `type` value | Source file format | Raylib type loaded at runtime |
| :--- | :--- | :--- |
| `TEXTURE` | `.png`, `.jpg`, `.bmp`, `.tga` | `Texture2D` |
| `MODEL` | `.obj`, `.gltf`, `.glb` | `Model` |
| `FONT` | `.ttf`, `.otf` | `Font` |
| `SOUND` | `.wav`, `.ogg`, `.mp3` | `Sound` (⚠ disabled on PS2 — raudio not compiled in) |

---

## Dependencies

The `deps` array lists the **base names** of other assets that must be loaded before this one. The packer resolves them to full disc paths automatically:

```json
{
    "type":   "MODEL",
    "source": "ENEMY.OBJ",
    "deps":   ["ENEMY_TEX"]
}
```

This tells the runtime: when loading `ENEMY.ps2a`, also load `cdrom0:\RASSETS\ENEMY_TEX.PS2A;1` first and increment its reference count. The dependency is declared once in the JSON — no changes needed in game code.

- Maximum **8 dependencies** per asset.
- Dependency names are case-insensitive at authoring time; the packer uppercases them.
- Circular dependencies are not detected — avoid them.

---

## Naming Conventions

| Rule | Example |
| :--- | :--- |
| Base name = JSON name = output name | `BOX.JSON` → `BOX.ps2a` |
| Names are uppercased on disc (ISO 9660 Level 1) | `box.json` still produces `BOX.PS2A;1` |
| Source file must be in this directory | Do **not** use subdirectory paths in `source` |
| Dep names have **no extension** | `"deps": ["BOX"]` not `"deps": ["BOX.PS2A"]` |

---

## Binary `.ps2a` Layout

The packer writes a **2080-byte fixed header** followed by the raw source file bytes:

```
Offset  Size   Field
------  -----  -------------------------------------------
0       4      magic        = 0x50533241 ("PS2A" little-endian)
4       4      type         = 0=TEXTURE 1=MODEL 2=SOUND 3=FONT
8       1      depCount     = number of active dependency slots (0–8)
9       3      reserved     = 0x000000
12      16     ext          = source file extension e.g. ".jpg", ".png" (null-terminated)
28      2048   deps[8][256] = null-terminated disc paths for each dep
2076    4      dataSize     = byte count of the payload that follows
2080    N      <raw source file bytes>
```

The runtime reads this header in `EngineResource.c` (`Internal_OnAsyncLoadComplete`),
validates the magic, loads dependencies, then passes the payload to the matching
Raylib `*FromMemory` decoder.

---

## How to Pack

Packing runs **automatically during the CMake build**. To run it manually:

```bash
# From the project root (WSL / Linux)
python3 tools/pack_assets.py

# Override source/output directories
python3 tools/pack_assets.py --src game/cd_files/ASSETS --dst game/cd_files/rassets
```

---

## Accessing Assets at Runtime (C++)

```cpp
// game/src/Game.cpp
int handle = game::LoadResource("TEXTURE", game::MakePath("RASSETS\\BOX.PS2A"));

void GameUpdate(float dt)
{
    if (game::IsResourceReady(handle))
        game::DrawCubeTextured(0.0f, 0.0f, 0.0f, 2.0f, handle);
}
```

See `docs/RESOURCE_MANAGER.md` and `engine/include/GameAPI.h` for the full resource-loading API.

---

## Example: Adding a New Texture

1. Drop `WALL.PNG` into this directory.
2. Create `WALL.JSON`:
   ```json
   { "type": "TEXTURE", "source": "WALL.PNG", "deps": [] }
   ```
3. Build — the packer produces `rassets/WALL.ps2a`.
4. In C++: `game::LoadResource("TEXTURE", "cdrom0:\\RASSETS\\WALL.PS2A;1")`.

## Example: Textured Model with Dependency

1. Drop `CRATE.OBJ` and `CRATE_TEX.PNG` into this directory.
2. Create `CRATE_TEX.JSON`:
   ```json
   { "type": "TEXTURE", "source": "CRATE_TEX.PNG", "deps": [] }
   ```
3. Create `CRATE.JSON`:
   ```json
   { "type": "MODEL", "source": "CRATE.OBJ", "deps": ["CRATE_TEX"] }
   ```
4. In C++: `game::LoadResource("MODEL", "cdrom0:\\RASSETS\\CRATE.PS2A;1")` — the texture is loaded automatically as a dependency.
