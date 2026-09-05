#include "EngineUi.h"

#include <cstring>

#include "EngineCore.h"
#include "EngineDebug.h"
#include "EngineInput.h"
#include "UiInternal.h"
#include "graphics/Renderer.h"
#include "platform/Platform.h"

namespace
{
    const float CURSOR_SPEED_SLOW = 0.35f;
    const float CURSOR_SPEED_FAST = 1.60f;
    const float CURSOR_RAMP_SECONDS = 0.45f;
    const uint32_t FNV_OFFSET_BASIS = 2166136261u;
    const uint32_t FNV_PRIME = 16777619u;

    bool s_Active = false;
    UI s_Ui;
    UiFrameState s_State;
    float s_StickHeld = 0.0f;

    void ReadScreenSize()
    {
        const Platform* platform = Engine_GetPlatform();
        s_State.screenW = platform ? static_cast<int>(platform->GetConstant(PlatformConstant::ScreenWidth)) : GFX_SCREEN_WIDTH;
        s_State.screenH = platform ? static_cast<int>(platform->GetConstant(PlatformConstant::ScreenHeight)) : GFX_SCREEN_HEIGHT;
    }

    void ClampPointer()
    {
        const float maxX = static_cast<float>(s_State.screenW - 1);
        const float maxY = static_cast<float>(s_State.screenH - 1);
        if (s_State.pointer.x < 0.0f)
            s_State.pointer.x = 0.0f;
        if (s_State.pointer.y < 0.0f)
            s_State.pointer.y = 0.0f;
        if (s_State.pointer.x > maxX)
            s_State.pointer.x = maxX;
        if (s_State.pointer.y > maxY)
            s_State.pointer.y = maxY;
    }

    void UpdateStickPointer(float dt)
    {
        const Vector2 stick = GetGamePadAxis(0, GamepadStick::Left);
        const float magnitude = (stick.x * stick.x) + (stick.y * stick.y);
        if (magnitude <= 0.0f)
        {
            s_StickHeld = 0.0f;
            s_State.pointer.stickSpeed = 0.0f;
            return;
        }

        s_StickHeld += dt;
        float ramp = s_StickHeld / CURSOR_RAMP_SECONDS;
        if (ramp > 1.0f)
            ramp = 1.0f;

        const float screens = CURSOR_SPEED_SLOW + (CURSOR_SPEED_FAST - CURSOR_SPEED_SLOW) * ramp;
        const float speed = screens * static_cast<float>(s_State.screenW);

        s_State.pointer.x += stick.x * speed * dt;
        s_State.pointer.y += stick.y * speed * dt;
        s_State.pointer.stickSpeed = speed;
        s_State.pointer.source = UiPointerSource::Stick;
        s_State.pointer.visible = true;
        s_State.pointerMoved = true;
    }

    void UpdateMousePointer()
    {
        const Platform* platform = Engine_GetPlatform();
        if (!platform || !platform->HasCapability(PlatformCapability::Mouse))
            return;

        const Vector2 delta = GetMouseDelta();
        const bool clicked = WasMouseButtonPressed(MouseButton::Left);
        if (delta.x == 0.0f && delta.y == 0.0f && !clicked)
            return;

        const Vector2 position = GetMousePosition();
        s_State.pointer.x = position.x;
        s_State.pointer.y = position.y;
        s_State.pointer.source = UiPointerSource::Mouse;
        s_State.pointer.visible = true;
        s_State.pointerMoved = true;
    }

    void UpdateTouchPointer()
    {
        Platform* platform = Engine_GetPlatform();
        if (!platform || !platform->HasCapability(PlatformCapability::Touch))
            return;
        if (platform->Touch_GetContactCount(TouchSurface::Front) == 0)
            return;

        TouchContact contact;
        if (!platform->Touch_GetContact(TouchSurface::Front, 0, &contact))
            return;

        s_State.pointer.x = contact.position.x * static_cast<float>(s_State.screenW);
        s_State.pointer.y = contact.position.y * static_cast<float>(s_State.screenH);
        s_State.pointer.source = UiPointerSource::Touch;
        s_State.pointer.visible = true;
        s_State.pointerMoved = true;
    }

