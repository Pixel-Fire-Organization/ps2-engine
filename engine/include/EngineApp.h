#pragma once

#include <cstdint>

#include "EngineDebug.h"

class Platform;
class Renderer;
struct EngineConfig;

#ifdef __cplusplus
extern "C" {
#endif

// Start the engine on the platform and renderer Engine_Main selected, then call
// the game module's GameInit(). Returns false if the engine failed to initialise.
bool EngineStart(const EngineConfig& config, Platform* platform, Renderer* renderer);

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
