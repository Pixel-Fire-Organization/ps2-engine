#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------
// EngineApp — the sole public interface exposed to the application layer.
// The app must include ONLY this header (and GameAPI.h) — enforced via
// ENGINE_SANDBOX_MODE in CMakeLists.txt. All subsystems (memory, IO,
// resources) are managed internally; gameplay is authored in C++ via GameAPI.h
// (GameInit/GameUpdate), which this header's functions drive each frame.
// ---------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// Start the engine and call the game module's GameInit().
// Returns false if the engine failed to initialise.
bool EngineStart(const char* resourceLocationToken);

// Advance one frame: calls GameUpdate(dt), renders, pumps IO and resource systems.
// Must be called inside the main loop while !EngineExited().
void EngineUpdate(void);

// Returns true when the engine should stop (window close or game::Exit()).
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