    bool PointerHeld()
    {
        if (s_State.pointer.source == UiPointerSource::Touch)
        {
            Platform* platform = Engine_GetPlatform();
            return platform && platform->Touch_GetContactCount(TouchSurface::Front) > 0;
        }
        if (s_State.pointer.source == UiPointerSource::Mouse)
            return IsMouseButtonDown(MouseButton::Left);
        return false;
    }

    bool PointerPressed()
    {
        if (s_State.pointer.source == UiPointerSource::Touch)
            return s_State.pointerMoved && PointerHeld() && !s_State.pointer.down;
        if (s_State.pointer.source == UiPointerSource::Mouse)
            return WasMouseButtonPressed(MouseButton::Left);
        return false;
    }

    void DrawCursor()
    {
        if (!s_State.pointer.visible)
            return;

        const UiStyle& style = Ui_GetStyle();
        const int size = style.cursorSize;
        const int x = static_cast<int>(s_State.pointer.x);
        const int y = static_cast<int>(s_State.pointer.y);
        const UiRgba outline = Ui_GetColor(UiColor::CursorOutline);
        const UiRgba fill = Ui_GetColor(UiColor::Cursor);

        UiInternal_PushRect(x - 1, y - size - 1, 3, size * 2 + 3, outline);
        UiInternal_PushRect(x - size - 1, y - 1, size * 2 + 3, 3, outline);
        UiInternal_PushRect(x, y - size, 1, size * 2 + 1, fill);
        UiInternal_PushRect(x - size, y, size * 2 + 1, 1, fill);
    }
} // namespace

UiFrameState& UiInternal_State() { return s_State; }

bool UiInternal_CanDraw() { return s_Active && s_State.inFrame; }

void UiInternal_PushRect(int x, int y, int w, int h, UiRgba color)
{
    if (!UiInternal_CanDraw() || w <= 0 || h <= 0)
        return;

    UiQuad quad;
    quad.x = static_cast<int16_t>(x);
    quad.y = static_cast<int16_t>(y);
    quad.w = static_cast<int16_t>(w);
    quad.h = static_cast<int16_t>(h);
    quad.u0 = 0;
    quad.v0 = 0;
    quad.u1 = 0;
    quad.v1 = 0;
    quad.texture = 0;
    quad.r = color.r;
    quad.g = color.g;
    quad.b = color.b;
    quad.a = color.a;
    s_Ui.Add(quad);
}

void UiInternal_PushBorder(int x, int y, int w, int h, int thickness, UiRgba color)
{
    if (thickness <= 0)
        return;
    UiInternal_PushRect(x, y, w, thickness, color);
    UiInternal_PushRect(x, y + h - thickness, w, thickness, color);
    UiInternal_PushRect(x, y + thickness, thickness, h - thickness * 2, color);
    UiInternal_PushRect(x + w - thickness, y + thickness, thickness, h - thickness * 2, color);
}

uint32_t UiInternal_Id(const char* label)
{
    uint32_t hash = FNV_OFFSET_BASIS;
    for (const char* p = label; p && *p; ++p)
    {
        hash ^= static_cast<uint32_t>(static_cast<unsigned char>(*p));
        hash *= FNV_PRIME;
    }
    return hash ? hash : 1u;
}

bool UiInternal_RegisterFocusable(uint32_t id)
{
    for (uint16_t i = 0; i < s_State.focusableCount; ++i)
    {
        if (s_State.focusables[i] != id)
            continue;
        if (!s_State.collisionReported)
        {
            s_State.collisionReported = true;
            Engine_LogError("Ui: two widgets share the identity 0x%08X in one frame; they will act as one.", id);
        }
        return s_State.focusId == id;
    }

    if (s_State.focusableCount < UI_MAX_FOCUSABLES)
    {
        if (s_State.focusId == id)
            s_State.focusIndex = static_cast<int>(s_State.focusableCount);
        s_State.focusables[s_State.focusableCount++] = id;
    }
    return s_State.focusId == id;
}

bool UiInternal_PointerOver(int x, int y, int w, int h)
{
    if (!s_State.pointer.visible)
        return false;
    const float px = s_State.pointer.x;
    const float py = s_State.pointer.y;
    return px >= static_cast<float>(x) && px < static_cast<float>(x + w) && py >= static_cast<float>(y) && py < static_cast<float>(y + h);
}

