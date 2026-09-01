// ---------------------------------------------------------------------------
// Game.cpp — the C++ game module (GameInit / GameUpdate).
//
// This is now a thin shell over the scene system (Scene.h / SceneManager.cpp):
// on boot the selector scene runs a menu to pick which test script to execute
// (stress swarm, level system, ...). See game/src/scene_*.cpp to add tests.
// ---------------------------------------------------------------------------

#include "EngineCore.h"
#include "EngineSubsystems.h"
#include "GameAPI.h"
#include "Scene.h"

// Everything this game needs. Trimming the list is how a build runs headless:
// dropping Level and Sector, for instance, boots straight into logic with no
// world resident. See docs/ENGINE.md.
void GameConfigure(EngineConfig* config)
{
    static const EngineSubsystem kSubsystems[] = {
        EngineSubsystem::Io, EngineSubsystem::Archive, EngineSubsystem::Resource, EngineSubsystem::Level, EngineSubsystem::Sector, EngineSubsystem::Input, EngineSubsystem::PerfLogger,
    };

    config->subsystems = kSubsystems;
    config->subsystemCount = sizeof(kSubsystems) / sizeof(kSubsystems[0]);
}

void GameInit()
{
    game::Log("MAIN (C++) starting...");
    SceneManager_Init(); // activates the selector scene
}

void GameUpdate(float dt) { SceneManager_Update(dt); }
