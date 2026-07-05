#pragma once

// Public sandbox entry-point — forwards to the canonical GameAPI.h.
// When ENGINE_SANDBOX_MODE is ON, the app target's include path points to this
// directory only; this shim keeps the friendly game API reachable from game
// code without exposing the rest of the engine include tree.
#include "../GameAPI.h"
