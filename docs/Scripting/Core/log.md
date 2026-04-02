# engine.log(message)

Outputs a text message to the PS2 internal debug log or serial console.

### Parameters
| Name | Type | Description |
| :--- | :--- | :--- |
| **message** | `string` | The message to output to the console. |

### Usage
```lua
engine.log("Hello from PlayStation 2!")
```

### Notes
- In `DEBUG` mode, these messages are often visible via standard terminal output or the PS2 serial port.
- Overusing this in production cycles can impact performance due to string formatting overhead.
