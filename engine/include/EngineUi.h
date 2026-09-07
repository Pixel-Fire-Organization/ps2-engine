#pragma once

#include <cstdint>

#include "graphics/UI.h"

/// A colour role. Widgets ask for a role, never for a literal colour, so a theme
/// is one value that can be replaced whole. See docs/subsystems/UI.md.
enum class UiColor : uint8_t
{
    WindowBackground = 0,
    PanelBackground,
    Border,
    Header,
    Text,
    TextDim,
    TextAccent,
    TextWarn,
    ItemBackground,
    ItemHovered,
    ItemActive,
    Focus,
    BarTrack,
    BarFill,
    BarFillWarn,
    Cursor,
    CursorOutline,

    Count
};

struct UiRgba
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

/// Colours and metrics for the whole interface.
struct UiStyle
{
    UiRgba colors[static_cast<uint8_t>(UiColor::Count)];
    int16_t panelPadding;
    int16_t itemSpacing;
    int16_t borderWidth;
    int16_t textScale;
    int16_t rowPadding;
    int16_t barHeight;
    int16_t cursorSize;
};

/// Which device last moved the cursor. The stick source exists on every
/// platform; the others appear only where the platform reports the capability.
enum class UiPointerSource : uint8_t
{
    None = 0,
    Stick,
    Mouse,
    Touch,

    Count
};

/// The single cursor, whichever device is currently driving it.
struct UiPointer
{
    float x;
    float y;
    float stickSpeed;
    UiPointerSource source;
    bool down;
    bool pressed;
    bool visible;
};

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

/// Bring the interface up. Requires the renderer and input to be running.
/// @return False when the renderer is absent.
bool Engine_Ui_Init();

void Engine_Ui_Shutdown();

/// @return Whether widget calls will do anything this run.
bool Engine_Ui_IsActive();

/// Open the frame: reads input into the cursor and navigation state, and clears
/// the quad buffer. Widget calls outside a frame are refused and reported.
void Ui_BeginFrame();

/// Close the frame: resolves focus for the next frame and submits the quads to
/// the active renderer.
void Ui_EndFrame();

// ---------------------------------------------------------------------------
// Containers and layout
// ---------------------------------------------------------------------------

/// Open a panel at an explicit screen position. Widgets stack inside it, in the
/// order they are asked for.
/// @param title Drawn as the panel header; pass an empty string for no header.
/// @param x Left edge in pixels.
/// @param y Top edge in pixels.
/// @param w Width in pixels.
/// @param h Height in pixels.
void Ui_BeginPanel(const char* title, int x, int y, int w, int h);

void Ui_EndPanel();

/// Advance the layout cursor without drawing anything.
/// @param pixels How far down to move.
void Ui_Spacing(int pixels);

/// A full-width rule.
void Ui_Separator();

/// A non-selectable category heading, used to group the rows under it.
/// @param text The heading.
void Ui_Header(const char* text);

/// @return The width in pixels available inside the open panel.
int Ui_ContentWidth();

/// @return The layout cursor's current y position in screen pixels.
int Ui_CursorY();

// ---------------------------------------------------------------------------
// Widgets
// ---------------------------------------------------------------------------

void Ui_Label(const char* text);

/// @param text The line to draw.
/// @param role Which colour role to draw it in.
void Ui_LabelColored(const char* text, UiColor role);

/// A label on the left and a value right-aligned against the panel edge.
/// @param label The name.
/// @param value The value, already formatted.
void Ui_LabelValue(const char* label, const char* value);

/// @param label The name, which is also the widget identity.
/// @return True on the frame it is activated.
bool Ui_Button(const char* label);

/// A row that can be picked out of a list.
/// @param label The name, which is also the widget identity.
/// @param selected Whether to draw it as the current choice.
/// @return True on the frame it is activated.
bool Ui_Selectable(const char* label, bool selected);

/// @param label The name, which is also the widget identity.
/// @param value Read for the current state, written when it changes.
/// @return True on the frame the value changed.
bool Ui_Checkbox(const char* label, bool* value);

/// A row whose identity is its label and whose right-hand text may change.
/// Use this rather than baking state into the label: identity is derived from
/// the label, so a label that changes is a different widget and focus jumps off
/// it at the moment the state it displays changes.
/// @param label The name, which is also the widget identity.
/// @param value Text drawn right-aligned; may differ every frame.
/// @param selected Whether to draw it as the current choice.
/// @return True on the frame it is activated.
bool Ui_SelectableValue(const char* label, const char* value, bool selected);

