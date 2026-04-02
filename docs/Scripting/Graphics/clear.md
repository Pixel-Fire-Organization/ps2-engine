# graphics.clear(color)

Clears the entire screen (frame buffer) with the provided color.

### Parameters
| Name | Type | Description |
| :--- | :--- | :--- |
| **color** | `table` | `{r, g, b, a}` color table (values 0-255). |

### Usage
```lua
local sky_blue = {100, 150, 255, 255}
graphics.clear(sky_blue)
```

### Notes
- This is a high-speed clear operation executed by the Graphics Synthesizer (GS).
- Should be called at the start of every frame.
