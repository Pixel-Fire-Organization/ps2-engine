#pragma once

#include <cstdint>
#include "graphics/Types.h"

#ifdef __cplusplus
extern "C" {
#endif

enum class GamePadButton : uint16_t
{
    Unknown = 0, // Never use!

    Select = 0x0001,
    L3 = 0x0002,
    R3 = 0x0004,
    Start = 0x0008,

    DPadUp = 0x0010,
    DPadRight = 0x0020,
    DPadDown = 0x0040,
    DPadLeft = 0x0080,

    R2 = 0x0100,
    R1 = 0x0200,
    L2 = 0x0400,
    L1 = 0x0800,

    Triangle = 0x1000,
    Circle = 0x2000,
    Cross = 0x4000,
    Square = 0x8000,

};

enum class GamePadJoystick : uint8_t
{
    UnknownJoystick = 0, // never use

    LeftJoystick,
    RightJoystick
};

bool InitPad(uint8_t port, bool locked);
void ShutdownAllPads();

// Returns true if the pad at 'port' was successfully opened via InitPad.
bool IsGamePadInitialized(uint8_t port);

void EngineInput_PollAllPads();

bool IsGamePadButtonPressed(uint8_t port, GamePadButton button);
Vector2 GetGamePadAxis(uint8_t port, GamePadJoystick axis);
Vector2* GetGamePadPosition(uint8_t port);

uint8_t GetKeyboardButtonPressed();

#ifdef __cplusplus
};
#endif
