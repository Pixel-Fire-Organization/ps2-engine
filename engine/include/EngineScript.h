#pragma once

#include <cstddef>
#include <cstdint>

// Forward declaration of lua_State to keep header clean
typedef struct lua_State lua_State;

typedef struct
{
    lua_State* L;
    uint32_t slotIndex; // Base index (EVEN: Heap, ODD: Bytecode)
    bool heapReady; // True once the free-list heap has been initialised in the slot
    size_t codeSize; // Actual byte count of the loaded script (not slot capacity)
    bool active;
} ScriptUnit;

// Initializer for the Lua subsystem
bool Engine_Script_Init();

void Engine_Script_Close();

using ExitCallback = void(*)();

// Register a callback invoked when Lua calls engine.exit().
// Must be called after Engine_Script_Init and before Engine_Script_Run.
void Engine_Script_SetExitCallback(ExitCallback onExit);

// Script Loading and Execution
// loads a script into a free slot pair (even/odd)
int Engine_Script_Load(const void* data, size_t size);

bool Engine_Script_Run(int unitIndex);

// Called every frame to trigger OnUpdate in all active scripts
void Engine_Script_UpdateAll(float dt);

// Closes any open BeginMode3D / BeginMode2D block.
// Called by EngineApp between UpdateAll and DrawDebugOverlay so the
// debug overlay is always drawn in flat 2D, outside of any 3D projection.
void Engine_Script_EndCurrentMode();

// Advances the internal frame counter and runs the camera LFU eviction pass.
// Must be called once per frame (after EndDrawing).
void Engine_Script_FrameTick();
