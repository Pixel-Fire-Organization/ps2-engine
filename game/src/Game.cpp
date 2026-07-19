// ---------------------------------------------------------------------------
// Game.cpp — the C++ game module (GameInit / GameUpdate).
//
// This is now a thin shell over the scene system (Scene.h / SceneManager.cpp):
// on boot the selector scene runs a menu to pick which test script to execute
// (stress swarm, level system, ...). See game/src/scene_*.cpp to add tests.
// ---------------------------------------------------------------------------

#include "GameAPI.h"
#include "Scene.h"

void GameInit()
{
    game::Log("MAIN (C++) starting...");
    SceneManager_Init(); // activates the selector scene
}

void GameUpdate(float dt)
{
    SceneManager_Update(dt);
}
