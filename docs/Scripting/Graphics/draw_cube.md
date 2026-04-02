# graphics.draw_cube(pos_x, pos_y, pos_z, size, color)

Renders a 3D wireframe or solid cube in world space.

### Parameters
| Name | Type | Description |
| :--- | :--- | :--- |
| **pos_x** | `number` | The X coordinate of the cube's center. |
| **pos_y** | `number` | The Y coordinate of the cube's center. |
| **pos_z** | `number` | The Z coordinate of the cube's center. |
| **size** | `number` | The edge length of the cube. |
| **color** | `table` | `{r, g, b, a}` color table (values 0-255). |

### Usage
```lua
local pos = {0, 5.5, 10}
local my_color = {255, 0, 0, 255} -- Red
graphics.draw_cube(pos[1], pos[2], pos[3], 2.0, my_color)
```

### Notes
- Uses our underlying `raylib` 3D rendering pipeline.
- Faces are drawn using the Emotion Engine and GS directly.
