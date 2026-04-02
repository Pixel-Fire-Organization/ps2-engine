# engine.get_time()

Returns the total elapsed time in seconds since the engine was initialized.

### Returns
| Type | Description |
| :--- | :--- |
| **number** | Seconds elapsed since `Engine_Init`. |

### Usage
```lua
local startTime = engine.get_time()
-- ... some work ...
local endTime = engine.get_time()
engine.log("It took " .. (endTime - startTime) .. " seconds")
```

### Notes
- High precision depends on the Emotion Engine's hardware timers indexed by `raylib`.
- Useful for delta-time calculations or event timing.
