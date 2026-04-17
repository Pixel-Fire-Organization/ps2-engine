# Build Version

The engine and app each carry an independent, auto-incrementing `uint32_t`
build version that is embedded in every `main.elf` binary. The counters allow
any two builds to be unambiguously distinguished during debugging.

---

## Version headers

| File | Tracks | Macro |
|---|---|---|
| `engine/include/build_engine_version.h` | Engine (`ps2_engine`) build counter | `ENGINE_BUILD_VERSION` |
| `app/src/build_app_version.h` | App (`main.elf`) build counter | `APP_BUILD_VERSION` |

Both headers live in the repository and are committed to git. They serve
double duty as the **persistent counter** (git history) *and* the **C include**
consumed directly by the compiler — no separate plain-text file is needed.
Each contains a single `uint32_t` macro that starts at `0` and is incremented
by `1` on every build.

---

## How it works

`scripts/increment_version.cmake` is a reusable CMake script that:

1. Reads the committed header file.
2. Extracts the current integer value from the `#define` line via regex.
3. Increments it by 1 and writes the updated header back in place.

Two unconditional CMake custom targets (one per component) invoke this script
before the corresponding compilation unit is compiled:

| Custom target | Runs before | Header (read & written in place) | Macro |
|---|---|---|---|
| `increment_engine_version` | `ps2_engine` library | `engine/include/build_engine_version.h` | `ENGINE_BUILD_VERSION` |
| `increment_app_version` | `main.elf` executable | `app/src/build_app_version.h` | `APP_BUILD_VERSION` |

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
`engine/include/build_engine_version.h` and/or `app/src/build_app_version.h`
so the repository always reflects the highest build number that has been
produced and the incremented versions are preserved in the repository history.
