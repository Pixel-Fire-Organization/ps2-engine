#pragma once

#define MAX_GAME_PAD_PORTS 2
#define MAX_JOYSTICKS 2

// PS2 analog sticks report a raw byte in [0, 255] with 128 as center.
// INPUT_ANALOG_RAW_CENTER / INPUT_ANALOG_RAW_SCALE are used to map the byte
// to [-1, +1].  Values whose absolute magnitude fall below
// INPUT_ANALOG_DEADZONE are clamped to 0 so idle sticks that drift slightly
// off-center (common on real PS2 hardware) produce no movement.
#define INPUT_ANALOG_RAW_CENTER 128
#define INPUT_ANALOG_RAW_SCALE 127.0f
#define INPUT_ANALOG_DEADZONE 0.25f
