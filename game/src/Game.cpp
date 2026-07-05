// ---------------------------------------------------------------------------
// Game.cpp — the C++ game module (GameInit / GameUpdate).
//
// Direct port of MAIN.LUA's gameplay: an orbit camera driven by the right stick,
// a player cube driven by the left stick, and a stress swarm that loads once the
// box texture has streamed in. This doubles as the reference example for the
// friendly GameAPI — note it includes only "GameAPI.h", no engine internals.
// ---------------------------------------------------------------------------

#include <cmath>

#include "GameAPI.h"
#include "SwarmSystem.h"

namespace
{

    // --- Player cube state ---
    float s_cubeX = 0.0f, s_cubeY = 0.0f, s_cubeZ = 0.0f;
    constexpr float SPEED = 4.0f; // units per second

    // --- Orbit camera state (spherical around the cube) ---
    float s_camYaw = 0.0f;
    float s_camPitch = 0.4f;
    constexpr float CAM_DIST = 20.0f;
    constexpr float CAM_SPEED = 1.8f;

    // --- Resources / scene ---
    int s_texHandle = -1;
    bool s_swarmLoaded = false;

} // namespace

void GameInit()
{
    game::Log("MAIN (C++) starting...");
    s_texHandle = game::LoadResource("TEXTURE", game::MakePath("RASSETS\\BOX.PS2A"));
    game::Log("Texture requested (async)");
}

void GameUpdate(float dt)
{
    // Left stick moves the cube on the ground plane; buttons nudge Y / reset.
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
        s_cubeX = 0.0f;
        s_cubeY = 0.0f;
        s_cubeZ = 0.0f;
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

    // Background + ground grid.
    game::Clear(20, 20, 20);
    game::DrawGrid(100, 1.0f);

    // Draw the player cube (textured once the texture is ready), and kick off the
    // stress swarm only after the async texture load completes — starting it
    // earlier would saturate the EE bus and starve the CDROM SIF DMA.
    if (game::IsResourceReady(s_texHandle))
    {
        game::DrawCubeTextured(s_cubeX, s_cubeY, s_cubeZ, 2.0f, s_texHandle);
        if (!s_swarmLoaded)
        {
            s_swarmLoaded = true;
            game::Log("Texture ready — starting native swarm");
            swarm::Init();
        }
    }
    else
    {
        game::DrawCube(s_cubeX, s_cubeY, s_cubeZ, 2.0f, 220, 60, 60);
    }

    if (s_swarmLoaded)
        swarm::Update();
}
