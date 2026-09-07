#pragma once

#include <cstdint>

#define ACHV_MAX_ENTRIES 128

/// Bring the Achievement subsystem up.
/// @param commId Communication id, or null to use the one the platform was built with.
/// @return True, including on platforms with no achievement support.
bool Engine_Achievement_Init(const char* commId);

void Engine_Achievement_Close();

/// @return True only when the subsystem is loaded and the platform can record.
bool Engine_Achievement_IsAvailable();

/// Record an achievement as earned. Idempotent, one-way, and safe on every platform.
/// @param id Achievement identifier.
/// @return Whether the platform accepted it.
bool Engine_Achievement_Unlock(uint32_t id);

/// @param id Achievement identifier.
/// @return False when not unlocked, unknown, unavailable, or not loaded.
bool Engine_Achievement_IsUnlocked(uint32_t id);

/// @return Achievements this title declares, whether or not any can be recorded.
uint32_t Engine_Achievement_GetCount();

/// @return How many achievements the platform itself believes this title has.
///         Short of the declared count means the platform never took delivery
///         of the set, which is a different fault from being unable to record.
uint32_t Engine_Achievement_GetConsoleCount();

/// @return Why nothing can be recorded, or null when it can.
const char* Engine_Achievement_GetUnavailableReason();