/// @param label The name, which is also the widget identity.
/// @param value Read for the current position, written when it changes.
/// @param minimum Lowest value the slider can reach.
/// @param maximum Highest value the slider can reach.
/// @return True on the frame the value changed.
bool Ui_SliderInt(const char* label, int* value, int minimum, int maximum);

/// A filled meter with its label and figures above it.
/// @param label The name.
/// @param value Current amount.
/// @param maximum Full-scale amount; a zero draws an empty track.
void Ui_Bar(const char* label, int value, int maximum);

/// A line strip over a fixed window of samples.
/// @param label The name.
/// @param values Samples, oldest first.
/// @param count How many samples to read.
/// @param minimum Value at the bottom of the plot.
/// @param maximum Value at the top of the plot.
/// @param marker A horizontal reference line; pass a value outside the range to
///        omit it.
void Ui_Plot(const char* label, const float* values, int count, float minimum, float maximum, float marker);

/// A framed box the caller can plot normalised points into, for visualising a
/// surface that has no pixel correspondence to the screen.
/// @param label The name.
/// @param height Box height in pixels.
void Ui_BeginPointBox(const char* label, int height);

/// @param nx Horizontal position in [0,1] across the open box.
/// @param ny Vertical position in [0,1] down the open box.
/// @param role Which colour role to draw the point in.
void Ui_Point(float nx, float ny, UiColor role);

void Ui_EndPointBox();

// ---------------------------------------------------------------------------
// Raw drawing, for scenes that need to place something exactly
// ---------------------------------------------------------------------------

/// @param x Left edge in screen pixels.
/// @param y Top edge in screen pixels.
/// @param w Width in pixels.
/// @param h Height in pixels.
/// @param role Which colour role to fill with.
void Ui_Rect(int x, int y, int w, int h, UiColor role);

/// @param x Left edge in screen pixels.
/// @param y Top edge in screen pixels.
/// @param w Width in pixels.
/// @param h Height in pixels.
/// @param color The exact colour to fill with.
void Ui_RectRgba(int x, int y, int w, int h, UiRgba color);

/// @param x Left edge in screen pixels.
/// @param y Top edge in screen pixels.
/// @param scale Whole-pixel size of one font dot.
/// @param text The string; characters outside the font draw blank.
/// @param role Which colour role to draw in.
/// @return The x position just past the string.
int Ui_Text(int x, int y, int scale, const char* text, UiColor role);

// ---------------------------------------------------------------------------
// Input and navigation
// ---------------------------------------------------------------------------

/// @return True on the frame the back action was taken.
bool Ui_WasBackPressed();

/// @return The cursor, whichever device is currently driving it.
const UiPointer& Ui_GetPointer();

// ---------------------------------------------------------------------------
// Style
// ---------------------------------------------------------------------------

const UiStyle& Ui_GetStyle();
void Ui_SetStyle(const UiStyle& style);

/// @return The built-in theme, so a caller can restore it after editing.
UiStyle Ui_DefaultStyle();

/// @param role Which colour to read.
/// @return The colour that role currently resolves to.
UiRgba Ui_GetColor(UiColor role);

/// @param role Which colour to write.
/// @param value The colour to use from now on.
void Ui_SetColor(UiColor role, UiRgba value);

/// @param role The role to name.
/// @return Its short name, for a style editor.
const char* Ui_ColorName(UiColor role);

// ---------------------------------------------------------------------------
// Font
// ---------------------------------------------------------------------------

/// Whole-pixel dimensions of one glyph cell at scale 1.
enum : int
{
    UI_GLYPH_W = 5,
    UI_GLYPH_H = 7,
    UI_GLYPH_ADVANCE = 6
};

/// @param scale Whole-pixel size of one font dot.
/// @param text The string to measure.
/// @return Its width in screen pixels.
int Ui_TextWidth(int scale, const char* text);

/// @param scale Whole-pixel size of one font dot.
/// @return A line's height in screen pixels.
int Ui_TextHeight(int scale);

/// @param scale Whole-pixel size of one font dot.
/// @param text The string to measure.
/// @return How many quads drawing it would cost, so a caller can check itself
///         against the per-frame budget before committing to a layout.
int Ui_MeasureTextQuads(int scale, const char* text);

/// @return The quads used so far this frame.
uint32_t Ui_QuadsUsed();

/// @return The per-frame quad budget on this platform.
uint32_t Ui_QuadBudget();
