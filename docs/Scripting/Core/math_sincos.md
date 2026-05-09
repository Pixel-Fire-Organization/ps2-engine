# math.sincos(angle)

Computes both the **sine** and **cosine** of an angle in a **single C binding call**.

This is a PS2 engine extension to Lua's standard `math` library. It exists purely for
performance: the normal Lua idiom of `math.sin(a)` followed by `math.cos(a)` makes **two**
separate Lua→C FFI crossings.  `math.sincos` collapses that into **one** call, which is
particularly valuable inside tight per-object loops (e.g. animating 100+ primitives per
frame).

### Parameters

| Name      | Type     | Description       |
|:----------|:---------|:------------------|
| **angle** | `number` | Angle in radians. |

### Returns

| Position | Type     | Description  |
|:---------|:---------|:-------------|
| 1st      | `number` | `sin(angle)` |
| 2nd      | `number` | `cos(angle)` |

### Usage

```lua
-- Basic usage:
local s, c = math.sincos(angle)

-- Orbit example — replaces two separate calls:
local sin_a, cos_a = math.sincos(ang)
local x = base_x + radius * cos_a
local z = base_z + radius * sin_a

-- Double-angle identity — saves a third trig call for Lissajous paths:
-- sin(2*ang) = 2 * sin(ang) * cos(ang)
local sin_a, cos_a = math.sincos(ang)
local x = base_x + radius * (2 * sin_a * cos_a)   -- equivalent to sin(2 * ang)
local z = base_z + radius * sin_a
```

### Performance Notes

- On PS2, `lua_Number` is **`float`** (configured via `LUA_32BITS=1`) so all math
  operations run on the EE COP1 hardware FPU — single precision, full hardware speed.
- Localize `math.sincos` as an upvalue at the top of your script to eliminate the
  per-call hash-table traversal:
  ```lua
  local sincos = math.sincos   -- once at module/function level
  -- ... then inside the hot loop:
  local s, c = sincos(angle)
  ```
- For objects whose orbit uses **both** `sin(ang)` and `cos(ang)` of the same angle
  this binding is a strict improvement over two separate calls.
- For the cylinder case where you need `cos(ang)` and `sin(ang * 0.5)` (different
  angles), it still pays to use `sincos(ang)` to get `cos_ang`, but you still need a
  separate `sin(ang * 0.5)` call for the other axis.

### See Also

- Lua standard `math.sin`, `math.cos`
- `STRESSDRAW.LUA` — reference implementation showing all three optimization patterns

