# Raylib PS2 — Implemented & Working Functions

This document catalogues every raylib function that is **concretely implemented** and produces
**correct output** on the PlayStation 2 target.

> **Build flags** (confirmed): `PLATFORM=PLATFORM_PLAYSTATION2 GRAPHICS=GRAPHICS_API_OPENGL_11 -I../../ps2gl/include`  
> `From: thirdparty/CMakeLists.txt @ 58:58`

Because raylib is compiled with `GRAPHICS_API_OPENGL_11`, every `rlgl*` call inside raylib routes
**directly** to the matching `gl*` function in ps2gl. This means every constraint documented in
[`docs/rendering/PS2GL_FUNCTIONS.md`](PS2GL_FUNCTIONS.md) applies equally here.  
Key inherited constraints:

- All vertex array `type` must be `GL_FLOAT`; `stride` must be `0`.
- Texture pixel data must be **16-byte aligned** in main RAM.
- No mipmaps (`level > 0` silently returns).
- Only 3 blend mode combinations are supported (see ps2gl doc §18).
- Depth is **inverted** — near maps to maximum depth and far maps to 0.
- `glMatrixMode(GL_TEXTURE)` is not implemented.

---

## Table of Contents

- [1. Window / Platform Initialization](#1-window--platform-initialization)
- [2. Frame Control](#2-frame-control)
- [3. Cursor (Logical State — No Hardware Cursor)](#3-cursor-logical-state--no-hardware-cursor)
- [4. Gamepad Input (DualShock 2)](#4-gamepad-input-dualshock-2)
- [5. 2D Drawing](#5-2d-drawing)
- [6. Textures](#6-textures)
- [7. Text & Fonts](#7-text--fonts)
- [8. 3D Drawing](#8-3d-drawing)
- [9. 3D Functions — Silent Failures on PS2](#9-3d-functions--silent-failures-on-ps2)
- [Not Available (No-ops on PS2)](#not-available-no-ops-on-ps2)

---

## 1. Window / Platform Initialization

| Function                                               | Description                                                                                                                                                                         | Evidence                                                                                                                  |
|:-------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------|
| `InitWindow(int width, int height, const char* title)` | Initializes ps2gl, allocates GS memory (via `initGsMemoryForRaylib`), opens the DualShock pad port, and sets the GS display mode (`NTSC` for `height=448`, `PAL` for `height=512`). | `From: thirdparty/raylib/src/rcore.c @ 651:803` → `From: thirdparty/raylib/src/platforms/rcore_playstation2.c @ 907:1044` |
| `WindowShouldClose(void)`                              | Returns `CORE.Window.shouldClose`; always returns `true` if the window is not ready.                                                                                                | `From: thirdparty/raylib/src/platforms/rcore_playstation2.c @ 141:145`                                                    |
| `CloseWindow(void)`                                    | De-initializes rlgl resources and shuts down the platform.                                                                                                                          | `From: thirdparty/raylib/src/rcore.c @ 805:946`                                                                           |
| `SetWindowTitle(const char* title)`                    | Stores `title` in `CORE.Window.title`; no visual effect on PS2.                                                                                                                     | `From: thirdparty/raylib/src/platforms/rcore_playstation2.c @ 202:205`                                                    |
| `SetWindowMinSize(int width, int height)`              | Stores minimum dimensions in `CORE.Window.screenMin`.                                                                                                                               | `From: thirdparty/raylib/src/platforms/rcore_playstation2.c @ 220:224`                                                    |
| `SetWindowMaxSize(int width, int height)`              | Stores maximum dimensions in `CORE.Window.screenMax`.                                                                                                                               | `From: thirdparty/raylib/src/platforms/rcore_playstation2.c @ 226:231`                                                    |

---

## 2. Frame Control

| Function                       | Description                                                                                                                                                                    | Evidence                                                               |
|:-------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-----------------------------------------------------------------------|
| `BeginDrawing(void)`           | Resets per-frame draw stats and calls `pglBeginGeometry()` to start the DMA chain.                                                                                             | `From: thirdparty/raylib/src/rcore.c @ 957:981`                        |
| `EndDrawing(void)`             | Calls `SwapScreenBuffer()` and updates fps timing.                                                                                                                             | `From: thirdparty/raylib/src/rcore.c @ 983:1097`                       |
| `SwapScreenBuffer(void)`       | Ends geometry DMA chain (`pglEndGeometry`), waits for previous frame to complete, waits for VSync, swaps buffers, then starts dispatching the new frame (`pglRenderGeometry`). | `From: thirdparty/raylib/src/platforms/rcore_playstation2.c @ 380:391` |
| `ClearBackground(Color color)` | Sets the clear color and calls `glClear(GL_COLOR_BUFFER_BIT \| GL_DEPTH_BUFFER_BIT)`.                                                                                          | `From: thirdparty/raylib/src/rcore.c @ 950:955`                        |
| `GetTime(void)`                | Returns elapsed seconds since `InitTimer()` using the EE `clock()` counter.                                                                                                    | `From: thirdparty/raylib/src/platforms/rcore_playstation2.c @ 398:402` |
| `GetFPS(void)`                 | Returns the last-measured frames-per-second.                                                                                                                                   | `From: thirdparty/raylib/src/rcore.c @ 1755:1775`                      |

---

## 3. Cursor (Logical State — No Hardware Cursor)

There is no hardware or software cursor rendered on PS2. These functions only toggle an internal flag and center the
logical position.

| Function                         | Description                                                   | Evidence                                                               |
|:---------------------------------|:--------------------------------------------------------------|:-----------------------------------------------------------------------|
| `ShowCursor(void)`               | Clears `CORE.Input.Mouse.cursorHidden`.                       | `From: thirdparty/raylib/src/platforms/rcore_playstation2.c @ 350:353` |
| `HideCursor(void)`               | Sets `CORE.Input.Mouse.cursorHidden`.                         | `From: thirdparty/raylib/src/platforms/rcore_playstation2.c @ 355:359` |
| `EnableCursor(void)`             | Centers the logical mouse position and clears `cursorHidden`. | `From: thirdparty/raylib/src/platforms/rcore_playstation2.c @ 361:368` |
| `DisableCursor(void)`            | Centers the logical mouse position and sets `cursorHidden`.   | `From: thirdparty/raylib/src/platforms/rcore_playstation2.c @ 370:377` |
| `SetMousePosition(int x, int y)` | Writes `(x, y)` to `CORE.Input.Mouse.currentPosition`.        | `From: thirdparty/raylib/src/platforms/rcore_playstation2.c @ 431:435` |

---

## 4. Gamepad Input (DualShock 2)

All 16 digital buttons are mapped. **Analog stick axes are not mapped** — `GetGamepadAxisMovement()` always returns
`0.0f`.

### Button Mapping

`From: thirdparty/raylib/src/platforms/rcore_playstation2.c @ 83:104`

| DualShock Button       | Raylib Constant                   |
|:-----------------------|:----------------------------------|
| SELECT                 | `GAMEPAD_BUTTON_MIDDLE_LEFT`      |
| START                  | `GAMEPAD_BUTTON_MIDDLE_RIGHT`     |
| Cross (×)              | `GAMEPAD_BUTTON_RIGHT_FACE_DOWN`  |
| Circle (○)             | `GAMEPAD_BUTTON_RIGHT_FACE_RIGHT` |
| Square (□)             | `GAMEPAD_BUTTON_RIGHT_FACE_LEFT`  |
| Triangle (△)           | `GAMEPAD_BUTTON_RIGHT_FACE_UP`    |
| L1                     | `GAMEPAD_BUTTON_LEFT_TRIGGER_1`   |
| L2                     | `GAMEPAD_BUTTON_LEFT_TRIGGER_2`   |
| R1                     | `GAMEPAD_BUTTON_RIGHT_TRIGGER_1`  |
| R2                     | `GAMEPAD_BUTTON_RIGHT_TRIGGER_2`  |
| D-Pad Up               | `GAMEPAD_BUTTON_LEFT_FACE_UP`     |
| D-Pad Down             | `GAMEPAD_BUTTON_LEFT_FACE_DOWN`   |
| D-Pad Left             | `GAMEPAD_BUTTON_LEFT_FACE_LEFT`   |
| D-Pad Right            | `GAMEPAD_BUTTON_LEFT_FACE_RIGHT`  |
| L3 (left stick click)  | `GAMEPAD_BUTTON_LEFT_THUMB`       |
| R3 (right stick click) | `GAMEPAD_BUTTON_RIGHT_THUMB`      |

### Input Query Functions

`PollInputEvents()` reads the pad state and populates `CORE.Input.Gamepad` every frame.  
`From: thirdparty/raylib/src/platforms/rcore_playstation2.c @ 445:620`

The following standard raylib functions work correctly against the populated state:

| Function                                           | Description                                               |
|:---------------------------------------------------|:----------------------------------------------------------|
| `IsGamepadAvailable(int gamepad)`                  | Returns `true` if pad port 0 is in a stable state.        |
| `IsGamepadButtonDown(int gamepad, int button)`     | Returns `true` while the mapped button is held.           |
| `IsGamepadButtonPressed(int gamepad, int button)`  | Returns `true` on the first frame the button is down.     |
| `IsGamepadButtonReleased(int gamepad, int button)` | Returns `true` on the first frame the button is released. |
| `GetGamepadButtonPressed(void)`                    | Returns the last button pressed this frame.               |

---

## 5. 2D Drawing

These functions operate through the rlgl batch renderer, which issues `glBegin`/`glVertex`/`glEnd`
calls (immediate mode) to ps2gl. All ps2gl constraints apply.

| Function                                                        | Description                                                                                                                                                                                                            | Evidence                                                                                              |
|:----------------------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------|
| `BeginMode2D(Camera2D camera)`                                  | Pushes projection and modelview matrices; sets a 2D orthographic projection via `glOrtho`. ⚠️ Depth inversion applies — use `BeginMode2D`/`EndMode2D` as a pair; do not call `glDepthFunc` manually inside a 2D block. | `From: thirdparty/raylib/src/rcore.c @ 1099:1108` → `From: thirdparty/ps2gl/src/matrix.cpp @ 220:282` |
| `EndMode2D(void)`                                               | Restores the projection and modelview matrices.                                                                                                                                                                        | `From: thirdparty/raylib/src/rcore.c @ 1110:1118`                                                     |
| `DrawPixel(int x, int y, Color color)`                          | Draws a single pixel using `GL_POINTS`.                                                                                                                                                                                | `From: thirdparty/raylib/src/rshapes.c @ 128:176`                                                     |
| `DrawLine(int x1, int y1, int x2, int y2, Color color)`         | Draws a 2D line using `GL_LINES`.                                                                                                                                                                                      | `From: thirdparty/raylib/src/rshapes.c @ 178:192`                                                     |
| `DrawRectangle(int x, int y, int w, int h, Color color)`        | Draws a filled rectangle using `GL_TRIANGLES`.                                                                                                                                                                         | `From: thirdparty/raylib/src/rshapes.c @ 727:738`                                                     |
| `DrawRectangleRec(Rectangle rec, Color color)`                  | `Rectangle`-struct variant of `DrawRectangle`.                                                                                                                                                                         | `From: thirdparty/raylib/src/rshapes.c @ 740:749`                                                     |
| `DrawCircle(int cx, int cy, float radius, Color color)`         | Draws a filled circle using `GL_TRIANGLES` fan.                                                                                                                                                                        | `From: thirdparty/raylib/src/rshapes.c @ 320:334`                                                     |
| `DrawTriangle(Vector2 v1, Vector2 v2, Vector2 v3, Color color)` | Draws a filled triangle.                                                                                                                                                                                               | `From: thirdparty/raylib/src/rshapes.c @ 1438:1466`                                                   |

---

## 6. Textures

| Function                                                                                                       | Description                                                                                                                                                                                                                 | Evidence                                                                                                   |
|:---------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-----------------------------------------------------------------------------------------------------------|
| `LoadTexture(const char* fileName)`                                                                            | Loads an image file and uploads it to GS VRAM via `glTexImage2D`. ⚠️ Pixel data must be 16-byte aligned (enforced by the `PGL_PATCHED_FONT_ALIGN` patch for font atlases). ⚠️ No mipmap support — only level 0 is uploaded. | `From: thirdparty/raylib/src/rtextures.c @ 4127:4180` → `From: thirdparty/ps2gl/src/texture.cpp @ 585:648` |
| `UnloadTexture(Texture2D texture)`                                                                             | Deletes the GL texture name and frees GS VRAM.                                                                                                                                                                              | `From: thirdparty/raylib/src/rtextures.c @ 4327:4341` → `From: thirdparty/ps2gl/src/texture.cpp @ 577:583` |
| `DrawTexture(Texture2D texture, int x, int y, Color tint)`                                                     | Draws a full texture at pixel coordinates.                                                                                                                                                                                  | `From: thirdparty/raylib/src/rtextures.c @ 4494:4514`                                                      |
| `DrawTextureRec(Texture2D texture, Rectangle src, Vector2 pos, Color tint)`                                    | Draws a sub-region of a texture.                                                                                                                                                                                            | `From: thirdparty/raylib/src/rtextures.c @ 4516:4524`                                                      |
| `DrawTexturePro(Texture2D texture, Rectangle src, Rectangle dest, Vector2 origin, float rotation, Color tint)` | Draws a texture with full transform: source rect, destination rect, pivot origin, and rotation.                                                                                                                             | `From: thirdparty/raylib/src/rtextures.c @ 4526:4620`                                                      |

---

## 7. Text & Fonts

Font atlas textures require special care on PS2.  
The `PGL_PATCHED_FONT_ALIGN` patch changes atlas allocation to `memalign(16, …)` to satisfy
ps2gl's 16-byte alignment requirement for `glTexImage2D`.  
The `PGL_PATCHED_ATLAS_LIFETIME` patch makes the atlas buffer `static` (BSS lifetime) so that it
is not freed before the GS DMA upload completes.

| Function                                                                                          | Description                                                                                                                                                                     | Evidence                                          |
|:--------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:--------------------------------------------------|
| `LoadFont(const char* fileName)`                                                                  | Loads a font from file and builds a texture atlas. ⚠️ The atlas pixel buffer is `static` on PS2 (see `PGL_PATCHED_ATLAS_LIFETIME`) to prevent cache-eviction races with GS DMA. | `From: thirdparty/raylib/src/rtext.c @ 433:600`   |
| `UnloadFont(Font font)`                                                                           | Unloads the font atlas texture and frees GS VRAM.                                                                                                                               | `From: thirdparty/raylib/src/rtext.c @ 1083:1117` |
| `DrawText(const char* text, int x, int y, int fontSize, Color color)`                             | Draws text using the default (built-in) font.                                                                                                                                   | `From: thirdparty/raylib/src/rtext.c @ 1266:1281` |
| `DrawTextEx(Font font, const char* text, Vector2 pos, float fontSize, float spacing, Color tint)` | Draws text using a custom font with spacing control.                                                                                                                            | `From: thirdparty/raylib/src/rtext.c @ 1283:1405` |
| `MeasureText(const char* text, int fontSize)`                                                     | Returns the pixel width of `text` rendered with the default font.                                                                                                               | `From: thirdparty/raylib/src/rtext.c @ 1407:1422` |

---

## 8. 3D Drawing

All 3D functions route through rlgl → ps2gl. The depth inversion from `glFrustum` means correct 3D
rendering requires **no manual depth function override** — raylib sets `GL_LEQUAL` by default which
ps2gl maps to GS `kGEqual` (the correct inverted comparison).

| Function                                                             | Description                                                                                                                                                              | Evidence                                                                                                  |
|:---------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------|
| `BeginMode3D(Camera3D camera)`                                       | Sets perspective projection via `glFrustum` and view matrix from camera parameters. ⚠️ Depth is inverted; `camera.fovy` must be set correctly for visible output.        | `From: thirdparty/raylib/src/rcore.c @ 1120:1157` → `From: thirdparty/ps2gl/src/matrix.cpp @ 156:218`     |
| `EndMode3D(void)`                                                    | Restores the 2D projection matrices.                                                                                                                                     | `From: thirdparty/raylib/src/rcore.c @ 1159:1173`                                                         |
| `DrawLine3D(Vector3 start, Vector3 end, Color color)`                | Draws a 3D line segment using `GL_LINES`.                                                                                                                                | `From: thirdparty/raylib/src/rmodels.c @ 175:196`                                                         |
| `DrawCube(Vector3 pos, float w, float h, float l, Color color)`      | Draws a colored cube using `GL_TRIANGLES`.                                                                                                                               | `From: thirdparty/raylib/src/rmodels.c @ 256:339`                                                         |
| `DrawCubeWires(Vector3 pos, float w, float h, float l, Color color)` | Draws a cube wireframe using `GL_LINES`.                                                                                                                                 | `From: thirdparty/raylib/src/rmodels.c @ 341:417`                                                         |
| `DrawSphere(Vector3 pos, float radius, Color color)`                 | Draws a sphere using `GL_TRIANGLES`.                                                                                                                                     | `From: thirdparty/raylib/src/rmodels.c @ 419:477`                                                         |
| `DrawPlane(Vector3 pos, Vector2 size, Color color)`                  | Draws a flat ground plane using `GL_QUADS`.                                                                                                                              | `From: thirdparty/raylib/src/rmodels.c @ 1051:1083`                                                       |
| `DrawGrid(int slices, float spacing)`                                | Draws a reference grid using `GL_LINES`.                                                                                                                                 | `From: thirdparty/raylib/src/rmodels.c @ 1085:1138`                                                       |
| `LoadModel(const char* fileName)`                                    | Loads a 3D model and stores its vertex data in arrays. ⚠️ Arrays are **not copied** by ps2gl; the app must double-buffer geometry if it changes after the first render.  | `From: thirdparty/raylib/src/rmodels.c @ 1140:1241` → `From: thirdparty/ps2gl/src/gmanager.cpp @ 163:179` |
| `UnloadModel(Model model)`                                           | Frees all mesh and material data for the model.                                                                                                                          | `From: thirdparty/raylib/src/rmodels.c @ 1243:1305`                                                       |
| `DrawModel(Model model, Vector3 pos, float scale, Color tint)`       | Draws a static 3D model. ⚠️ Geometry is rendered one frame deferred; **do not modify model mesh arrays** between `EndDrawing` and the next `pglFinishRenderingGeometry`. | `From: thirdparty/raylib/src/rmodels.c @ 3840:3924` → `From: thirdparty/ps2gl/src/gmanager.cpp @ 163:179` |

---

## 9. 3D Functions — Silent Failures on PS2

These functions **compile and link without errors** but produce **no visible output** or
**corrupted output** on PS2 hardware. They are included here so bugs surface in testing rather
than production.

| Function                                                                              | Why it silently fails                                                                                                                                                                                                                                              | Evidence                                                                                                                                                       |
|:--------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `LoadModelAnimations(const char* fileName, int* animCount)`                           | Loads animation data successfully in main RAM. However, applying animations modifies the vertex array in-place. Because ps2gl does not copy geometry, the in-place modification races with the VIF1 DMA chain from the previous frame, producing garbled vertices. | `From: thirdparty/raylib/src/rmodels.c @ 2319:2332` → `From: thirdparty/ps2gl/src/gmanager.cpp @ 163:179`                                                      |
| `LoadShader(const char* vs, const char* fs)`                                          | ps2gl does not implement GLSL. `rlLoadShaderCode` is a no-op on `GRAPHICS_API_OPENGL_11`; the returned `Shader` has `id=0`. The default pipeline is always used.                                                                                                   | `From: thirdparty/raylib/src/rcore.c @ 1391:1410`                                                                                                              |
| `SetShaderValue(Shader shader, int locIndex, …)`                                      | All shader locations are `-1` on `GRAPHICS_API_OPENGL_11`; the call is a no-op.                                                                                                                                                                                    | `From: thirdparty/raylib/src/rcore.c @ 1525:1529`                                                                                                              |
| `BeginShaderMode(Shader shader)` / `EndShaderMode()`                                  | No custom shader binding exists in `GRAPHICS_API_OPENGL_11`; the default ps2gl pipeline is always active.                                                                                                                                                          | `From: thirdparty/raylib/src/rcore.c @ 1227:1262`                                                                                                              |
| `LoadRenderTexture(int width, int height)` / `BeginTextureMode(RenderTexture2D)`      | Requires framebuffer objects or `glCopyTexImage2D`. Neither exists in ps2gl. Output is undefined — typically a black screen or stale framebuffer.                                                                                                                  | `From: thirdparty/raylib/src/rtextures.c @ 4272:4305` → `From: thirdparty/raylib/src/rcore.c @ 1175:1197` → `From: thirdparty/ps2gl/src/texture.cpp @ 758:766` |
| `DrawBillboard(Camera3D camera, Texture2D tex, Vector3 pos, float scale, Color tint)` | Internally calls `DrawBillboardPro`, which uses `rlDisableDepthMask()` (maps to `glDepthMask(false)` — supported) plus alpha blending. Only 3 blend combinations are supported; non-standard billboard blending silently falls through to wrong alpha compositing. | `From: thirdparty/raylib/src/rmodels.c @ 3926:3931` → `From: thirdparty/ps2gl/src/drawcontext.cpp @ 292:315`                                                   |
| `GenMeshHeightmap(Image heightmap, Vector3 size)`                                     | Generates a mesh with interleaved vertex/normal/texcoord data in a single buffer. ps2gl requires separate arrays with `stride=0` and `type=GL_FLOAT`; the interleaved buffer triggers `mNotImplemented()` in `glVertexPointer`.                                    | `From: thirdparty/raylib/src/rmodels.c @ 3164:3260` → `From: thirdparty/ps2gl/src/gmanager.cpp @ 80:97`                                                        |

---

## Not Available (No-ops on PS2)

These functions emit `TRACELOG(LOG_WARNING, "… not available on target platform")` and return
immediately. They are listed with the architectural reason.

`From: thirdparty/raylib/src/platforms/rcore_playstation2.c @ 148:346`

| Function                                                            | Why it is not available                                                                                                                          |
|:--------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------|
| `ToggleFullscreen()`                                                | PS2 has no window system; the display is always fullscreen.                                                                                      |
| `ToggleBorderlessWindowed()`                                        | No window compositor exists on PS2.                                                                                                              |
| `MaximizeWindow()` / `MinimizeWindow()` / `RestoreWindow()`         | No window state management — always fullscreen.                                                                                                  |
| `SetWindowState()` / `ClearWindowState()`                           | No OS window flags.                                                                                                                              |
| `SetWindowIcon()` / `SetWindowIcons()`                              | No window titlebar or icon.                                                                                                                      |
| `SetWindowPosition(int x, int y)`                                   | Hardware display origin is fixed.                                                                                                                |
| `SetWindowMonitor(int monitor)`                                     | No multi-monitor concept.                                                                                                                        |
| `SetWindowSize(int w, int h)`                                       | Hardware resolution is fixed at `InitWindow`.                                                                                                    |
| `SetWindowOpacity(float opacity)`                                   | No desktop compositing layer.                                                                                                                    |
| `SetWindowFocused()`                                                | No window manager.                                                                                                                               |
| `GetWindowHandle()`                                                 | No OS window handle; returns `NULL`.                                                                                                             |
| `GetMonitorCount()` / `GetCurrentMonitor()`                         | No monitor query API; return `1`/`0`.                                                                                                            |
| `GetMonitorPosition()` / `GetMonitorWidth()` / `GetMonitorHeight()` | No monitor query API; return zeros.                                                                                                              |
| `GetMonitorPhysicalWidth()` / `GetMonitorPhysicalHeight()`          | No monitor query API.                                                                                                                            |
| `GetMonitorRefreshRate()` / `GetMonitorName()`                      | No monitor query API.                                                                                                                            |
| `GetWindowPosition()` / `GetWindowScaleDPI()`                       | No window position concept; return zero vectors.                                                                                                 |
| `SetClipboardText()` / `GetClipboardText()`                         | No IOP clipboard service; `GetClipboardText` returns `NULL`.                                                                                     |
| `SetMouseCursor(int cursor)`                                        | No hardware or software cursor on PS2.                                                                                                           |
| `SetGamepadMappings(const char* mappings)`                          | Mapping is a fixed compile-time table; custom mappings are not supported. `From: thirdparty/raylib/src/platforms/rcore_playstation2.c @ 424:428` |
| Audio API (`LoadSound`, `PlaySound`, `LoadMusicStream`, etc.)       | `SUPPORT_MODULE_RAUDIO` is disabled at compile time. `From: thirdparty/raylib/src/config.h @ 39:39`                                              |

