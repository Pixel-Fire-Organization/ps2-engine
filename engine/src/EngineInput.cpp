#include "EngineInput.h"

#include <input.h>

#include "Constants.h"
#include "EngineApp.h"

static pad_t* openedGamePads[MAX_GAME_PAD_PORTS] = {nullptr, nullptr};
static Vector2 joystickPositions[MAX_GAME_PAD_PORTS][MAX_JOYSTICKS] = {0};

static void PollPad(uint8_t port);

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

    pad_t* pad = pad_open(port, 0, MODE_DIGITAL, locked);
    if (!pad)
    {
        return false;
    }

    const bool valid = pad->port == port && pad->lock == locked;

    if (valid)
    {
        openedGamePads[port] = pad;
        return true;
    }

    return false;
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

bool IsGamePadButtonPressed(uint8_t port, GamePadButton button)
{
    if (!openedGamePads[port])
    {
        Engine_LogError("Tried to get game pad button status, without it being initialized! Port: %u", port);
        return false;
    }

    PollPad(port);
    const uint32_t buttonMask = padGetButtonMask(port, 0);

    return (buttonMask & static_cast<uint16_t>(button)) != 0;
}

Vector2 GetGamePadAxis(uint8_t port, GamePadJoystick axis)
{
    if (!openedGamePads[port])
    {
        Engine_LogError("Tried to get game pad joystick status, without it being initialized! Port: %u", port);
        return Vector2{0, 0};
    }

    PollPad(port);
    pad_get_buttons(openedGamePads[port]);

    if (axis == GamePadJoystick::LeftJoystick)
        return Vector2{static_cast<float>(openedGamePads[port]->buttons->ljoy_h),
                       static_cast<float>(openedGamePads[port]->buttons->ljoy_v)};
    if (axis == GamePadJoystick::RightJoystick)
        return Vector2{static_cast<float>(openedGamePads[port]->buttons->rjoy_h),
                       static_cast<float>(openedGamePads[port]->buttons->rjoy_v)};

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
}
