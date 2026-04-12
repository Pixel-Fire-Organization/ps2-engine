# input.is_pad_pressed(button)

Checks if the specified physical button on the PS2 Controller is currently being held.

### Parameters

| Name       | Type      | Description                                                                                             |
|:-----------|:----------|:--------------------------------------------------------------------------------------------------------|
| **port**   | `integer` | Range: [0; 1]. Used to target a specific controller port. This port needs to be initialized beforehand. |
| **button** | `string`  | See table #1.                                                                                           |

#### Table 1

| Name       | Description       |
|:-----------|:------------------|
| `"tri"`    | Triangle button   |
| `"squ"`    | Square button     |
| `"cir"`    | Circle button     |
| `"x"`      | Cross button      |
| `"l1"`     | L1 button         |
| `"l2"`     | L2 button         |
| `"l3"`     | L3 button         |
| `"r1"`     | R1 button         |
| `"r2"`     | R2 button         |
| `"r3"`     | R3 button         |
| `"up"`     | DPad Up button    |
| `"down"`   | DPad Down button  |
| `"left"`   | DPad Left button  |
| `"right"`  | DPad Right button |
| `"select"` | Select button     |
| `"start"`  | Start button      |

### Returns

| Type      | Description                                         |
|:----------|:----------------------------------------------------|
| `boolean` | `true` if the button is pressed, `false` otherwise. |

### Usage

```lua
if input.is_pad_pressed(0, "x") then
    engine.log("Jump pressed!")
end
```

### Notes

- This uses the engine's low-level `pad` drivers compiled for the EE.
- If a port is specified that isn't initialized or out of range, an error is logged and `false` is returned.
- Button strings are required to be lowercase.
