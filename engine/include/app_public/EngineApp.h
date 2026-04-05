// Public sandbox entry-point — forwards to the canonical EngineApp.h.
// When ENGINE_SANDBOX_MODE is ON, the app target's include path points to this
// directory only, so EngineMemory.h, EngineIO.h, etc. are unreachable.
#include "../EngineApp.h"

