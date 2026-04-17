# ISO Generation

The build system assembles a bootable PS2 disc image (`.iso`) from the compiled ELF, a generated `SYSTEM.CNF`, and the
contents of `app/cd_files/` — minus any entries on the **ISO content blacklist**.

---

## How It Works

The `generate-iso` CMake target performs the following steps in order:

1. **Wipe** the staging directory (`<build>/iso_root/`) to eliminate stale files from previous builds.
2. **Pre-compile** Lua scripts — `compile-lua` runs `scripts/compile_lua.py`, which compiles every `.lua` file in
   `app/cd_files/` to a `.LUC` bytecode file in the same directory (see [Lua Pre-compilation](#lua-pre-compilation)).
3. **Copy** `app/cd_files/` into the staging directory, honouring the [ISO Content Blacklist](#iso-content-blacklist).
4. **Copy** the compiled ELF into the staging directory as `${APP_SERIAL}` (see [USERETAILNAME](#useretailname-option)
   below).
5. **Copy** the auto-generated `SYSTEM.CNF` into the staging directory.
6. **Run** `mkisofs` / `genisoimage` to produce `exec/<APP_ISO_NAME>.iso`.

> **Requirement**: `genisoimage` (which provides `mkisofs`) must be installed and on `PATH`.  
> If it is not found, `generate-iso` still exists as a no-op target so `build.sh --target generate-iso` does not fail.

---

## `USERETAILNAME` Option

Defined in the root `CMakeLists.txt`:

```cmake
option(USERETAILNAME "Changes the executable's name to the retail scheme." OFF)
```

| Value             | `APP_SERIAL` (ELF name on disc)                    | Volume label |
|:------------------|:---------------------------------------------------|:-------------|
| `OFF` *(default)* | `main.elf`                                         | *(not set)*  |
| `ON`              | `SLHB_000.00` *(or custom via `-DAPP_SERIAL=...`)* | `SLHB-00000` |

When **OFF**, the ELF is placed on the disc as `main.elf` and `SYSTEM.CNF` boots it by that name.  
When **ON**, the ELF is renamed to the PS2 serial format (`XXYY_NNN.NN`). This is required for OPL and retail disc
managers that parse the `BOOT2` field expecting a proper serial.

Enable it at configure time:

```sh
cmake -DUSERETAILNAME=ON -DAPP_SERIAL=SLES_508.77 ...
```

---

## `SYSTEM.CNF`

`SYSTEM.CNF` is **not** a static file. It is generated dynamically by `CMakeLists.txt` at configure time so that the
`BOOT2` path always reflects the current value of `APP_SERIAL` (e.g. `BOOT2 = cdrom0:\SLHB_000.00;1` when
`USERETAILNAME=ON`, or `BOOT2 = cdrom0:\main.elf;1` by default).

---

## Adding Assets to the Disc

Place any file or directory that must be present on the PS2 disc inside:

```
app/cd_files/
```

All contents are automatically staged into the ISO **unless** they are listed in
the [ISO Content Blacklist](#iso-content-blacklist).

> **Note**: Raw asset source files (JSON, PNGs, etc.) live in `app/cd_files/RAYLIB/`.  
> They are compiled to `.ps2a` by `pack-assets` and written to `app/cd_files/rassets/`.  
> Only `rassets/` needs to be on disc; `RAYLIB/` is excluded by the blacklist.

---

## Lua Pre-compilation

Lua scripts (`.lua`) placed in `app/cd_files/` are **pre-compiled** to bytecode (`.LUC`) by the `compile-lua` CMake
target before the ISO is assembled.  This step is handled by `scripts/compile_lua.py`.

### Why pre-compile?

| Benefit          | Detail                                                                               |
|:-----------------|:-------------------------------------------------------------------------------------|
| **Performance**  | The PS2 Lua VM loads bytecode directly without parsing or compiling source text.    |
| **Smaller disc** | Debug symbols (line numbers, variable names) are stripped via `luac -s`.            |
| **Fail-fast**    | Syntax errors are caught at build time, not at runtime on the PS2.                  |

### How it works

1. `scripts/compile_lua.py` scans `app/cd_files/` recursively for `.lua` files.
2. Each file is compiled to a `.LUC` sibling using `luac -s -o <name>.LUC <name>.lua`.
3. The `.lua` source files are excluded from the ISO by the blacklist (`*.LUA`, `*.lua`).
4. Only `.LUC` bytecode files reach the disc image and are read by the engine.

The engine entry point (`SCRIPTING_MAIN_SCRIPT_PATH` in `engine/include/Constants.h`) points to
`cdrom0:\MAIN.LUC;1`.

### Requirements

`luac` must be installed and on `PATH` (or the path passed to CMake via `-DLUAC_BIN`).  
It is provided by the standard Lua package on most distributions:

```sh
# Debian / Ubuntu
sudo apt install lua5.4

# macOS (Homebrew)
brew install lua
```

> **Compatibility**: The `luac` binary must produce bytecode compatible with the Lua version and word size used by the
> engine.  The PS2 EE processor is 32-bit little-endian.  A 64-bit host `luac` encodes `sizeof(size_t) = 8`, which the
> PS2 Lua VM will reject with *"size_t size mismatch"*.  Use a 32-bit `luac` build or a cross-compiled `luac` matching
> the engine's embedded Lua version to ensure runtime compatibility.

If `luac` is not found, `compile_lua.py` prints a warning and exits successfully so the rest of the build is not
blocked; however, the disc image will contain neither `.lua` source (excluded by blacklist) nor `.LUC` bytecode, and
the engine will fail to load the entry-point script at runtime.

---

## ISO Content Blacklist

The blacklist prevents specific files or directories inside `app/cd_files/` from being included in the disc image. It is
defined near the top of `app/CMakeLists.txt`:

```cmake
# ─────────────────────────────────────────────────────────────────────────────
# ISO Content Blacklist
# Add folder or file names (relative to cd_files/) that should NOT be included
# on the disc image.  RAYLIB/ holds raw asset sources — only the compiled
# .ps2a files produced by pack-assets and stored in rassets/ are needed at
# runtime.
# ─────────────────────────────────────────────────────────────────────────────
set(ISO_CONTENT_BLACKLIST
        "RAYLIB"   # Raw asset sources; packed output lives in rassets/
        "*.LUA"    # Lua source files; pre-compiled .LUC bytecode is used on disc instead
        "*.lua"    # Match lower-case variants on case-preserving filesystems
)
```

### Rules

| Rule                              | Detail                                                                                     |
|:----------------------------------|:-------------------------------------------------------------------------------------------|
| **Name-based matching**           | Each entry is matched against the file or directory name (not the full path).              |
| **Case-sensitive**                | Matching follows the filesystem on the build host (case-sensitive on Linux/WSL).           |
| **Applies at staging time**       | Exclusion happens when files are copied into `iso_root/`, before `mkisofs` runs.           |
| **Does not affect `pack-assets`** | The `pack-assets` target always reads from `cd_files/RAYLIB/` regardless of the blacklist. |

### Adding an Entry

Append the name to the list in `app/CMakeLists.txt`:

```cmake
set(ISO_CONTENT_BLACKLIST
        "RAYLIB"        # Raw asset sources
        "DEBUG_LOGS"    # Not needed at runtime
)
```

No other files need to be modified. The next build will exclude the new entry from the disc image.

---

## Implementation Detail

The copy-with-exclusion step is handled by `scripts/copy_iso_files.cmake`. It receives `SRC`, `DST`, and `BLACKLIST` as
CMake `-D` variables and uses CMake's built-in `file(COPY ... PATTERN ... EXCLUDE)` to apply the blacklist:

```cmake
# scripts/copy_iso_files.cmake
set(_exclude_args)
foreach (_item IN LISTS BLACKLIST)
    list(APPEND _exclude_args PATTERN "${_item}" EXCLUDE)
endforeach ()

file(COPY "${SRC}/" DESTINATION "${DST}" ${_exclude_args})
```

This keeps the logic self-contained and avoids any dependency on shell utilities.

---

## Debug Builds — `exec/iso_contents/`

When building with `DEBUG=ON` targeting PS2 (`CMAKE_SYSTEM_NAME STREQUAL "Generic"`), the staged ISO directory is
additionally copied to `exec/iso_contents/` after the ISO is generated. This lets you inspect exactly what ended up on
the disc without mounting the image.


