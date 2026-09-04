#pragma once

#include <cstdint>

/// Achievement recording, supplied only by platforms that have achievements.
/// Platform::GetAchievements() returns null on those that do not.
class AchievementContract
{
public:
    virtual ~AchievementContract() = default;

    AchievementContract(const AchievementContract&) = delete;
    AchievementContract(AchievementContract&&) = delete;
    AchievementContract& operator=(const AchievementContract&) = delete;
    AchievementContract& operator=(AchievementContract&&) = delete;

    /// Bind to the title achievement set.
    /// @param commId Identifies the set to the platform service.
    /// @return False when nothing can be recorded; not an engine failure.
    virtual bool Init(const char* commId) = 0;
    virtual void Shutdown() = 0;

    /// Record an achievement as earned. Idempotent.
    /// @param id Achievement identifier.
    /// @return Whether the platform accepted it.
    virtual bool Unlock(uint32_t id) = 0;

    /// @param id Achievement identifier.
    /// @return False when not unlocked, unknown, or unavailable.
    virtual bool IsUnlocked(uint32_t id) const = 0;

    /// @return Achievements this title declares; zero when unavailable.
    virtual uint32_t GetCount() const = 0;

    /// @return Whether anything can actually be recorded right now.
    virtual bool IsAvailable() const = 0;

protected:
    AchievementContract() = default;
};
