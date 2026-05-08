#include "EngineInput.h"

extern "C" {
#include <input.h>
#include <loadfile.h>
}

#include <cmath>
#include "Constants.h"
#include "EngineApp.h"

static pad_t* openedGamePads[MAX_GAME_PAD_PORTS] = {nullptr, nullptr};

// Map a raw PS2 analog byte [0, 255] to a normalised float [-1, +1] and apply
// a symmetric deadzone.  Values inside the dead zone return exactly 0.0f so
// that idle sticks drifting off their mechanical center produce no movement.
static float NormalizeAxis(uint8_t raw)
{
    float v = (static_cast<float>(raw) - INPUT_ANALOG_RAW_CENTER) / INPUT_ANALOG_RAW_SCALE;
    if (v > 1.0f)
        v = 1.0f;
    if (v < -1.0f)
        v = -1.0f;
    return (fabsf(v) < INPUT_ANALOG_DEADZONE) ? 0.0f : v;
}
static Vector2 joystickPositions[MAX_GAME_PAD_PORTS][MAX_JOYSTICKS] = {};
static bool s_PadSystemInitDone = false;

static void PollPad(uint8_t port);
static bool InitPadSystem();

bool InitPad(uint8_t port, bool locked)
{
    if (port >= MAX_GAME_PAD_PORTS)
    {
        Engine_LogError("Out of range game pad port was passed! Port: %u", port);
        return false;
    }

    if (openedGamePads[port])
    {
        Engine_LogInfo("Tried to open a game pad port twice! Port: %u", port);
        return true;
    }

    if (!InitPadSystem())
        return false;

    pad_t* pad = pad_open(port, 0, MODE_ANALOG, locked);
    if (!pad)
    {
        Engine_LogError("pad_open failed for port %u", port);
        return false;
    }

    openedGamePads[port] = pad;
    return true;
}


void ShutdownAllPads()
{
    for (auto i = 0; i < MAX_GAME_PAD_PORTS; i++)
    {
        if (!openedGamePads[i])
            continue;

        pad_close(openedGamePads[i]);
        openedGamePads[i] = nullptr;
    }
}

bool IsGamePadInitialized(uint8_t port)
{
    if (port >= MAX_GAME_PAD_PORTS)
        return false;
    return openedGamePads[port] != nullptr;
}

bool IsGamePadButtonPressed(uint8_t port, GamePadButton button)
{
    if (!openedGamePads[port])
    {
        Engine_LogError("Tried to get game pad button status, without it being initialized! Port: %u", port);
        return false;
    }

    PollPad(port);
    const uint16_t buttonMask = ~openedGamePads[port]->buttons->btns;
    return (buttonMask & static_cast<uint16_t>(button)) == static_cast<uint16_t>(button);
}

Vector2 GetGamePadAxis(uint8_t port, GamePadJoystick axis)
{
    if (!openedGamePads[port])
    {
        Engine_LogError("Tried to get game pad joystick status, without it being initialized! Port: %u", port);
        return Vector2{0, 0};
    }

    PollPad(port);

    if (axis == GamePadJoystick::LeftJoystick)
        return Vector2{NormalizeAxis(openedGamePads[port]->buttons->ljoy_h), NormalizeAxis(openedGamePads[port]->buttons->ljoy_v)};
    if (axis == GamePadJoystick::RightJoystick)
        return Vector2{NormalizeAxis(openedGamePads[port]->buttons->rjoy_h), NormalizeAxis(openedGamePads[port]->buttons->rjoy_v)};

    return Vector2{0, 0};
}

Vector2* GetGamePadPosition(uint8_t port)
{
    if (!openedGamePads[port])
    {
        Engine_LogError("Tried to get game pad joysticks status, without it being initialized! Port: %u", port);
        return nullptr;
    }

    joystickPositions[port][0] = GetGamePadAxis(port, GamePadJoystick::LeftJoystick);
    joystickPositions[port][1] = GetGamePadAxis(port, GamePadJoystick::RightJoystick);

    return joystickPositions[port];
}

uint8_t GetKeyboardButtonPressed() { return 0; }

static void PollPad(uint8_t port)
{
    if (!openedGamePads[port])
        return;

    pad_wait(openedGamePads[port]);
    pad_get_buttons(openedGamePads[port]);
}

bool InitPadSystem()
{
    if (s_PadSystemInitDone) // ← lazy-init guard: IOP already up, nothing to do
        return true;

    Engine_LogInfo("EngineInput: Loading IOP pad modules (first pad open)...");

    int ret = SifLoadModule("rom0:SIO2MAN", 0, nullptr);
    if (ret < 0)
    {
        Engine_LogError("EngineInput: Failed to load SIO2MAN IOP module: %d", ret);
        return false;
    }

    ret = SifLoadModule("rom0:PADMAN", 0, nullptr);
    if (ret < 0)
    {
        Engine_LogError("EngineInput: Failed to load PADMAN IOP module: %d", ret);
        return false;
    }

    padInit(0);
    s_PadSystemInitDone = true;
    Engine_LogInfo("EngineInput: IOP pad system ready.");
    return true;
}
