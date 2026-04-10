# `graphics.begin_mode_2d`

Activates 2D camera transform (scroll / zoom / rotation).  
All subsequent `draw_*` calls until the mode ends use the 2D camera's transform.

## Signature

```lua
graphics.begin_mode_2d(handle)
```

| Arg | Type | Description |
|-----|------|-------------|
| `handle` | `integer` | Handle returned by `graphics.make_camera_2d` |

## Behaviour

| Current mode | Elapsed since last entry | Result |
|---|---|---|
| `NONE` | — | Opens `BeginMode2D`, enters 2D mode |
| `3D` | any | Closes `EndMode3D`, opens `BeginMode2D`, enters 2D mode |
| `2D` | **< 1 second** | **No-op** (re-entry guard) |
| `2D` | **≥ 1 second** | Closes `EndMode2D`, reopens `BeginMode2D` (camera switch) |

The re-entry guard and eviction semantics are symmetric with `begin_mode_3d`.  
See [`begin_mode_3d.md`](begin_mode_3d.md) for the full rationale.

## Example

```lua
local hud_cam = graphics.make_camera_2d(0, 0, 0, 0, 0.0, 1.0)

function OnUpdate(dt)
    graphics.begin_mode_3d(world_cam)
    -- ... 3D drawing ...
    graphics.begin_mode_2d(hud_cam)  -- auto-closes 3D mode first
    graphics.draw_rect(10, 10, 100, 20, {255,255,255,255})
    -- mode is closed automatically before the debug overlay
end
```

