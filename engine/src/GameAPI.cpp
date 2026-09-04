// ---------------------------------------------------------------------------
// GameAPI.cpp — engine-side implementation of the friendly C++ game API.
//
// Every function here is a thin marshalling wrapper over the same engine
// services the retired Lua bindings called (Renderer, EngineInput,
// Engine_Resource_*, core timing). Gameplay now runs natively: no interpreter
// and no FFI crossing in the per-frame hot loop.
// ---------------------------------------------------------------------------

#include "GameAPI.h"

#include <cstdlib>
#include <cstring>

#include "EngineAchievement.h"
#include "EngineCore.h"
#include "EngineDebug.h"
#include "EngineInput.h"
#include "EngineLevel.h"
#include "EngineResource.h"
#include "PlatformConstants.h"
#include "graphics/Primitives.h"
#include "graphics/Renderer.h"
#include "graphics/Types.h"
#include "platform/Platform.h"

// Defined in EngineApp.cpp — sets the internal exit flag polled by EngineExited().
void EngineApp_OnExitRequested();

namespace
{

    inline Color3 MakeColor(int r, int g, int b) { return Color3{static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f, static_cast<float>(b) / 255.0f}; }

    // Map a key name to the platform enum. Kept string-keyed because GameAPI is
    // the documented public surface and plain scalars are its stated design; the
    // enums are the platform interface, and this is the seam between them.
    KeyboardKey KeyFromName(const char* name)
    {
        if (!name || !name[0])
            return KeyboardKey::Unknown;

        // Single characters cover a-z and 0-9 without a 36-entry table.
        if (name[1] == '\0')
        {
            const char c = name[0];
            if (c >= 'a' && c <= 'z')
                return static_cast<KeyboardKey>(static_cast<uint16_t>(KeyboardKey::A) + (c - 'a'));
            if (c >= 'A' && c <= 'Z')
                return static_cast<KeyboardKey>(static_cast<uint16_t>(KeyboardKey::A) + (c - 'A'));
            if (c >= '0' && c <= '9')
                return static_cast<KeyboardKey>(static_cast<uint16_t>(KeyboardKey::Num0) + (c - '0'));
        }

        // f1..f12
        if ((name[0] == 'f' || name[0] == 'F') && name[1] >= '0' && name[1] <= '9')
        {
            int n = atoi(name + 1);
            if (n >= 1 && n <= 12)
                return static_cast<KeyboardKey>(static_cast<uint16_t>(KeyboardKey::F1) + (n - 1));
        }

        static const struct
        {
            const char* name;
            KeyboardKey key;
        } kNamed[] = {
            {"up", KeyboardKey::Up},
            {"down", KeyboardKey::Down},
            {"left", KeyboardKey::Left},
            {"right", KeyboardKey::Right},
            {"space", KeyboardKey::Space},
            {"enter", KeyboardKey::Enter},
            {"return", KeyboardKey::Enter},
            {"escape", KeyboardKey::Escape},
            {"esc", KeyboardKey::Escape},
            {"tab", KeyboardKey::Tab},
            {"backspace", KeyboardKey::Backspace},
            {"delete", KeyboardKey::Delete},
            {"insert", KeyboardKey::Insert},
            {"home", KeyboardKey::Home},
            {"end", KeyboardKey::End},
            {"pageup", KeyboardKey::PageUp},
            {"pagedown", KeyboardKey::PageDown},
            {"shift", KeyboardKey::LeftShift},
            {"lshift", KeyboardKey::LeftShift},
            {"rshift", KeyboardKey::RightShift},
            {"ctrl", KeyboardKey::LeftControl},
            {"lctrl", KeyboardKey::LeftControl},
            {"rctrl", KeyboardKey::RightControl},
            {"alt", KeyboardKey::LeftAlt},
            {"lalt", KeyboardKey::LeftAlt},
            {"ralt", KeyboardKey::RightAlt},
            {"minus", KeyboardKey::Minus},
            {"equal", KeyboardKey::Equal},
            {"comma", KeyboardKey::Comma},
            {"period", KeyboardKey::Period},
            {"slash", KeyboardKey::Slash},
            {"backslash", KeyboardKey::Backslash},
            {"semicolon", KeyboardKey::Semicolon},
            {"apostrophe", KeyboardKey::Apostrophe},
            {"grave", KeyboardKey::Grave},
            {"lbracket", KeyboardKey::LeftBracket},
            {"rbracket", KeyboardKey::RightBracket},
        };

        for (size_t i = 0; i < sizeof(kNamed) / sizeof(kNamed[0]); ++i)
        {
            if (strcmp(name, kNamed[i].name) == 0)
                return kNamed[i].key;
        }
        return KeyboardKey::Unknown;
    }

