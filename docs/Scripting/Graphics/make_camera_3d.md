# `graphics.make_camera_3d`

Creates a 3D perspective or orthographic camera and returns an integer handle.  
Camera data is stored in a fixed C-side registry (`SCRIPTING_MAX_CAMERAS_3D = 4` slots).

## Signature

```lua
local handle = graphics.make_camera_3d(
    pos_x,    pos_y,    pos_z,     -- camera world position
    target_x, target_y, target_z,  -- look-at point
    up_x,     up_y,     up_z,      -- up vector (usually 0,1,0)
    fovy,                           -- vertical field-of-view in degrees
    projection                      -- 0 = PERSPECTIVE, 1 = ORTHOGRAPHIC
)
```

## Return value

| Value | Meaning |
|------:|---------|
| `>= 0` | Slot index (use as handle for `begin_mode_3d`) |
| `-1` | No free camera slots — all `SCRIPTING_MAX_CAMERAS_3D` slots occupied |

## Notes

- Call **once at startup** and cache the handle in a `local` variable.
- The slot is marked **active** on creation; `Engine_Script_FrameTick` will
  evict it automatically after `SCRIPTING_CAM_IDLE_FRAMES_EVICT` (1000) frames
  without a matching `begin_mode_3d` call.
- If a handle is passed to `begin_mode_3d` after its slot has been evicted,
  an error is logged and the call is ignored.

## Example

```lua
local cam = graphics.make_camera_3d(
    0, 10, 20,   -- behind and above the origin
    0,  0,  0,   -- looking at the origin
    0,  1,  0,   -- Y-up
    45.0, 0      -- 45° FOV, perspective
)
```

