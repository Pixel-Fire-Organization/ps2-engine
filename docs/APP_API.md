# App API (`EngineApp`)

The `EngineApp` module (plus `GameAPI.h`) is the **only** interface the application layer is
permitted to include. All engine subsystems (memory arenas, async I/O, resource streaming, rendering)
are driven internally. Gameplay is authored in C++ against the friendly `GameAPI` (`GameInit()` /
`GameUpdate(dt)`); the app shell itself is intentionally minimal: it starts the engine, pumps the
frame loop, and stops.

---

## Enforcement

Controlled by a CMake option in `engine/CMakeLists.txt`:

```cmake
option(ENGINE_SANDBOX_MODE "Restrict app to the curated app_public headers" ON)
```

| Mode | App include path | Visible headers |
| :--- | :--- | :--- |
| `ON` (default) | `engine/include/app_public/` | `EngineApp.h`, `GameAPI.h` |
| `OFF` (dev/test) | `engine/include/` | Full engine surface |

---

## API Reference

### `EngineStart(const char *resourceLocationToken) → bool`

Initialises all engine subsystems (arenas, renderer, input, IO, resources), then calls the game
module's `GameInit()` once.

- `resourceLocationToken` selects the active storage device (`"cdrom0:"`, `"mass0:"`, `"hdd0:"`,
  `"host:"`); pass `NULL` to default to `cdrom0:`.
- Returns `false` if engine init fails.

### `EngineUpdate(void)`

Advances one frame:
1. `Engine_Update()` — timing, async IO/resource pumps.
2. `GameUpdate(dt)` — the game module's per-frame gameplay + draw submission.
3. Renderer `BeginFrame()` / `Render()` / debug overlay / `EndFrame()`.
4. Frame stats reporting + perf logger tick.

Must be called every iteration of the main loop.

### `EngineExited(void) → bool`

Returns `true` when the engine should stop — set by `game::Exit()`.

### `EngineStop(void)`

Shuts down all subsystems and releases all memory. Call once after the main loop exits.

---

## Logging & Panic

`EngineApp.h` re-declares three functions from `EngineDebug.c` as `extern` prototypes, making them
available to the app without transitively including `EngineDebug.h`:

```c
extern void Engine_LogInfo(const char *text, ...);
extern void Engine_LogError(const char *text, ...);
extern void Engine_Panic(const char *message);
```

---

## Minimal `main.cpp`

```cpp
#include "EngineApp.h"

int main(void) {
    if (!EngineStart(NULL)) return -1;
    while (!EngineExited()) EngineUpdate();
    EngineStop();
    return 0;
}
```

Gameplay itself lives in a separate game module implementing the `GameAPI.h` entry points:

```cpp
#include "GameAPI.h"

void GameInit() { /* load resources, set initial state */ }
void GameUpdate(float dt) { /* input, animation, game::Draw*, game::SetCamera3D, ... */ }
```

See `game/src/Game.cpp` and `game/src/SwarmSystem.cpp` for a complete example.

---

## Internal Flow

```
EngineStart(token)
  └─ Engine_Init()   — arenas, pool, renderer, pad, IO, resources
  └─ GameInit()

EngineUpdate()
  └─ Engine_Update()             — dt, IO/resource pumps
  └─ GameUpdate(dt)              — gameplay + draw-list submission
  └─ Renderer BeginFrame/Render/EndFrame
  └─ Engine_ReportFrameStats()
  └─ Engine_PerfLogger_Tick()
```
