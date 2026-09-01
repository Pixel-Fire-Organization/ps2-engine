# App API (`EngineApp`)

`EngineApp` is the engine's own shell: it brings the subsystems up, advances a frame, and tears
them down. All engine subsystems (memory arenas, async I/O, resource streaming, rendering) are
driven internally.

**Gameplay is authored against `GameAPI.h`** (`GameConfigure(config)` / `GameInit()` / `GameUpdate(dt)`) — that is the
surface game code should reach for; see `engine/include/GameAPI.h`.

---

## No sandbox — convention, not a fence

Game code once had a curated include fence: `engine/include/app_public/` held forwarding shims and
a CMake option (`ENGINE_SANDBOX_MODE`) restricted the app target's include path to that directory
alone. Both are **gone**.

That fence existed to keep a *scripted* app layer out of engine internals. With Lua removed and
gameplay written natively in C++, the shims were forwarding headers whose entire body was
`#include "../GameAPI.h"`, and the option existed only to make that indirection load-bearing.

Game code now uses the normal engine headers. The rule is unchanged, it is simply a convention
rather than a build error:

> Game code should include `GameAPI.h`. Reaching into `EngineMemory.h`, `EngineIO.h`,
> `EngineResource.h` or the renderer from `game/**` means the API is missing something — extend
> `GameAPI.h` rather than bypassing it.

---

## Input

`GameAPI.h` exposes three separately named device groups, mirroring the platform layer:

| Group | Functions |
| :--- | :--- |
| Gamepad | `IsPadPressed`, `WasPadPressed`, `GetJoyAxis` |
| Keyboard | `IsKeyDown`, `WasKeyPressed` |
| Mouse | `IsMouseButtonDown`, `WasMouseButtonPressed`, `GetMousePosition`, `GetMouseDelta`, `GetMouseWheel` |

All of them compile and run everywhere. On a platform with no keyboard or mouse (the PS2) the corresponding calls
return false/zero rather than pretending. Branch on `game::HasInputDevice("keyboard")`, never on which platform is
running.

Pad-only game code needs no change to be playable on desktop: Win32 maps a default keyboard layout onto virtual pad 0
(see `docs/PLATFORMS.md`).

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

`EngineApp.h` includes `EngineDebug.h`, so logging and panic come with it:

```c
void Engine_LogInfo(const char *text, ...);
void Engine_LogError(const char *text, ...);
void Engine_Panic(const char *message);
```

It previously hand-copied these three as `extern` prototypes to avoid pulling in `EngineDebug.h`
"and transitively raylib.h". raylib is gone and so is the sandbox, so both reasons for the
duplicate declarations are void — and a duplicated prototype is a prototype that can drift.

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

void GameConfigure(EngineConfig* config) { /* choose subsystems - see EngineSubsystems.h */ }
void GameInit() { /* load resources, set initial state */ }
void GameUpdate(float dt) { /* input, animation, game::Draw*, game::SetCamera3D, ... */ }
```

See `game/src/Game.cpp` and `game/src/SwarmSystem.cpp` for a complete example.

---

## Internal Flow

```
EngineStart(token)
  └─ Engine_Init()   — arenas, pool, renderer, pad, IO, resources
  └─ GameConfigure(config)       — choose subsystems, before the engine exists
  └─ GameInit()

EngineUpdate()
  └─ Engine_Update()             — dt, IO/resource pumps
  └─ GameUpdate(dt)              — gameplay + draw-list submission
  └─ Renderer BeginFrame/Render/EndFrame
  └─ Engine_ReportFrameStats()
  └─ Engine_PerfLogger_Tick()
```
