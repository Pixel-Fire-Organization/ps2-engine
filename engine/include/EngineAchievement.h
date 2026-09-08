#pragma once

#include <cstdint>

#define ACHV_MAX_ENTRIES 128

/// Presentation tier of a declared achievement. A set has at most one platinum,
/// earned by earning everything else.
enum class AchievementGrade : uint8_t
{
    Platinum = 0,
    Gold,
    Silver,
    Bronze
};

// ---------------------------------------------------------------------------
// The declared set. Generated from the title's achievement declaration, so the
// set a game can name and the set the engine can describe are one set. See
// docs/subsystems/ACHIEVEMENT.md.
// ---------------------------------------------------------------------------

/// @return How many achievements the title declares.
uint32_t Engine_Achievement_DeclaredCount();

/// @param id Achievement identifier.
/// @return Its display name, or an empty string when the identifier is unknown.
const char* Engine_Achievement_GetName(uint32_t id);

/// @param id Achievement identifier.
/// @return How it is earned, or an empty string.
const char* Engine_Achievement_GetDetail(uint32_t id);

/// @param id Achievement identifier.
/// @return Its tier; the lowest tier for an unknown identifier, since a caller
///         drawing one is better served by something than by nothing.
AchievementGrade Engine_Achievement_GetGrade(uint32_t id);

/// @param id Achievement identifier.
/// @return Whether it is concealed until earned.
bool Engine_Achievement_IsHidden(uint32_t id);

/// Bring the Achievement subsystem up.
/// @param commId Communication id, or null to use the one the platform was built with.
/// @return True, including on platforms with no achievement support.
bool Engine_Achievement_Init(const char* commId);

void Engine_Achievement_Close();

/// @return True only when the subsystem is loaded and the platform can record.
bool Engine_Achievement_IsAvailable();

/// Advance whatever the platform could not finish while binding, now that
/// frames are running. Called once per frame until it answers false.
/// @return True while binding is still settling, so a caller that must wait for
///         a settled answer knows not to read one yet.
bool Engine_Achievement_PumpStartup();

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
int32_t Engine_Achievement_GetConsoleCount();

/// @return Why nothing can be recorded, or null when it can.
const char* Engine_Achievement_GetUnavailableReason();
