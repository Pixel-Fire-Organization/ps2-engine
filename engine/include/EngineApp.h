#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------
// EngineApp — the sole public interface exposed to the application layer.
// The app must include ONLY this header (enforced via ENGINE_SANDBOX_MODE in
// CMakeLists.txt). All subsystems (memory, IO, resources, scripting) are
// managed internally and driven by Lua scripts.
// ---------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// Start the engine and run the entry-point Lua script.
// If mainScript is NULL, the canonical SCRIPTING_MAIN_SCRIPT_PATH is used.
// Returns false if the engine failed to initialise or the script could not be loaded.
bool EngineStart(const char* resourceLocationToken, const char* mainScript);

// Advance one frame: runs Lua OnUpdate, renders, pumps IO and resource systems.
// Must be called inside the main loop while !EngineExited().
void EngineUpdate(void);

// Returns true when the engine should stop (window close or engine.exit() from Lua).
bool EngineExited(void);

// Shut down all engine subsystems and release resources. Call after the main loop.
void EngineStop(void);

#ifdef __cplusplus
}
#endif

// ---------------------------------------------------------------------------
// Logging & panic — forward declarations resolved by EngineDebug.c.
// Avoids pulling in EngineDebug.h (and transitively raylib.h) into the app.
// ---------------------------------------------------------------------------
extern void Engine_LogInfo(const char* text, ...);

extern void Engine_LogError(const char* text, ...);

extern void Engine_Panic(const char* message);
