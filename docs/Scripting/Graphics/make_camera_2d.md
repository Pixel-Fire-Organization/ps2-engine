# `graphics.make_camera_2d`

Creates a 2D camera (scroll / zoom / rotation) and returns an integer handle.  
Camera data is stored in a fixed C-side registry (`SCRIPTING_MAX_CAMERAS_2D = 4` slots).

## Signature

```lua
local handle = graphics.make_camera_2d(
    offset_x, offset_y,   -- screen-space offset of the camera origin
    target_x, target_y,   -- world-space point at the centre of the view
    rotation,             -- rotation in degrees (clockwise)
    zoom                  -- zoom factor (1.0 = no zoom)
)
```

## Return value

| Value | Meaning |
|------:|---------|
| `>= 0` | Slot index (use as handle for `begin_mode_2d`) |
| `-1` | No free camera slots |

## Notes

- Slot eviction follows the same 1000-idle-frame rule as 3D cameras.
- Using an evicted handle with `begin_mode_2d` logs an error and is ignored.

## Example

```lua
local hud_cam = graphics.make_camera_2d(0, 0, 0, 0, 0.0, 1.0)
```