    // Map a button name to the engine enum. Mirrors the old input.is_pad_pressed
    // binding one-for-one so ported scripts behave identically.
    GamepadButton ButtonFromName(const char* btn)
    {
        if (strcmp(btn, "x") == 0)
            return GamepadButton::Cross;
        if (strcmp(btn, "cir") == 0)
            return GamepadButton::Circle;
        if (strcmp(btn, "squ") == 0)
            return GamepadButton::Square;
        if (strcmp(btn, "tri") == 0)
            return GamepadButton::Triangle;
        if (strcmp(btn, "dpad_up") == 0)
            return GamepadButton::DPadUp;
        if (strcmp(btn, "dpad_down") == 0)
            return GamepadButton::DPadDown;
        if (strcmp(btn, "dpad_left") == 0)
            return GamepadButton::DPadLeft;
        if (strcmp(btn, "dpad_right") == 0)
            return GamepadButton::DPadRight;
        if (strcmp(btn, "l1") == 0)
            return GamepadButton::L1;
        if (strcmp(btn, "l2") == 0)
            return GamepadButton::L2;
        if (strcmp(btn, "r1") == 0)
            return GamepadButton::R1;
        if (strcmp(btn, "r2") == 0)
            return GamepadButton::R2;
        if (strcmp(btn, "l3") == 0)
            return GamepadButton::L3;
        if (strcmp(btn, "r3") == 0)
            return GamepadButton::R3;
        if (strcmp(btn, "start") == 0)
            return GamepadButton::Start;
        if (strcmp(btn, "select") == 0)
            return GamepadButton::Select;
        Engine_LogError("[Game] IsPadPressed: unknown button '%s'", btn);
        return GamepadButton::Unknown;
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
        GamepadButton b = ButtonFromName(button);
        if (b == GamepadButton::Unknown)
            return false;
        return IsGamePadButtonPressed(static_cast<uint8_t>(pad), b);
    }

    void GetJoyAxis(int pad, const char* side, float* outX, float* outY)
    {
        float x = 0.0f, y = 0.0f;
        GamepadStick joy = GamepadStick::Count;
        if (strcmp(side, "left") == 0)
            joy = GamepadStick::Left;
        else if (strcmp(side, "right") == 0)
            joy = GamepadStick::Right;
        else
            Engine_LogError("[Game] GetJoyAxis: unknown side '%s' (expected 'left' or 'right')", side);

        if (joy != GamepadStick::Count)
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

    bool WasPadPressed(int pad, const char* button)
    {
        GamepadButton b = ButtonFromName(button);
        if (b == GamepadButton::Unknown)
            return false;
        return WasGamePadButtonPressed(static_cast<uint8_t>(pad), b);
    }

    bool IsKeyDown(const char* key) { return ::IsKeyDown(KeyFromName(key)); }

    bool WasKeyPressed(const char* key) { return ::WasKeyPressed(KeyFromName(key)); }

    bool IsMouseButtonDown(int button)
    {
        if (button < 0 || button >= static_cast<int>(MouseButton::Count))
            return false;
        return ::IsMouseButtonDown(static_cast<MouseButton>(button));
    }

    bool WasMouseButtonPressed(int button)
    {
        if (button < 0 || button >= static_cast<int>(MouseButton::Count))
            return false;
        return ::WasMouseButtonPressed(static_cast<MouseButton>(button));
    }

    void GetMousePosition(float* outX, float* outY)
    {
        const Vector2 p = ::GetMousePosition();
        if (outX)
            *outX = p.x;
        if (outY)
            *outY = p.y;
    }

    void GetMouseDelta(float* outX, float* outY)
    {
        const Vector2 d = ::GetMouseDelta();
        if (outX)
            *outX = d.x;
        if (outY)
            *outY = d.y;
    }

    float GetMouseWheel() { return ::GetMouseWheelDelta(); }

    namespace
    {
        TouchSurface ParseTouchSurface(const char* surface)
        {
            if (!surface)
                return TouchSurface::Count;
            if (strcmp(surface, "front") == 0)
                return TouchSurface::Front;
            if (strcmp(surface, "rear") == 0)
                return TouchSurface::Rear;
            Engine_LogError("game: unknown touch surface '%s'", surface);
            return TouchSurface::Count;
        }
    } // namespace

    int GetTouchCount(const char* surface)
    {
        Platform* platform = Engine_GetPlatform();
        const TouchSurface s = ParseTouchSurface(surface);
        if (!platform || s == TouchSurface::Count)
            return 0;
        return static_cast<int>(platform->Touch_GetContactCount(s));
    }

    bool GetTouch(const char* surface, int index, float* outX, float* outY)
    {
        Platform* platform = Engine_GetPlatform();
        const TouchSurface s = ParseTouchSurface(surface);
        if (!platform || s == TouchSurface::Count || index < 0 || index > 255)
            return false;

        TouchContact contact;
        if (!platform->Touch_GetContact(s, static_cast<uint8_t>(index), &contact))
            return false;

        if (outX)
            *outX = contact.position.x;
        if (outY)
            *outY = contact.position.y;
        return true;
    }

    bool HasInputDevice(const char* device)
    {
        Platform* platform = Engine_GetPlatform();
        if (!platform || !device)
            return false;

        if (strcmp(device, "gamepad") == 0)
            return platform->HasCapability(PlatformCapability::Gamepad);
        if (strcmp(device, "keyboard") == 0)
            return platform->HasCapability(PlatformCapability::Keyboard);
        if (strcmp(device, "mouse") == 0)
            return platform->HasCapability(PlatformCapability::Mouse);
        if (strcmp(device, "touch") == 0)
            return platform->HasCapability(PlatformCapability::Touch);

        Engine_LogError("game::HasInputDevice: unknown device '%s'", device);
        return false;
    }

    // --- Achievements -----------------------------------------------------------
    bool UnlockAchievement(int id)
    {
        if (id < 0)
        {
            Engine_LogError("game::UnlockAchievement: negative id %d", id);
            return false;
        }
        return Engine_Achievement_Unlock(static_cast<uint32_t>(id));
    }

    bool IsAchievementUnlocked(int id) { return (id >= 0) && Engine_Achievement_IsUnlocked(static_cast<uint32_t>(id)); }

    bool HasAchievements() { return Engine_Achievement_IsAvailable(); }

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
