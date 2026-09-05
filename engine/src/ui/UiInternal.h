#pragma once

#include "EngineUi.h"

/// Per-frame interface state, shared between the core and the widgets.
struct UiFrameState
{
    int screenW;
    int screenH;

    int panelX;
    int panelY;
    int panelW;
    int panelH;
    int contentX;
    int contentW;
    int cursorY;

    int boxX;
    int boxY;
    int boxW;
    int boxH;

    uint32_t focusId;
    uint32_t activeId;
    uint32_t hotId;

    uint32_t focusables[UI_MAX_FOCUSABLES];
    uint16_t focusableCount;
    int focusIndex;
    int navDelta;

    UiPointer pointer;
    bool pointerMoved;
    bool accept;
    bool back;
    bool inFrame;
    bool inPanel;
    bool inBox;
    bool collisionReported;
};

/// @return The live frame state.
UiFrameState& UiInternal_State();

/// @return Whether the subsystem is up and a frame is open, so a widget may draw.
bool UiInternal_CanDraw();

/// Append one solid screen-space rectangle to this frame's quads.
/// @param x Left edge in screen pixels.
/// @param y Top edge in screen pixels.
/// @param w Width in pixels; a non-positive value draws nothing.
/// @param h Height in pixels; a non-positive value draws nothing.
/// @param color The colour to fill with.
void UiInternal_PushRect(int x, int y, int w, int h, UiRgba color);

/// A one-pixel-thick frame around a rectangle.
/// @param x Left edge in screen pixels.
/// @param y Top edge in screen pixels.
/// @param w Width in pixels.
/// @param h Height in pixels.
/// @param thickness Border width in pixels.
/// @param color The colour to draw the border in.
void UiInternal_PushBorder(int x, int y, int w, int h, int thickness, UiRgba color);

/// @param label The widget label, which carries its identity.
/// @return A stable identifier for it.
uint32_t UiInternal_Id(const char* label);

/// Record a widget as reachable by directional navigation, in call order, and
/// report a duplicate identity once per frame.
/// @param id The widget identity.
/// @return Whether this widget currently holds focus.
bool UiInternal_RegisterFocusable(uint32_t id);

/// @param x Left edge in screen pixels.
/// @param y Top edge in screen pixels.
/// @param w Width in pixels.
/// @param h Height in pixels.
/// @return Whether the visible cursor is inside the rectangle.
bool UiInternal_PointerOver(int x, int y, int w, int h);

/// Take the next row of the open panel and advance the layout cursor past it.
/// @param height Row height in pixels.
/// @param outX Receives the row's left edge.
/// @param outY Receives the row's top edge.
/// @param outW Receives the row's width.
/// @return False when no panel is open, in which case nothing is written.
bool UiInternal_TakeRow(int height, int* outX, int* outY, int* outW);

/// Draw a string with the built-in bitmap font.
/// @param x Left edge in screen pixels.
/// @param y Top edge in screen pixels.
/// @param scale Whole-pixel size of one font dot.
/// @param text The string; characters outside the font draw blank.
/// @param color The colour to draw in.
/// @return The x position just past the string.
int UiFont_Draw(int x, int y, int scale, const char* text, UiRgba color);
