# Lua `resources` — Resource Manager Bindings

The `resources` table lets Lua scripts request and release Raylib-managed assets (textures, models, sounds, fonts). Lua holds opaque integer **handles** — all memory is owned by the Resource Manager inside Raylib's allocator.

---

## API Reference

### `resources.load(type, path) → handle`

Requests a resource from disc. Loading is **asynchronous** for textures, sounds, and fonts (streamed via `Engine_IO_ReadAsync`). Models are loaded synchronously.

| `type` string | Raylib type |
| :--- | :--- |
| `"TEXTURE"` | `Texture2D` |
| `"MODEL"` | `Model` |
| `"SOUND"` | `Sound` (disabled on PS2) |
| `"FONT"` | `Font` |

- Returns a `handle >= 0` on success (load queued or already cached), `-1` on failure.
- If the resource is already loaded, returns the existing handle without reloading.
- The resource file must be a compiled `.ps2a` asset (see `RESOURCE_MANAGER.md`).

```lua
local texHandle = resources.load("TEXTURE", engine.make_path("RASSETS\\PLAYER.PS2A"))
```

### `resources.is_ready(handle) → bool`

Returns `true` only when the resource has finished loading and is safe to use. Always check before accessing.

```lua
if resources.is_ready(texHandle) then
    -- resource is ready
end
```

### `resources.unload(handle)`

Explicitly unloads a resource. Decrements `refCount` on all its dependencies. Calls the matching Raylib `Unload*` function.

```lua
resources.unload(texHandle)
```

---

## Usage Pattern

```lua
-- Module-level: request load (may not be ready immediately)
local texHandle = resources.load("TEXTURE", engine.make_path("RASSETS\\HUD.PS2A"))

function OnUpdate(dt)
    if resources.is_ready(texHandle) then
        -- safe to use the handle via C-side graphics calls
    end
end
```

---

## Notes

- Handles are integers managed by the 64-slot Resource Manager table (see `RESOURCE_MANAGER.md`).
- When the table is full, LRU eviction occurs automatically for unpinned resources.
- `RES_SOUND` is disabled on PS2 (raudio module not compiled in). `resources.load("SOUND", ...)` will log an error and return `-1`.

