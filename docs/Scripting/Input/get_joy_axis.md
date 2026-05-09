# input.get_joy_axis(port, joystick)

Returns the current analogue axis values of a joystick as **two separate numbers**,
without allocating a Lua table.

This is a zero-allocation alternative to `input.get_joy_status`. Use it in `OnUpdate`
to avoid per-frame heap pressure on the 512 KB Lua script heap.

### Parameters

| Name         | Type      | Description                           |
|:-------------|:----------|:--------------------------------------|
| **port**     | `integer` | Range: [0; 1]. Controller port index. |
| **joystick** | `string`  | `"left"` or `"right"`.                |

### Returns

| Position | Type     | Description                               |
|:---------|:---------|:------------------------------------------|
| 1st      | `number` | X-axis value in the range `[-1.0, +1.0]`. |
| 2nd      | `number` | Y-axis value in the range `[-1.0, +1.0]`. |

On any error (invalid port, unknown joystick name) both values are `0`.

### Usage

```lua
-- Zero-allocation joystick read (preferred in OnUpdate hot path):
local lj_x, lj_y = input.get_joy_axis(0, "left")
cubeX = cubeX + lj_x * SPEED * dt
cubeZ = cubeZ + lj_y * SPEED * dt

local rj_x, rj_y = input.get_joy_axis(0, "right")
camYaw   = camYaw   + rj_x * CAM_SPEED * dt
camPitch = camPitch + rj_y * CAM_SPEED * dt
```

### Comparison With get_joy_status

|                     | `get_joy_status`              | `get_joy_axis`       |
|:--------------------|:------------------------------|:---------------------|
| Return type         | `{x, y}` table or `nil`       | Two `number` values  |
| Lua heap allocation | **Yes** — 1 table per call    | **None**             |
| Nil guard required  | Yes (`if lj then`)            | No                   |
| Use case            | Occasional reads, legacy code | Hot `OnUpdate` loops |

### Notes

- Dead-zone normalization is applied in C (`NormalizeAxis` / `INPUT_ANALOG_DEADZONE`).
  The returned values are already in the `[-1, +1]` range with the dead zone zeroed.
- Deadzone and normalization are **not** applied by the Lua script.
- If the port is out of range (`< 0` or `>= MAX_GAME_PAD_PORTS`) an error is logged and
  `0, 0` is returned.

### See Also

- `input.get_joy_status` — legacy table-based joystick read
- `input.is_pad_pressed` — digital button check

