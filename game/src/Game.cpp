#include <cmath>

#include "EcsComponents.h"
#include "EngineCore.h"
#include "EngineSubsystems.h"
#include "GameAPI.h"

namespace
{
    const char* const LEVEL_NAME = "TEST";
    const float MOVE_SPEED = 6.0f;
    const float LIFT_SPEED = 4.0f;
    const float CAMERA_DISTANCE = 20.0f;
    const float CAMERA_SPEED = 1.8f;
    const float PITCH_MIN = 0.05f;
    const float PITCH_MAX = 1.45f;
    const float PLAYER_SIZE = 2.0f;
    const int GRID_SLICES = 100;
    const float GRID_SPACING = 1.0f;

    float s_PlayerX = 0.0f;
    float s_PlayerY = 0.0f;
    float s_PlayerZ = 0.0f;
    float s_Yaw = 0.0f;
    float s_Pitch = 0.4f;
    int s_Texture = -1;
    bool s_LevelLoaded = false;

    void Reset()
    {
        s_PlayerX = 0.0f;
        s_PlayerY = 0.0f;
        s_PlayerZ = 0.0f;
        s_Yaw = 0.0f;
        s_Pitch = 0.4f;
    }

    void MovePlayer(float dt)
    {
        float moveX = 0.0f;
        float moveZ = 0.0f;
        game::GetJoyAxis(0, "left", &moveX, &moveZ);
        s_PlayerX += moveX * MOVE_SPEED * dt;
        s_PlayerZ += moveZ * MOVE_SPEED * dt;

        if (game::IsPadPressed(0, "l1"))
            s_PlayerY += LIFT_SPEED * dt;
        if (game::IsPadPressed(0, "l2"))
            s_PlayerY -= LIFT_SPEED * dt;
        if (game::IsPadPressed(0, "tri"))
            Reset();
    }

    void MoveCamera(float dt)
    {
        float turnX = 0.0f;
        float turnY = 0.0f;
        game::GetJoyAxis(0, "right", &turnX, &turnY);
        s_Yaw += turnX * CAMERA_SPEED * dt;
        s_Pitch += turnY * CAMERA_SPEED * dt;
        if (s_Pitch > PITCH_MAX)
            s_Pitch = PITCH_MAX;
        if (s_Pitch < PITCH_MIN)
            s_Pitch = PITCH_MIN;

        const float flat = cosf(s_Pitch);
        game::SetCamera3D(s_PlayerX + CAMERA_DISTANCE * flat * sinf(s_Yaw), s_PlayerY + CAMERA_DISTANCE * sinf(s_Pitch),
                          s_PlayerZ + CAMERA_DISTANCE * flat * cosf(s_Yaw), s_PlayerX, s_PlayerY, s_PlayerZ, 45.0f);
    }
} // namespace

// Everything this game needs. Trimming the list is how a build runs headless:
// dropping Level and Sector, for instance, boots straight into logic with no
// world resident. See docs/ENGINE.md.
void GameConfigure(EngineConfig* config)
{
    static const EngineSubsystem kSubsystems[] = {
        EngineSubsystem::Io,    EngineSubsystem::Archive,     EngineSubsystem::Resource,  EngineSubsystem::Level,
        EngineSubsystem::Sector, EngineSubsystem::Input,      EngineSubsystem::Achievement, EngineSubsystem::PerfLogger,
    };

    config->subsystems = kSubsystems;
    config->subsystemCount = sizeof(kSubsystems) / sizeof(kSubsystems[0]);
}

void GameInit()
{
    Reset();
    game::SetSpawnHandler(&Ecs_SpawnDispatch);
    s_Texture = game::LoadResource("TEXTURE", game::MakePath("RASSETS\\BOX.PS2A"));

    s_LevelLoaded = game::LoadLevel(LEVEL_NAME);
    if (!s_LevelLoaded)
        game::Log("Level TEST not available - running on the grid");
}

void GameUpdate(float dt)
{
    MovePlayer(dt);
    MoveCamera(dt);
    if (s_LevelLoaded)
        game::SetStreamingCenter(s_PlayerX, s_PlayerY, s_PlayerZ);

    game::Clear(20, 20, 26);
    game::DrawGrid(GRID_SLICES, GRID_SPACING);

    if (s_Texture >= 0 && game::IsResourceReady(s_Texture))
        game::DrawCubeTextured(s_PlayerX, s_PlayerY, s_PlayerZ, PLAYER_SIZE, s_Texture);
    else
        game::DrawCube(s_PlayerX, s_PlayerY, s_PlayerZ, PLAYER_SIZE, 220, 60, 60);
}
