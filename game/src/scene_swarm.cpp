// scene_swarm — the stress-draw test: 630 animated primitives + a player cube.
#include "GameAPI.h"
#include "Scene.h"
#include "SwarmSystem.h"

namespace
{
    int s_tex = -1;
} // namespace

void Scene_Swarm_Init()
{
    Player_Reset();
    // The swarm has no real texture dependency; load BOX only for the textured
    // player cube, and start the swarm immediately.
    s_tex = game::LoadResource("TEXTURE", game::MakePath("RASSETS\\BOX.PS2A"));
    swarm::Init();
    game::Log("Swarm stress scene started");
}

void Scene_Swarm_Update(float dt)
{
    Player_Update(dt);
    game::Clear(20, 20, 20);
    game::DrawGrid(100, 1.0f);
    Player_DrawCube(s_tex);
    swarm::Update();
}
