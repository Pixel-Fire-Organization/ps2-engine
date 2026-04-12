# `graphics.update_camera_3d`

Updates the **position** and **look-at target** of an existing 3D camera slot in-place.  
The up vector, field-of-view, and projection type are preserved.

Call this every frame (before `begin_mode_3d`) when driving the camera with analogue input
or any other per-frame computation.

## Signature

```lua
graphics.update_camera_3d(
    handle,                        -- integer handle returned by make_camera_3d
    pos_x,   pos_y,   pos_z,      -- new camera world position
    target_x, target_y, target_z  -- new look-at point
)
```

## Parameters

| Parameter      | Type    | Description                              |
|----------------|---------|------------------------------------------|
| `handle`       | integer | Camera slot handle from `make_camera_3d` |
| `pos_x/y/z`    | number  | New camera world-space position          |
| `target_x/y/z` | number  | New look-at target (world-space)         |

## Return value

None. Logs an error and does nothing if `handle` is invalid or the slot has been evicted.

## Notes

- The slot's **last-used frame counter** is refreshed on each call, preventing idle eviction.
- Does **not** re-enter or re-exit 3D mode; call `begin_mode_3d` after updating.

## Example — spherical orbit camera controlled by the right joystick

```lua
local camYaw   = 0.0   -- horizontal angle (radians)
local camPitch = 0.4   -- vertical angle   (radians)
local CAM_DIST = 20.0
local CAM_SPEED = 1.8
local DEAD_ZONE = 0.12

function OnUpdate(dt)
    local rj = input.get_joy_status(0, "right")
    if rj then
        if math.abs(rj.x) > DEAD_ZONE then camYaw   = camYaw   + rj.x * CAM_SPEED * dt end
        if math.abs(rj.y) > DEAD_ZONE then camPitch  = camPitch + rj.y * CAM_SPEED * dt end
    end

    -- Clamp pitch so the camera never flips over
    if camPitch > 1.45 then camPitch = 1.45 end
    if camPitch < 0.05 then camPitch = 0.05 end

    -- Spherical → Cartesian, orbiting around target (tx, ty, tz)
    local cp = math.cos(camPitch)
    local cx = tx + CAM_DIST * cp * math.sin(camYaw)
    local cy = ty + CAM_DIST * math.sin(camPitch)
    local cz = tz + CAM_DIST * cp * math.cos(camYaw)

    graphics.update_camera_3d(cam3d, cx, cy, cz, tx, ty, tz)
    graphics.begin_mode_3d(cam3d)
    -- ... draw scene ...
end
```

