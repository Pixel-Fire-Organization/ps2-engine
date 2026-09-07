#include "EngineAchievement.h"

#include "EngineDebug.h"
#include "EngineSubsystems.h"
#include "platform/AchievementContract.h"
#include "platform/Platform.h"

namespace
{
    AchievementContract* s_Contract = nullptr;
    bool s_Loaded = false;

    bool s_ReportedUnavailable = false;

    bool Available()
    {
        return s_Loaded && s_Contract && s_Contract->IsAvailable();
    }

    void ReportUnavailableOnce()
    {
        if (s_ReportedUnavailable)
            return;
        s_ReportedUnavailable = true;

        Platform* platform = Engine_GetPlatform();
        const char* name = platform ? platform->GetName() : "<no platform>";

        if (!s_Loaded)
            Engine_LogInfo("Achievement: subsystem not loaded; unlocks are ignored");
        else if (!s_Contract)
            Engine_LogInfo("Achievement: %s has no achievement support; unlocks are ignored", name);
        else
            Engine_LogInfo("Achievement: %s cannot record achievements; unlocks are ignored", name);
    }
}

const char* Engine_Achievement_GetUnavailableReason()
{
    if (!s_Loaded)
        return "ACHIEVEMENT SUBSYSTEM NOT REQUESTED";
    if (!s_Contract)
        return "THIS PLATFORM HAS NO ACHIEVEMENTS";
    return s_Contract->GetUnavailableReason();
}

bool Engine_Achievement_Init(const char* commId)
{
    s_Loaded = true;
    s_ReportedUnavailable = false;
    s_Contract = nullptr;

    Platform* platform = Engine_GetPlatform();
    if (!platform)
        return true;

    s_Contract = platform->GetAchievements();
    if (!s_Contract)
    {
        Engine_LogInfo("Achievement: %s has no achievement support", platform->GetName());
        return true;
    }

    if (!s_Contract->Init(commId))
    {
        return true;
    }

    Engine_LogInfo("Achievement: ready on %s (%u declared)", platform->GetName(), s_Contract->GetCount());
    return true;
}

void Engine_Achievement_Close()
{
    if (s_Contract)
        s_Contract->Shutdown();
    s_Contract = nullptr;
    s_Loaded = false;
    s_ReportedUnavailable = false;
}

bool Engine_Achievement_IsAvailable() { return Available(); }

bool Engine_Achievement_Unlock(uint32_t id)
{
    if (!Available())
    {
        ReportUnavailableOnce();
        return false;
    }

    if (id >= ACHV_MAX_ENTRIES || id >= s_Contract->GetCount())
    {
        Engine_LogError("Achievement: unlock of unknown achievement %u (declared: %u)", id, s_Contract->GetCount());
        return false;
    }

    if (!s_Contract->Unlock(id))
    {
        Engine_LogError("Achievement: platform refused unlock of achievement %u", id);
        return false;
    }
    return true;
}

bool Engine_Achievement_IsUnlocked(uint32_t id)
{
    if (!Available() || id >= ACHV_MAX_ENTRIES)
        return false;
    return s_Contract->IsUnlocked(id);
}

uint32_t Engine_Achievement_GetCount() { return Available() ? s_Contract->GetCount() : 0u; }
