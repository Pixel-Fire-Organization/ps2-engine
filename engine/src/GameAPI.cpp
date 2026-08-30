// ---------------------------------------------------------------------------
// GameAPI.cpp — engine-side implementation of the friendly C++ game API.
//
// Every function here is a thin marshalling wrapper over the same engine
// services the retired Lua bindings called (Renderer, EngineInput,
// Engine_Resource_*, core timing). Gameplay now runs natively: no interpreter
// and no FFI crossing in the per-frame hot loop.
// ---------------------------------------------------------------------------

#include "GameAPI.h"

#include <cstring>

#include "Constants.h"
#include "EngineCore.h"
#include "EngineDebug.h"
#include "EngineInput.h"
#include "EngineLevel.h"
#include "EngineResource.h"
#include "graphics/Primitives.h"
#include "graphics/Renderer.h"
#include "graphics/Types.h"

// Defined in EngineApp.cpp — sets the internal exit flag polled by EngineExited().
void EngineApp_OnExitRequested();

namespace
{

    inline Color3 MakeColor(int r, int g, int b) { return Color3{static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f, static_cast<float>(b) / 255.0f}; }

    // Map a button name to the engine enum. Mirrors the old input.is_pad_pressed
    // binding one-for-one so ported scripts behave identically.
    GamePadButton ButtonFromName(const char* btn)
    {
        if (strcmp(btn, "x") == 0)
            return GamePadButton::Cross;
        if (strcmp(btn, "cir") == 0)
            return GamePadButton::Circle;
        if (strcmp(btn, "squ") == 0)
            return GamePadButton::Square;
        if (strcmp(btn, "tri") == 0)
            return GamePadButton::Triangle;
        if (strcmp(btn, "dpad_up") == 0)
            return GamePadButton::DPadUp;
        if (strcmp(btn, "dpad_down") == 0)
            return GamePadButton::DPadDown;
        if (strcmp(btn, "dpad_left") == 0)
            return GamePadButton::DPadLeft;
        if (strcmp(btn, "dpad_right") == 0)
            return GamePadButton::DPadRight;
        if (strcmp(btn, "l1") == 0)
            return GamePadButton::L1;
        if (strcmp(btn, "l2") == 0)
            return GamePadButton::L2;
        if (strcmp(btn, "r1") == 0)
            return GamePadButton::R1;
        if (strcmp(btn, "r2") == 0)
            return GamePadButton::R2;
        if (strcmp(btn, "l3") == 0)
            return GamePadButton::L3;
        if (strcmp(btn, "r3") == 0)
            return GamePadButton::R3;
        if (strcmp(btn, "start") == 0)
            return GamePadButton::Start;
        if (strcmp(btn, "select") == 0)
            return GamePadButton::Select;
        Engine_LogError("[Game] IsPadPressed: unknown button '%s'", btn);
        return GamePadButton::Unknown;
    }

} // namespace

namespace game
{

    // --- Time -------------------------------------------------------------------
    float GetTime() { return Engine_GetTotalTime(); }
    float GetDeltaTime() { return Engine_GetDeltaTime(); }

    // --- Lifecycle / debug ------------------------------------------------------
    void Exit() { EngineApp_OnExitRequested(); }

    void Log(const char* msg) { Engine_LogInfo("[Game] %s", msg); }

    const char* MakePath(const char* relativePath)
    {
        static char pathBuf[IO_FILE_MAX_PATH];
        const char* token = Engine_GetResourceLocationToken();
        Engine_BuildPath(token ? token : "cdrom0:", relativePath, pathBuf, IO_FILE_MAX_PATH);
        return pathBuf;
    }

    // --- Camera -----------------------------------------------------------------
    void SetCamera3D(float posX, float posY, float posZ, float targetX, float targetY, float targetZ, float fovy)
    {
        Renderer* r = Engine_GetRenderer();
        if (!r)
            return;

        Camera3D cam;
        cam.position = Vector3{posX, posY, posZ};
        cam.target = Vector3{targetX, targetY, targetZ};
        cam.up = Vector3{0.0f, 1.0f, 0.0f};
        cam.fovy = fovy;
        cam.projection = CAMERA_PERSPECTIVE;

        r->SetCamera3D(static_cast<CameraID>(0), cam);
        r->SetActiveCamera3D(static_cast<CameraID>(0));
    }

    // --- Frame / background -----------------------------------------------------
    void Clear(int r, int g, int b)
    {
        Renderer* renderer = Engine_GetRenderer();
        if (renderer)
            renderer->ClearFrame(MakeColor(r, g, b));
    }

    // --- 2D primitives ------------------------------------------------------------
    void DrawRect(int x, int y, int width, int height, int r, int g, int b)
    {
        Renderer* renderer = Engine_GetRenderer();
        if (renderer)
            renderer->DrawRect2D(x, y, width, height, MakeColor(r, g, b));
    }

    // --- 3D primitives ----------------------------------------------------------
    void DrawGrid(int slices, float spacing)
    {
        Renderer* r = Engine_GetRenderer();
        if (r)
            r->DrawGrid(slices, spacing);
    }

