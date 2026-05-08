#pragma once

#define SCRIPTING_LUA_MAX_UNITS 4
#define SCRIPTING_LUA_CODE_SLOT_SIZE (512 * 1024)

#define SCRIPTING_MAIN_SCRIPT_FILENAME "MAIN.LUA"

#define SCRIPTING_MAX_CAMERAS_3D 4
#define SCRIPTING_MAX_CAMERAS_2D 4

#define SCRIPTING_CAM_IDLE_FRAMES_EVICT 1000

// Minimum seconds between successive begin_mode_3d / begin_mode_2d re-entries
// when already inside the same mode. Prevents accidental rapid GPU overhead.
#define SCRIPTING_MODE_REENTRY_COOLDOWN_SEC 1.0

