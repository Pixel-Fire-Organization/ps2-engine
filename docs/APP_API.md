# App API (`EngineApp`)

The `EngineApp` module is the **only** interface the application layer is permitted to include. All engine subsystems (memory arenas, async I/O, resource streaming, Lua scripting) are driven internally. The app shell is intentionally minimal: it starts the engine, pumps the frame loop, and stops.

---

## Enforcement

Controlled by a CMake option in `engine/CMakeLists.txt`:

```cmake
option(ENGINE_SANDBOX_MODE "Restrict app to EngineApp.h" ON)
```

| Mode | App include path | Visible headers |
| :--- | :--- | :--- |
| `ON` (default) | `engine/include/app_public/` | `EngineApp.h` only |
| `OFF` (dev/test) | `engine/include/` | Full engine surface |

---

## API Reference

### `EngineStart(const char *mainScript) → bool`

Initialises all engine subsystems, synchronously reads and executes the entry-point Lua script.

- If `mainScript` is `NULL`, uses the canonical path defined by `SCRIPTING_MAIN_SCRIPT_PATH` in `Constants.h` (`"cdrom0:\\MAIN.LUA;1"`).
- Returns `false` if engine init, file read, or Lua execution fails.

### `EngineUpdate(void)`

Advances one frame:
1. `BeginDrawing()`
2. Calls Lua `OnUpdate(dt)` on all active script units.
3. Renders the debug overlay.
4. `EndDrawing()`
5. Pumps async I/O and the resource manager.

Must be called every iteration of the main loop.

### `EngineExited(void) → bool`

Returns `true` when the engine should stop — either because the window was closed or Lua called `engine.exit()`.

### `EngineStop(void)`

Shuts down all subsystems and releases all memory. Call once after the main loop exits.

---

## Logging & Panic

`EngineApp.h` re-declares three functions from `EngineDebug.c` as `extern` prototypes, making them available to the app without transitively including `EngineDebug.h` or `raylib.h`:

```c
extern void Engine_LogInfo(const char *text, ...);
extern void Engine_LogError(const char *text, ...);
extern void Engine_Panic(const char *message);
```

---

## Minimal `main.c`

```c
#include "EngineApp.h"

int main(void) {
    if (!EngineStart(NULL)) return -1;
    while (!EngineExited()) EngineUpdate();
    EngineStop();
    return 0;
}
```

---

## Internal Flow

```
EngineStart(path)
  └─ Engine_Init()           — arenas, pool, window, IO, resources, scripts
  └─ Engine_Script_SetExitCallback(EngineApp_OnExitRequested)
  └─ EngineApp_FileOpen(path)  — reads script into ARENA_CONFIG slot 0
  └─ Engine_Script_Load()
  └─ Engine_Script_Run()
  └─ EngineApp_FileClose()   — slot 0 released

EngineUpdate()
  └─ BeginDrawing()
  └─ Engine_Script_UpdateAll(dt)  — Lua OnUpdate(dt)
  └─ Engine_DrawDebugOverlay()
  └─ EndDrawing()
  └─ Engine_IO_Update()
  └─ Engine_Resource_Update()
```

