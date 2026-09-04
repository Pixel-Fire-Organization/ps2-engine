#include "EngineSubsystems.h"

#include <cstdio>

#include "EngineDebug.h"

namespace
{
    bool s_Enabled[static_cast<uint32_t>(EngineSubsystem::Count)] = {};

    struct Dependency
    {
        EngineSubsystem subsystem;
        EngineSubsystem requires_;
    };

    // Declared here and mirrored in each subsystem spec, so the bring-up order
    // can be reviewed against the documentation rather than inferred.
    const Dependency kDependencies[] = {
        {EngineSubsystem::Archive, EngineSubsystem::Io},
        {EngineSubsystem::Resource, EngineSubsystem::Io},
        {EngineSubsystem::Level, EngineSubsystem::Resource},
        {EngineSubsystem::Level, EngineSubsystem::Archive},
        {EngineSubsystem::Sector, EngineSubsystem::Level},
    };

    const uint32_t kDependencyCount = sizeof(kDependencies) / sizeof(kDependencies[0]);

    const EngineSubsystem kAll[] = {
        EngineSubsystem::Io,    EngineSubsystem::Archive, EngineSubsystem::Resource,    EngineSubsystem::Level,
        EngineSubsystem::Sector, EngineSubsystem::Input,   EngineSubsystem::Achievement, EngineSubsystem::PerfLogger,
    };
}

void Engine_Subsystems_Set(const EngineSubsystem* list, uint32_t count)
{
    for (uint32_t i = 0; i < static_cast<uint32_t>(EngineSubsystem::Count); ++i)
        s_Enabled[i] = false;

    if (!list)
        return;

    for (uint32_t i = 0; i < count; ++i)
    {
        const uint32_t index = static_cast<uint32_t>(list[i]);
        if (index < static_cast<uint32_t>(EngineSubsystem::Count))
            s_Enabled[index] = true;
    }
}

bool Engine_Subsystem_IsEnabled(EngineSubsystem subsystem)
{
    const uint32_t index = static_cast<uint32_t>(subsystem);
    return index < static_cast<uint32_t>(EngineSubsystem::Count) && s_Enabled[index];
}

const char* Engine_Subsystem_Name(EngineSubsystem subsystem)
{
    switch (subsystem)
    {
    case EngineSubsystem::Io:
        return "io";
    case EngineSubsystem::Archive:
        return "archive";
    case EngineSubsystem::Resource:
        return "resource";
    case EngineSubsystem::Level:
        return "level";
    case EngineSubsystem::Sector:
        return "sector";
    case EngineSubsystem::Input:
        return "input";
    case EngineSubsystem::Achievement:
        return "achievement";
    case EngineSubsystem::PerfLogger:
        return "perflogger";
    case EngineSubsystem::Count:
    default:
        return "<unknown>";
    }
}

void Engine_Subsystems_Validate()
{
    for (uint32_t i = 0; i < kDependencyCount; ++i)
    {
        const Dependency& d = kDependencies[i];
        if (Engine_Subsystem_IsEnabled(d.subsystem) && !Engine_Subsystem_IsEnabled(d.requires_))
        {
            char message[160];
            snprintf(message, sizeof(message), "subsystem '%s' was requested but its dependency '%s' was not", Engine_Subsystem_Name(d.subsystem), Engine_Subsystem_Name(d.requires_));
            Engine_Panic(message);
        }
    }
}

const EngineSubsystem* Engine_Subsystems_Default(uint32_t* outCount)
{
    if (outCount)
        *outCount = sizeof(kAll) / sizeof(kAll[0]);
    return kAll;
}
