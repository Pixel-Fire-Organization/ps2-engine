# Lua `level` — Level Management Bindings

The `level` table allows Lua scripts to load and unload game levels stored as `.ps2l` binary files.

> ⚠️ **These functions are currently stubs.** Calling them logs `"Not implemented"` and returns `false`. Implementation is pending `.ps2l` deserialisation (see `.ps2l` format below).

---

## API Reference

### `level.load(path) → bool`

Loads a level from a `.ps2l` file. When implemented:
1. Reads `LevelFileHeader` — validates `magic` (`"PS2L"`) and `version`.
2. Reads the packed `Level` struct directly into `ARENA_LEVEL_DATA`.
3. Calls `Engine_Level_Load` to pin all required resources.

- Returns `true` on success, `false` on failure or if not yet implemented.

```lua
local ok = level.load("cdrom0:\\LEVELS\\LEVEL1.PS2L;1")
if not ok then
    engine.log("Level load failed")
end
```

### `level.unload() → bool`

Unloads the current level, unpins required resources, and clears `ARENA_LEVEL_DATA`.

- Returns `true` on success, `false` on failure or if not yet implemented.

```lua
level.unload()
```

---

## `.ps2l` Binary File Format

Level files are binary-serialised `Level` structs with a fixed header guard.

### Header (`LevelFileHeader` — 8 bytes)

| Offset | Size | Field | Description |
| :--- | :--- | :--- | :--- |
| 0 | 4 | `magic` | `0x4C325350` — `"PS2L"` in little-endian (`LEVEL_FILE_MAGIC`) |
| 4 | 1 | `version` | File format version (`LEVEL_FILE_VERSION = 1`) |
| 5 | 3 | `reserved` | Padding |

### Payload (`Level` struct — packed)

Immediately follows the header. The `Level` struct is marked `__attribute__((packed))` to eliminate host/target padding divergence (e.g. when a PC-side level editor writes the file).

| Field | Type | Description |
| :--- | :--- | :--- |
| `name` | `char[64]` | Human-readable level name |
| `requiredResources` | `char[16][256]` | Paths of resources to pin on load |
| `requiredCount` | `uint32_t` | Number of active entries in `requiredResources` |

### Authoring

A level file can be written by any tool that:
1. Fills a `LevelFileHeader` with the correct magic and version.
2. Fills a `Level` struct (packed layout).
3. Writes both sequentially to a `.ps2l` file.
4. Places the file in `app/cd_files/` so the ISO build pipeline picks it up.

---

## Constants

| Constant | Value | Description |
| :--- | :--- | :--- |
| `LEVEL_FILE_MAGIC` | `0x4C325350` | `"PS2L"` LE — guards against corrupt reads |
| `LEVEL_FILE_VERSION` | `1` | Increment when `Level` struct changes |
| `LEVEL_FILE_EXT` | `".ps2l"` | Canonical file extension |
| `LEVEL_MAX_RESOURCES_COUNT` | `16` | Max pinned resources per level |