bool UiInternal_TakeRow(int height, int* outX, int* outY, int* outW)
{
    if (!s_State.inPanel)
        return false;

    const UiStyle& style = Ui_GetStyle();
    *outX = s_State.contentX;
    *outY = s_State.cursorY;
    *outW = s_State.contentW;
    s_State.cursorY += height + style.itemSpacing;
    return true;
}

bool Engine_Ui_Init()
{
    if (!Engine_GetRenderer())
    {
        Engine_LogError("Ui: no renderer, interface cannot start");
        return false;
    }

    memset(&s_State, 0, sizeof(s_State));
    s_State.focusIndex = -1;
    s_Ui.Reset();
    s_StickHeld = 0.0f;
    s_Active = true;
    Engine_LogInfo("Ui initialized. Quad budget %u per frame.", static_cast<unsigned>(UI::Capacity()));
    return true;
}

void Engine_Ui_Shutdown()
{
    s_Active = false;
    s_Ui.Reset();
    memset(&s_State, 0, sizeof(s_State));
    s_State.focusIndex = -1;
}

bool Engine_Ui_IsActive() { return s_Active; }

void Ui_BeginFrame()
{
    if (!s_Active)
        return;
    if (s_State.inFrame)
    {
        Engine_LogError("Ui: BeginFrame called with a frame already open");
        return;
    }

    s_State.inFrame = true;
    s_State.inPanel = false;
    s_State.inBox = false;
    s_State.collisionReported = false;
    s_State.focusableCount = 0;
    s_State.focusIndex = -1;
    s_State.hotId = 0;
    s_State.navDelta = 0;
    s_State.pointerMoved = false;
    s_Ui.Reset();

    ReadScreenSize();

    const bool navUp = WasGamePadButtonPressed(0, GamepadButton::DPadUp);
    const bool navDown = WasGamePadButtonPressed(0, GamepadButton::DPadDown);
    if (navUp != navDown)
    {
        s_State.navDelta = navDown ? 1 : -1;
        s_State.pointer.visible = false;
    }

    s_State.accept = WasGamePadButtonPressed(0, GamepadButton::Cross);
    s_State.back = WasGamePadButtonPressed(0, GamepadButton::Circle);

    UpdateStickPointer(Engine_GetDeltaTime());
    UpdateMousePointer();
    UpdateTouchPointer();
    ClampPointer();

    const bool pressed = PointerPressed();
    s_State.pointer.down = PointerHeld();
    s_State.pointer.pressed = pressed;
}

void Ui_EndFrame()
{
    if (!s_Active)
        return;
    if (!s_State.inFrame)
    {
        Engine_LogError("Ui: EndFrame called with no frame open");
        return;
    }
    if (s_State.inPanel)
    {
        Engine_LogError("Ui: a panel was left open at end of frame; nothing submitted");
        s_State.inFrame = false;
        s_State.inPanel = false;
        s_Ui.Reset();
        return;
    }

    if (s_State.focusableCount > 0)
    {
        const int count = static_cast<int>(s_State.focusableCount);
        int index = (s_State.focusIndex < 0) ? 0 : s_State.focusIndex + s_State.navDelta;
        while (index < 0)
            index += count;
        while (index >= count)
            index -= count;
        s_State.focusId = s_State.focusables[index];
        s_State.focusIndex = index;
    }
    else
    {
        s_State.focusId = 0;
    }

    DrawCursor();

    if (s_Ui.Dropped() > 0)
    {
        Engine_LogError("Ui: dropped %u quad(s) this frame (budget %u); the screen needs paging.", static_cast<unsigned>(s_Ui.Dropped()),
                        static_cast<unsigned>(UI::Capacity()));
    }

    Renderer* renderer = Engine_GetRenderer();
    if (renderer)
    {
        const Vector2 offset = Vector2{0.0f, 0.0f};
        const Vector2 scale = Vector2{1.0f, 1.0f};
        renderer->AddUIToDrawList(s_Ui, offset, scale);
    }

    s_State.inFrame = false;
}

bool Ui_WasBackPressed() { return s_Active && s_State.back; }

const UiPointer& Ui_GetPointer() { return s_State.pointer; }

uint32_t Ui_QuadsUsed() { return s_Ui.Count(); }

uint32_t Ui_QuadBudget() { return UI::Capacity(); }
