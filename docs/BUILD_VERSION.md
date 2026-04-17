# Build Version

The engine and app each carry an independent, auto-incrementing `uint32_t`
build version that is embedded in every `main.elf` binary. The counters allow
any two builds to be unambiguously distinguished during debugging.

---

## Version headers

| File | Tracks | Macro |
|---|---|---|
| `engine/include/BuildEngineVersion.h` | Engine (`ps2_engine`) build counter | `ENGINE_BUILD_VERSION` |
| `app/include/BuildAppVersion.h` | App (`main.elf`) build counter | `APP_BUILD_VERSION` |

Both headers live in the repository and are committed to git. They serve
double duty as the **persistent counter** (git history) *and* the **C include**
consumed directly by the compiler — no separate plain-text file is needed.
Each contains a single `uint32_t` macro that starts at `0` and is incremented
by `1` only when the corresponding source files change.

---

## How it works

`scripts/increment_version.cmake` is a reusable CMake script that:

1. Reads the committed header file.
2. Extracts the current integer value from the `#define` line via regex.
3. Increments it by 1 and writes the updated header back in place.

Two CMake custom commands (one per component) use a **stamp file** and a
`DEPENDS` list to run the script only when source files have changed:

| Custom target | Triggered by | Header (read & written in place) | Macro |
|---|---|---|---|
| `increment_engine_version` | Any engine `.c` source change | `engine/include/BuildEngineVersion.h` | `ENGINE_BUILD_VERSION` |
| `increment_app_version` | `app/src/main.c` change | `app/include/BuildAppVersion.h` | `APP_BUILD_VERSION` |

Each command writes a stamp file (`build/engine_version.stamp` /
`build/app_version.stamp`) after a successful run. Make re-runs the command
only when a tracked source file is newer than the stamp, so the version counter
advances exclusively on code changes. `OBJECT_DEPENDS` on `EngineCore.c` /
`main.c` ensures those translation units are recompiled whenever their
respective header is regenerated, so the new version value is always linked
into the final binary.

---

## Embedded symbols

### Engine (`engine/src/EngineCore.c`)

```c
#include "BuildEngineVersion.h"
// ENGINE_BUILD_VERSION is a uint32_t macro; logged once at Engine_Init time.
Engine_LogInfo("Engine build version: %u", ENGINE_BUILD_VERSION);
```

### App (`app/src/main.c`)

```c
#include "BuildAppVersion.h"

static const uint32_t AppVersion = APP_BUILD_VERSION;

// Logged before EngineStart so it appears at the very top of the serial output.
Engine_LogInfo("App build version: %u", AppVersion);
```

`AppVersion` is a file-scope `static const` which guarantees the value is
placed in the ELF's `.rodata` section and is visible in the PCSX2 `.sym` file.

---

## Updating the counter

The counter advances automatically on every `cmake --build` invocation where
the corresponding source files have changed. No manual edits are required.
After a successful build, commit the updated `engine/include/BuildEngineVersion.h`
and/or `app/include/BuildAppVersion.h` so the repository always reflects the
highest build number that has been produced and the incremented versions are
preserved in the repository history.
