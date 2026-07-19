#include <cmath>

#include "GameAPI.h"
#include "Scene.h"

// The scene table. Index 0 is the selector (the boot scene); the rest are the
// selectable test scripts. Add a test = new scene_*.cpp + one row here.
namespace
{
    const Scene s_Scenes[] = {
        {"SELECT", Scene_Select_Init, Scene_Select_Update},
        {"SWARM STRESS", Scene_Swarm_Init, Scene_Swarm_Update},
        {"LEVEL SYSTEM", Scene_Level_Init, Scene_Level_Update},
    };
    const int s_SceneCount = static_cast<int>(sizeof(s_Scenes) / sizeof(s_Scenes[0]));
    int s_Active = 0;
} // namespace

void SceneManager_Init()
{
    s_Active = 0;
    if (s_Scenes[0].init)
        s_Scenes[0].init();
}

void SceneManager_Update(float dt)
{
    if (s_Scenes[s_Active].update)
        s_Scenes[s_Active].update(dt);
}

void SceneManager_SetScene(int index)
{
    if (index < 0 || index >= s_SceneCount)
        return;
    s_Active = index;
    if (s_Scenes[index].init)
        s_Scenes[index].init();
}

int SceneManager_Count() { return s_SceneCount; }

const char* SceneManager_Name(int index)
{
    return (index >= 0 && index < s_SceneCount) ? s_Scenes[index].name : "";
}

// --- Shared player + orbit camera -------------------------------------------
namespace
{
    float s_cubeX = 0.0f, s_cubeY = 0.0f, s_cubeZ = 0.0f;
    float s_camYaw = 0.0f, s_camPitch = 0.4f;
    const float SPEED = 4.0f;
    const float CAM_DIST = 20.0f;
    const float CAM_SPEED = 1.8f;
} // namespace

void Player_Reset()
{
    s_cubeX = s_cubeY = s_cubeZ = 0.0f;
    s_camYaw = 0.0f;
    s_camPitch = 0.4f;
}

float Player_X() { return s_cubeX; }
float Player_Y() { return s_cubeY; }
float Player_Z() { return s_cubeZ; }

void Player_Update(float dt)
{
    // Left stick moves the cube on the ground plane; buttons nudge Y / reset / exit.
    float lx = 0.0f, ly = 0.0f;
    game::GetJoyAxis(0, "left", &lx, &ly);
    s_cubeX += lx * SPEED * dt;
    s_cubeZ += ly * SPEED * dt;

    if (game::IsPadPressed(0, "x"))
        s_cubeY += SPEED * dt;
    if (game::IsPadPressed(0, "cir"))
        s_cubeY -= SPEED * dt;
    if (game::IsPadPressed(0, "tri"))
    {
        s_cubeX = s_cubeY = s_cubeZ = 0.0f;
    }
    if (game::IsPadPressed(0, "squ"))
    {
        game::Log("Square pressed — exiting");
        game::Exit();
    }

    // Right stick orbits the camera; clamp pitch so it never flips over.
    float rx = 0.0f, ry = 0.0f;
    game::GetJoyAxis(0, "right", &rx, &ry);
    s_camYaw += rx * CAM_SPEED * dt;
    s_camPitch += ry * CAM_SPEED * dt;
    if (s_camPitch > 1.45f)
        s_camPitch = 1.45f;
    if (s_camPitch < 0.05f)
        s_camPitch = 0.05f;

    const float cosPitch = cosf(s_camPitch);
    const float camX = s_cubeX + CAM_DIST * cosPitch * sinf(s_camYaw);
    const float camY = s_cubeY + CAM_DIST * sinf(s_camPitch);
    const float camZ = s_cubeZ + CAM_DIST * cosPitch * cosf(s_camYaw);
    game::SetCamera3D(camX, camY, camZ, s_cubeX, s_cubeY, s_cubeZ, 45.0f);
}

void Player_DrawCube(int texHandle)
{
    if (texHandle >= 0 && game::IsResourceReady(texHandle))
        game::DrawCubeTextured(s_cubeX, s_cubeY, s_cubeZ, 2.0f, texHandle);
    else
        game::DrawCube(s_cubeX, s_cubeY, s_cubeZ, 2.0f, 220, 60, 60);
}
