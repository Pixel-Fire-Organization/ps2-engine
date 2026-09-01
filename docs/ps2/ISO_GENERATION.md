# ISO Generation

The build system assembles a bootable PS2 disc image (`.iso`) from the compiled ELF, a generated `SYSTEM.CNF`, and the
contents of `game/cd_files/` — minus any entries on the **ISO content blacklist**.

---

## How It Works

The per-platform `iso-<platform>` CMake target (`iso-ps2pal`, `iso-ps2ntsc`) performs the following steps in order:

1. **Wipe** the staging directory (`<build>/iso_root/`) to eliminate stale files from previous builds.
2. **Copy** `game/cd_files/` into the staging directory, honouring the [ISO Content Blacklist](#iso-content-blacklist).
3. **Copy** the compiled ELF into the staging directory as `${APP_SERIAL}` (see [USERETAILNAME](#useretailname-option)
   below).
4. **Copy** the auto-generated `SYSTEM.CNF` into the staging directory.
5. **Run** `mkisofs` / `genisoimage` to produce `dist/<APP_ISO_NAME>.iso`.

> **Requirement**: `genisoimage` (which provides `mkisofs`) must be installed and on `PATH`.  
> If it is not found, the target still exists as a no-op so `python3 tools/build.py` does not fail.

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
game/cd_files/
```

All contents are automatically staged into the ISO **unless** they are listed in
the [ISO Content Blacklist](#iso-content-blacklist).

> **Note**: Raw asset source files (JSON, PNGs, etc.) live in `game/cd_files/ASSETS/`.  
> They are cooked to `.ps2a` by `cook-<platform>` and written to `dist/cooked/<platform>/rassets/`.  
> Only `rassets/` needs to be on disc; `ASSETS/` is excluded by the blacklist.

---

## ISO Content Blacklist

The blacklist prevents specific files or directories inside `game/cd_files/` from being included in the disc image. It is
defined near the top of `game/CMakeLists.txt`:

```cmake
# ─────────────────────────────────────────────────────────────────────────────
# ISO Content Blacklist
# Add folder or file names (relative to cd_files/) that should NOT be included
# on the disc image.  ASSETS/ holds raw asset sources — only the compiled
# .ps2a files produced by the cook stage are needed at
# runtime.
# ─────────────────────────────────────────────────────────────────────────────
set(ISO_CONTENT_BLACKLIST
        "ASSETS"   # Raw asset sources; packed output lives in rassets/
)
```

### Rules

| Rule                              | Detail                                                                                     |
|:----------------------------------|:-------------------------------------------------------------------------------------------|
| **Name-based matching**           | Each entry is matched against the file or directory name (not the full path).              |
| **Case-sensitive**                | Matching follows the filesystem on the build host (case-sensitive on Linux/WSL).           |
| **Applies at staging time**       | Exclusion happens when files are copied into `iso_root/`, before `mkisofs` runs.           |
| **Does not affect cooking** | The `cook-<platform>` target always reads from `cd_files/ASSETS/` regardless of the blacklist. |

### Adding an Entry

Append the name to the list in `game/CMakeLists.txt`:

```cmake
set(ISO_CONTENT_BLACKLIST
        "ASSETS"        # Raw asset sources
        "DEBUG_LOGS"    # Not needed at runtime
)
```

No other files need to be modified. The next build will exclude the new entry from the disc image.

---

## Implementation Detail

The copy-with-exclusion step is handled by `tools/copy_iso_files.cmake`. It receives `SRC`, `DST`, and `BLACKLIST` as
CMake `-D` variables and uses CMake's built-in `file(COPY ... PATTERN ... EXCLUDE)` to apply the blacklist:

```cmake
# tools/copy_iso_files.cmake
set(_exclude_args)
foreach (_item IN LISTS BLACKLIST)
    list(APPEND _exclude_args PATTERN "${_item}" EXCLUDE)
endforeach ()

file(COPY "${SRC}/" DESTINATION "${DST}" ${_exclude_args})
```

This keeps the logic self-contained and avoids any dependency on shell utilities.

---

## Debug Builds — `dist/iso_contents/`

When building with `DEBUG=ON` targeting PS2 (`CMAKE_SYSTEM_NAME STREQUAL "Generic"`), the staged ISO directory is
additionally copied to `dist/iso_contents/` after the ISO is generated. This lets you inspect exactly what ended up on
the disc without mounting the image.


