# graphics.draw_rect(x, y, width, height, color)

Draws a 2D rectangle at the specified screen coordinates.

### Parameters
| Name | Type | Description |
| :--- | :--- | :--- |
| **x** | `number` | Left position in pixels. |
| **y** | `number` | Top position in pixels. |
| **width** | `number` | Width in pixels. |
| **height** | `number` | Height in pixels. |
| **color** | `table` | `{r, g, b, a}` color table (values 0-255). |

### Usage
```lua
local my_rect = {10, 10, 100, 50}
local red = {255, 0, 0, 255}
graphics.draw_rect(my_rect[1], my_rect[2], my_rect[3], my_rect[4], red)
```

### Notes
- Coordinate system is based on the PS2 screen resolution configured at `Engine_Init`.
- Highly efficient for UI elements.
