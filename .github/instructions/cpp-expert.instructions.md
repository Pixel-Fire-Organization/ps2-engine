# C++ Expert Instructions — PS2 Engine

## Standard

- **C++11** (`-std=c++11`). No exceptions (`-fno-exceptions`). No RTTI (`-fno-rtti`).
- No STL containers (`std::vector`, `std::string`, etc.) — they use heap and throw.
- `<cstdint>`, `<cstddef>`, `<cmath>`, `<cstring>` from the C++ standard library are permitted.

## Naming

| Element          | Convention              | Example                        |
|------------------|-------------------------|--------------------------------|
| Files            | `PascalCase.cpp` / `.h` | `RaylibRenderer.cpp`           |
| Classes          | `PascalCase`            | `RaylibRenderer`, `DrawLists`  |
| Methods          | `PascalCase_SnakeCase`  | `Engine_GetRenderer()`         |
| Local variables  | `camelCase`             | `uint32_t vertexCount;`        |
| Private members  | `m_camelCase`           | `m_drawLists`, `m_initialized` |
| Static/globals   | `PascalCase`            | `static int SlotCount;`        |
| Macros/constants | `UPPER_SNAKE_CASE`      | `GFX_MAX_DRAW_LIST_LENGTH`     |

## Enums

- Always use `enum class` — never plain `enum`. Scoped enums prevent namespace pollution and implicit int conversions.
  ```cpp
  // BAD
  enum Primitive3D { Cube, Sphere };

  // GOOD
  enum class Primitive3D : uint8_t { Cube, Sphere };
  ```
- Always specify an explicit underlying type (e.g. `: uint8_t`, `: uint32_t`) to control size and ABI.

## Class Design

- Abstract base classes (interfaces) are **header-only** — no `.cpp` file.
    - `virtual ~Base() = default;` in the header is sufficient.
    - Each public interface needs to be set with `= 0` for interface classes.
- Use `= delete` for copy/move constructors and assignment operators on non-copyable types.
- Always use `final` on leaf classes to help the compiler devirtualize.
- No `new`/`delete` — objects are either stack-allocated, arena-allocated, or static.
    - Exception: `EngineCore.cpp` uses `new` for the single `Renderer` instance (lifetime = process).

## Overloads vs Default Parameters

- **Prefer overloads to default parameters** for virtual methods and public API.
- Only one overload is the canonical one, so it contains the full implementation. Others call it with sensible default
  values.

## `auto`

- Use `auto` only when the type is **not** already written on the right-hand side.
    - ✅ `auto windowReadyBase = GetTime();` — type not obvious, saves repetition
    - ❌ `auto tex = static_cast<const Texture2D*>(ptr);` — defeats readability, write the type
- Never use `auto` for function parameters or return types.

## Casts

- Use `static_cast<T>` for safe numeric conversions.
- Use `reinterpret_cast<T>` only for hardware/DMA pointer aliasing.
- Never use C-style casts `(T)value` in new C++ code.

## Memory — CRITICAL for PS2

- Same rules as C: no `malloc`/`free`/`new`/`delete` in subsystem code.
- `__attribute__((aligned(16)))` on any buffer sent to GS/VIF/DMA.
- Struct members: **largest to smallest** to minimize padding.
- No `std::string` — use `char[]` with `snprintf`/`strncpy`.

## Designated Initializers

- **Forbidden in C++11.** Use plain member assignment instead:
  ```cpp
  // BAD  (C++20 / C99 only)
  Camera3D cam = { .position = {0,0,0}, .fovy = 45 };

  // GOOD (C++11)
  Camera3D cam;
  cam.position   = Vector3{0.0f, 0.0f, 0.0f};
  cam.fovy       = 45.0f;
  ```

## Clang-Format

The repo `.clang-format` is authoritative. **ALWAYS** follow it.

- **Never** run clang-format over `external/`.

## Graphics Subsystem Rules

- `rlBegin` / `rlEnd` must wrap the **entire batch** for a primitive type — one pair per frame.
    - Never call `rlPushMatrix`/`rlPopMatrix` inside a `rlBegin`/`rlEnd` block.
- Per-object transforms are applied **on the CPU** (build rotation matrix once, apply to all vertices).
- `BeginMode3D` / `EndMode3D` / `BeginMode2D` / `EndMode2D` are owned exclusively by `DrawLists::Render()`.
    - No other code may call these directly.
- The render order each frame is: Skybox → BeginMode3D → Primitives → Models → EndMode3D → BeginMode2D → UI → EndMode2D.

## Pure Virtual / Abstract Classes

- A class is abstract if it has at least one pure virtual method.
- The destructor of an abstract class should be `virtual ~Base() = default;` in the header.
- **No `.cpp` file** for pure-abstract classes — all method bodies are either `= 0` or inline in the header.

## Constants

- All engine-wide constants live in `engine/include/Constants.h` (split by category `Constants.XXX.h`).
- Include `Constants.h` (the umbrella) — never individual `Constants.XXX.h` directly.

## Comments

- Comment the **why**, not the what. Do that if the problem cannot be understood at a glance.
- Keep comments at a minimum.
- Never comment when removing a block **why** you removed it.
- Single-line comments only for inline annotation.
- Multi-line block comments for section headers.