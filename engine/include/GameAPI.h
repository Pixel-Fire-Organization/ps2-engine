#pragma once

// ---------------------------------------------------------------------------
// GameAPI — the friendly C++ surface for authoring gameplay.
//
// This replaces the old Lua gameplay bindings. Game code lives in C++ and runs
// natively (no interpreter in the per-frame hot loop), but stays approachable:
// every function takes plain scalars (floats / ints / const char*), so a game
// module needs no engine internals and no engine types.
//
//   * The GAME implements  GameInit() / GameUpdate(dt)  (engine calls them).
//   * The ENGINE implements everything in namespace game (game calls them).
//
// Mirrors the semantics of the retired graphics.*/input.*/resources.* bindings
// one-for-one, so porting a Lua script to C++ is mechanical.
// ---------------------------------------------------------------------------

// --- Entry points the game module must define -------------------------------
// GameInit()      is called once, after the engine is initialised.
// GameUpdate(dt)  is called every frame (dt = seconds since last frame). Do all
//                 per-frame gameplay + draw submission here.
void GameInit();
void GameUpdate(float dt);

namespace game {

// --- Time -------------------------------------------------------------------
float GetTime();       // seconds since engine start
float GetDeltaTime();  // seconds elapsed last frame

// --- Lifecycle / debug ------------------------------------------------------
void Exit();               // request engine shutdown (loop ends next check)
void Log(const char* msg); // info log line

// Resolve a relative asset path against the active device token
// (e.g. "RASSETS\\BOX.PS2A" -> "cdrom0:\\RASSETS\\BOX.PS2A;1").
// Returns a pointer to an internal static buffer — copy it if you need to keep
// it; the next call overwrites it. Not reentrant (single-threaded game code).
const char* MakePath(const char* relativePath);

// --- Camera -----------------------------------------------------------------
// Set the 3D camera pose and make it the active (rendered) camera this frame.
// Up vector is (0,1,0) and projection is perspective. Call each frame when the
// camera moves. Collapses the old make/update/begin_mode_3d trio into one call.
void SetCamera3D(float posX, float posY, float posZ,
                 float targetX, float targetY, float targetZ,
                 float fovy);

// --- Frame / background -----------------------------------------------------
void Clear(int r, int g, int b); // clear colour, components 0..255

// --- 2D primitives (immediate mode — submit every frame) --------------------
// Screen-space rectangle in pixels. Colour components 0..255. The building
// block for UI/HUD authored in C++ (there is no separate UI scripting layer).
void DrawRect(int x, int y, int width, int height, int r, int g, int b);

// --- 3D primitives (immediate mode — submit every frame) --------------------
// Colour components are 0..255.
void DrawGrid(int slices, float spacing);
void DrawCube(float x, float y, float z, float size, int r, int g, int b);
void DrawSphere(float x, float y, float z, float size, int r, int g, int b);
void DrawCylinder(float x, float y, float z, float size, int r, int g, int b);
void DrawCubeTextured(float x, float y, float z, float size, int textureId);

// --- Input ------------------------------------------------------------------
// button: "x","cir","squ","tri","dpad_up/down/left/right","l1","l2","r1","r2",
//         "l3","r3","start","select".
bool IsPadPressed(int pad, const char* button);
// side: "left" or "right". Writes analog stick axes in [-1,+1] (deadzone
// applied C-side). Writes 0,0 on any error.
void GetJoyAxis(int pad, const char* side, float* outX, float* outY);

// --- Resources (async streaming; poll IsResourceReady) ----------------------
// type: "TEXTURE","MODEL","SOUND","FONT". Returns a handle >= 0, or -1.
int  LoadResource(const char* type, const char* path);
bool IsResourceReady(int handle);

} // namespace game
