// scene_level — the level-system test: load TEST.PS2R and stream sectors around
// the player.
#include "EcsComponents.h" // generated (build dir): declares Ecs_SpawnDispatch
#include "GameAPI.h"
#include "Scene.h"

void Scene_Level_Init()
{
    Player_Reset();
    // The level loader turns compiled map entities into the game's typed
    // components via the generated dispatcher (see EcsHooks.cpp).
    game::SetSpawnHandler(&Ecs_SpawnDispatch);
    if (game::LoadLevel("TEST"))
        game::Log("Level TEST loaded");
    else
        game::Log("Level TEST not available — running empty");
}

void Scene_Level_Update(float dt)
{
    Player_Update(dt);
    game::SetStreamingCenter(Player_X(), Player_Y(), Player_Z());
    game::Clear(20, 20, 20);
    game::DrawGrid(100, 1.0f);
    Player_DrawCube(-1);
}
