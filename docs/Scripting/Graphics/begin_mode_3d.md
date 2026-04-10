# `graphics.begin_mode_3d`

Activates 3D projection using the specified camera.  
All subsequent `draw_*` calls until the mode ends are rendered in 3D world space.

## Signature

```lua
graphics.begin_mode_3d(handle)
```

| Arg | Type | Description |
|-----|------|-------------|
| `handle` | `integer` | Handle returned by `graphics.make_camera_3d` |

## Behaviour

| Current mode | Elapsed since last entry | Result |
|---|---|---|
| `NONE` | — | Opens `BeginMode3D`, enters 3D mode |
| `2D` | any | Closes `EndMode2D`, opens `BeginMode3D`, enters 3D mode |
| `3D` | **< 1 second** | **No-op** (re-entry guard) |
| `3D` | **≥ 1 second** | Closes `EndMode3D`, reopens `BeginMode3D` (camera switch) |

### Re-entry guard

Calling `begin_mode_3d` while already in 3D mode within 1 second of the last
entry is silently ignored. This prevents accidental double-calls from creating
expensive `EndMode3D` / `BeginMode3D` GPU round-trips on ps2gl.

After 1 second the call is allowed through, enabling deliberate mid-frame camera
switches (e.g. split-screen or cinematic cuts).

### End mode

`Engine_Script_EndCurrentMode()` is called automatically by `EngineApp` between
`Engine_Script_UpdateAll` and `Engine_DrawDebugOverlay`, so the debug overlay
always renders in flat 2D.  
There is **no `end_mode_3d` Lua function** — you never need to call one explicitly.

### Evicted handles

If the handle's slot has been evicted by the camera LFU system, the call is
rejected with an error log. Re-create the camera with `make_camera_3d`.

## Example

```lua
local cam = graphics.make_camera_3d(0,10,20, 0,0,0, 0,1,0, 45.0, 0)

function OnUpdate(dt)
    graphics.clear({20,20,20,255})
    graphics.begin_mode_3d(cam)
    graphics.draw_grid(100, 1.0)
    graphics.draw_cube(0, 0, 0, 2.0, {255,0,0,255})
    -- mode is closed automatically before the debug overlay
end
```