    void DrawCube(float x, float y, float z, float size, int r, int g, int b)
    {
        Renderer* renderer = Engine_GetRenderer();
        if (renderer)
            renderer->AddPrimitiveToDrawList(Primitive3D::Cube, Vector3{x, y, z}, Vector3{0.f, 0.f, 0.f}, Vector3{size, size, size}, MakeColor(r, g, b));
    }

    void DrawSphere(float x, float y, float z, float size, int r, int g, int b)
    {
        Renderer* renderer = Engine_GetRenderer();
        if (renderer)
            renderer->AddPrimitiveToDrawList(Primitive3D::Sphere, Vector3{x, y, z}, Vector3{0.f, 0.f, 0.f}, Vector3{size, size, size}, MakeColor(r, g, b));
    }

    void DrawCylinder(float x, float y, float z, float size, int r, int g, int b)
    {
        Renderer* renderer = Engine_GetRenderer();
        if (renderer)
            renderer->AddPrimitiveToDrawList(Primitive3D::Cylinder, Vector3{x, y, z}, Vector3{0.f, 0.f, 0.f}, Vector3{size, size, size}, MakeColor(r, g, b));
    }

    void DrawCubeTextured(float x, float y, float z, float size, int textureId)
    {
        Renderer* renderer = Engine_GetRenderer();
        if (renderer)
            renderer->AddPrimitiveToDrawList(Primitive3D::Cube, Vector3{x, y, z}, Vector3{0.f, 0.f, 0.f}, Vector3{size, size, size}, Color3{1.0f, 1.0f, 1.0f}, static_cast<int32_t>(textureId));
    }

    // --- Input ------------------------------------------------------------------
    bool IsPadPressed(int pad, const char* button)
    {
        GamePadButton b = ButtonFromName(button);
        if (b == GamePadButton::Unknown)
            return false;
        return IsGamePadButtonPressed(static_cast<uint8_t>(pad), b);
    }

    void GetJoyAxis(int pad, const char* side, float* outX, float* outY)
    {
        float x = 0.0f, y = 0.0f;
        GamePadJoystick joy = GamePadJoystick::UnknownJoystick;
        if (strcmp(side, "left") == 0)
            joy = GamePadJoystick::LeftJoystick;
        else if (strcmp(side, "right") == 0)
            joy = GamePadJoystick::RightJoystick;
        else
            Engine_LogError("[Game] GetJoyAxis: unknown side '%s' (expected 'left' or 'right')", side);

        if (joy != GamePadJoystick::UnknownJoystick)
        {
            const Vector2 v = GetGamePadAxis(static_cast<uint8_t>(pad), joy);
            x = v.x;
            y = v.y;
        }
        if (outX)
            *outX = x;
        if (outY)
            *outY = y;
    }

    // --- Resources --------------------------------------------------------------
    int LoadResource(const char* type, const char* path)
    {
        ResourceType t = RES_TEXTURE;
        if (strcmp(type, "TEXTURE") == 0)
            t = RES_TEXTURE;
        else if (strcmp(type, "MODEL") == 0)
            t = RES_MODEL;
        else if (strcmp(type, "SOUND") == 0)
            t = RES_SOUND;
        else if (strcmp(type, "FONT") == 0)
            t = RES_FONT;
        else
            Engine_LogError("[Game] LoadResource: unknown type '%s'", type);

        return static_cast<int>(Engine_Resource_Load(t, path));
    }

    bool IsResourceReady(int handle) { return Engine_Resource_IsReady(static_cast<int32_t>(handle)); }

    // --- Entities / spawning ----------------------------------------------------
    namespace
    {
        SpawnHandler s_SpawnHandler = nullptr;
    }

    void SetSpawnHandler(SpawnHandler handler) { s_SpawnHandler = handler; }

    // --- Levels -----------------------------------------------------------------
    namespace
    {
        Level s_GameLevel;
        bool s_LevelLoaded = false;
    } // namespace

    bool LoadLevel(const char* name)
    {
        if (s_LevelLoaded)
        {
            Engine_Level_Unload(&s_GameLevel, false);
            s_LevelLoaded = false;
        }
        memset(&s_GameLevel, 0, sizeof(s_GameLevel));
        strncpy(s_GameLevel.name, name, sizeof(s_GameLevel.name) - 1);
        s_LevelLoaded = Engine_Level_Load(&s_GameLevel);
        return s_LevelLoaded;
    }

    void UnloadLevel()
    {
        if (s_LevelLoaded)
        {
            Engine_Level_Unload(&s_GameLevel, false);
            s_LevelLoaded = false;
        }
    }

    void SetStreamingCenter(float x, float y, float z)
    {
        (void)y; // streaming is on the X/Z ground plane
        if (s_LevelLoaded)
            Engine_Level_SetStreamingCenter(x, z);
    }

} // namespace game

bool Engine_Game_DispatchSpawn(const game::EntitySpawn& spawn)
{
    if (!game::s_SpawnHandler)
    {
        Engine_LogError("[Game] no spawn handler registered — cannot spawn '%s'", spawn.classname ? spawn.classname : "(null)");
        return false;
    }
    return game::s_SpawnHandler(spawn);
}
