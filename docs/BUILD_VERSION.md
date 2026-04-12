# Build Version

The engine and app each carry an independent, auto-incrementing `uint32_t`
build version that is embedded in every `main.elf` binary. The counters allow
any two builds to be unambiguously distinguished during debugging.

---

## Version files

| File | Tracks |
|---|---|
| `last_app_version.txt` | App (`main.elf`) build counter |
| `last_engine_version.txt` | Engine (`ps2_engine`) build counter |

Both files live in the repository root and are committed to Git. They contain
a single plain decimal integer. The value starts at `0` and is incremented
by `1` on every build.

---

## How it works

`scripts/increment_version.cmake` is a reusable CMake script that:

1. Reads the integer from the given version file.
2. Increments it by 1 and writes the new value back to the file.
3. Generates a C header in the CMake binary directory (`build/`) containing
   the `uint32_t` macro.

Two unconditional CMake custom targets (one per component) invoke this script
before the corresponding compilation unit is compiled:

| Custom target | Runs before | Version file | Generated header | Macro |
|---|---|---|---|---|
| `increment_engine_version` | `ps2_engine` library | `last_engine_version.txt` | `build/build_engine_version.h` | `ENGINE_BUILD_VERSION` |
| `increment_app_version` | `main.elf` executable | `last_app_version.txt` | `build/build_app_version.h` | `APP_BUILD_VERSION` |

Because the custom targets have no declared output, Make considers them
always out-of-date and runs them unconditionally — even when no source files
have changed. `OBJECT_DEPENDS` on `EngineCore.c` / `main.c` ensures those
translation units are recompiled whenever their respective header is
regenerated, so the new version value is always linked into the final binary.

---

## Embedded symbols

### Engine (`engine/src/EngineCore.c`)

```c
#include "build_engine_version.h"
// ENGINE_BUILD_VERSION is a uint32_t macro; logged once at Engine_Init time.
Engine_LogInfo("Engine build version: %u", ENGINE_BUILD_VERSION);
```

### App (`app/src/main.c`)

```c
#include "build_app_version.h"

static const uint32_t AppVersion = APP_BUILD_VERSION;

// Logged before EngineStart so it appears at the very top of the serial output.
Engine_LogInfo("App build version: %u", AppVersion);
```

`AppVersion` is a file-scope `static const` which guarantees the value is
placed in the ELF's `.rodata` section and is visible in the PCSX2 `.sym` file.

---

## Updating the counter

The counter advances automatically on every `cmake --build` invocation. No
manual edits are required. After a successful build, commit the updated
`last_app_version.txt` and/or `last_engine_version.txt` so the repository
always reflects the highest build number that has been produced.

---

## Generated files (not committed)

`build/build_app_version.h` and `build/build_engine_version.h` are written to
the CMake binary directory and are excluded from Git via `.gitignore` (`build/`
is already ignored). They are recreated automatically on every build.
