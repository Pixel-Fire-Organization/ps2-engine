# input.is_pad_pressed(button)

Checks if the specified physical button on the PS2 Controller is currently being held.

### Parameters
| Name | Type | Description |
| :--- | :--- | :--- |
| **button** | `string` | `"tri"`, `"squ"`, `"cir"`, `"x"`, `"l1"`, `"r1"`, `"up"`, `"down"`, etc. |

### Returns
| Type | Description |
| :--- | :--- |
| `boolean` | `true` if the button is pressed, `false` otherwise. |

### Usage
```lua
if input.is_pad_pressed("x") then
    engine.log("Jump pressed!")
end
```

### Notes
- This uses the engine's low-level `pad` drivers compiled for the EE.
- Recommended button strings are lowercase.
