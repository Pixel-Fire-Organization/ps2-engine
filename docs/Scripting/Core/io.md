# Lua `io` — File I/O Bindings

The `io` table provides file I/O to Lua scripts. All file data is managed C-side inside `ARENA_CONFIG` slots. Lua holds only opaque integer **fileIds** (descriptors). This mirrors the POSIX file-descriptor model.

---

## PS2 Path Format Rules

The PS2 filesystem drivers process paths differently per device. Using the wrong separator causes silent open failures.

| Device | Separator | Correct example |
| :--- | :--- | :--- |
| `cdrom0:` | `\` (backslash), ISO 9660 version suffix `;1` | `"cdrom0:\\RASSETS\\BOX.PS2A;1"` |
| `host0:` | **none** — no separator after the colon | `"host0:SAVE.TXT"` |
| `mc0:` | `\` (backslash) | `"mc0:\\SAVE.BIN"` |

> **Note**: In Lua strings, `\\` is a single backslash character.  
> `host0:\SAVE.TXT` is **wrong** — the PS2 host filesystem driver converts `\` to `/`, producing `host0:/SAVE.TXT` which fails to open.

---

## Open Modes

| Function | Mode | Allocates arena slot? |
| :--- | :--- | :--- |
| `io.open(path)` | Read | Yes — file is read into a config slot |
| `io.open_write(path)` | Write | No — only the path is registered |

---

## API Reference

### `io.open(path) → fileId`

Opens a file for reading. Synchronously reads the entire file into an `ARENA_CONFIG` slot.

- Returns a `fileId >= 0` on success, `-1` on failure (file not found, slot exhausted, or file exceeds `APP_MAX_FILE_DATA_SIZE`).
- **Intended for small files only**: config, dialogue, scripts. Binary assets must go through `resources.load`.

```lua
local fd = io.open("cdrom0:\\CONFIG.TXT;1")
if fd >= 0 then
    -- use fd ...
    io.close(fd)
end
```

### `io.open_write(path) → fileId`

Registers a path for writing. No disc read or arena allocation occurs.

- Returns a `fileId >= 0` on success, `-1` if no descriptor slots are available.

```lua
local fd = io.open_write("host0:SAVE.TXT")
```

### `io.close(fileId) → bool`

Releases the file descriptor. For READ descriptors, clears the underlying `ARENA_CONFIG` slot.

- Returns `false` if `fileId` is not found in the descriptor table.

### `io.get_size(fileId) → number`

Returns the byte size of the file read into this descriptor (READ mode only). Returns `0` for WRITE descriptors or invalid fileIds.

### `io.read(fileId) → string, number`

Copies the C-side file data into a Lua string and returns it alongside the byte count.

- Returns `nil, 0` on failure (wrong mode, invalid fileId, empty file).
- **Scope warning**: only use for small text files. Large reads pressure `ARENA_SCRIPT` (Lua's heap) with a full data copy.

```lua
local content, bytes = io.read(fd)
if bytes > 0 then
    engine.log("Read " .. bytes .. " bytes")
end
```

### `io.write(fileId, content) → number`

Writes the string `content` to disc at the path registered by `io.open_write`.

- Returns bytes written on success.
- Returns `0` on failure (write error).
- **Guard**: if called on a READ descriptor, logs an error and returns `0` — use `io.open_write` for write operations.

```lua
local written = io.write(fd, "save data\n")
if written == 0 then
    engine.log("Write failed")
end
```

---

## Full Read/Write Example

```lua
-- Read a config file
local rfd = io.open("cdrom0:\\CONFIG.TXT;1")
if rfd >= 0 then
    local text, bytes = io.read(rfd)
    if bytes > 0 then engine.log(text) end
    io.close(rfd)
end

-- Write a save marker
local wfd = io.open_write("host0:SAVE.TXT")
if wfd >= 0 then
    local n = io.write(wfd, "progress=1\n")
    engine.log("Wrote " .. n .. " bytes")
    io.close(wfd)
end
```

---

## Limits

| Constant | Value | Description |
| :--- | :--- | :--- |
| `APP_MAX_FILE_SLOTS` | 4 | Max simultaneously open descriptors |
| `APP_MAX_FILE_DATA_SIZE` | 256 KB | Max file size for a single `io.open` |

