#pragma once

// ---------------------------------------------------------------------------
// Scene system — a small testbed harness so we can pick, at boot, which "script"
// runs (stress swarm, level system, ...). Everything is a scene, including the
// selector menu itself: the manager runs ONE active scene, calling init() when
// it becomes active and update(dt) each frame. Adding a test = write a
// scene_*.cpp with an init/update pair and add one row to the table in
// SceneManager.cpp.
// ---------------------------------------------------------------------------

struct Scene
{
    const char* name;
    void (*init)();
    void (*update)(float dt);
};

// --- Scene manager (SceneManager.cpp) ---
void SceneManager_Init(); // from GameInit: activates the boot scene (the selector)
void SceneManager_Update(float dt); // from GameUpdate: dispatches the active scene
void SceneManager_SetScene(int index); // switch active scene (calls its init once)
int SceneManager_Count();
const char* SceneManager_Name(int index);

// --- Shared player + orbit camera (SceneManager.cpp) ---
// Gameplay scenes call Player_Update(dt) then read Player_X/Y/Z. Handles left/right
// stick movement + orbit camera (game::SetCamera3D) and the exit button (square).
void Player_Update(float dt);
void Player_Reset();
float Player_X();
float Player_Y();
float Player_Z();
// Draw the player cube: textured if texHandle >= 0 and ready, else a plain colour.
void Player_DrawCube(int texHandle);

// --- Scene entry points (defined in scene_*.cpp) ---
void Scene_Select_Init();
void Scene_Select_Update(float dt);
void Scene_Swarm_Init();
void Scene_Swarm_Update(float dt);
void Scene_Level_Init();
void Scene_Level_Update(float dt);
