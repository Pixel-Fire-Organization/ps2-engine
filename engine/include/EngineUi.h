#pragma once

#include <cstddef>
#include <cstdint>

#include "UiThemeIds.h"
#include "graphics/ThemeFormat.h"
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
///
/// This layout is an on-disc contract: a cooked theme is a copy of this block,
/// not a parse of it. Fields may not be reordered or resized without a theme
/// format version change, and the build asserts its size against the value the
/// format declares. See docs/formats/THEME_FORMAT.md.
struct UiStyle
{
    UiRgba colors[static_cast<uint8_t>(UiColor::Count)];
    float repeatDelaySeconds;
    float repeatIntervalSeconds;
    int16_t panelPadding;
    int16_t itemSpacing;
    int16_t borderWidth;
    int16_t textScale;
    int16_t rowPadding;
    int16_t barHeight;
    int16_t cursorSize;
    int16_t screenMargin;
    int16_t panelGap;
    int16_t scrollBarWidth;
    int16_t reserved[2];
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

/// Draw a line straight at a renderer with the built-in font, outside any
/// interface frame.
///
/// For the panic display, which runs when the interface may be down and which
/// never returns. It touches no subsystem state and allocates nothing.
/// @param renderer The renderer to draw into; must have a frame open.
/// @param x Left edge in framebuffer pixels.
/// @param y Top edge in framebuffer pixels.
/// @param scale Whole-pixel size of one font dot.
/// @param text The line to draw.
/// @param color The colour to draw in.
void Engine_DrawPanicText(class Renderer* renderer, int x, int y, int scale, const char* text, UiRgba color);

/// Open the frame: reads input into the cursor and navigation state, and clears
/// the quad buffer. Widget calls outside a frame are refused and reported.
void Ui_BeginFrame();

/// Close the frame: resolves focus for the next frame and submits the quads to
/// the active renderer.
void Ui_EndFrame();

/// Drop everything the interface remembers between scenes: focus, the container
/// stacks, and the cooked font, whose resources the runtime reset has already
/// released. Restores the default theme.
void Ui_ResetRuntimeState();

/// Which fixed region of the screen a panel occupies. Panels stay explicitly
/// placed; this is a small named set, not a layout solver.
enum class UiPanelSlot : uint8_t
{
    Full = 0,
    Left,
    Right,
    Top,
    Bottom,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,

    Count
};

/// How text sits within the width it is given.
enum class UiAlign : uint8_t
{
    Left = 0,
    Center,
    Right,

    Count
};

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

/// Open a panel filling one of the named screen regions, with the theme's
/// screen margin and panel gap applied. Removes the need for every caller to
/// recompute the same halves of the screen.
/// @param title Drawn as the panel header; pass an empty string for no header.
/// @param slot Which region to fill.
void Ui_BeginPanelSlot(const char* title, UiPanelSlot slot);

/// @return The framebuffer width the interface is laid out against.
int Ui_ScreenWidth();

/// @return The framebuffer height the interface is laid out against.
int Ui_ScreenHeight();

/// Open a scrolling region inside the open panel.
///
/// Content is clipped to it and the scroll position is remembered between
/// frames; directional navigation scrolls the focused row into view, so a
/// region that does not fit is scrolled rather than paged. A row entirely
/// outside the region costs nothing: it is neither drawn nor focusable.
/// @param id Identity of the region, which is where its scroll position is filed.
/// @param height Region height in pixels.
/// @return False when no panel is open, in which case nothing is drawn.
bool Ui_BeginScroll(const char* id, int height);

void Ui_EndScroll();

/// Divide the remaining panel width into equal columns.
/// @param count How many columns; one or fewer does nothing.
void Ui_BeginColumns(int count);

/// Move to the next column, resetting the layout cursor to the column top.
void Ui_NextColumn();

/// Close the column set, leaving the cursor below the tallest column.
void Ui_EndColumns();

/// A row that opens and closes a nested group of rows.
/// @param label The name, which is also the widget identity.
/// @param defaultOpen Whether it starts open the first time it is seen.
/// @return Whether the group is open, and its contents should be drawn.
bool Ui_BeginTree(const char* label, bool defaultOpen);

void Ui_EndTree();

/// A strip of pages across the top of a panel.
/// @param id Identity of the strip, which is where the active page is filed.
/// @return False when no panel is open.
bool Ui_BeginTabBar(const char* id);

/// @param label The tab's name.
/// @return Whether this tab is the active one, and its page should be drawn.
bool Ui_Tab(const char* label);

void Ui_EndTabBar();

/// Open an identity scope, so two containers may hold identically-named rows.
/// @param text Text distinguishing this scope from its siblings.
void Ui_PushId(const char* text);

/// @param index Index distinguishing this scope from its siblings.
void Ui_PushIdIndex(int index);

void Ui_PopId();

/// @return The width in pixels available inside the open panel.
int Ui_ContentWidth();

/// @return The height in pixels left in the open panel below the layout cursor.
///         What a scrolling region is usually given.
int Ui_ContentHeight();

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

/// @param label The name, which is also the widget identity.
/// @param value Read for the current position, written when it changes.
/// @param minimum Lowest value the slider can reach.
/// @param maximum Highest value the slider can reach.
/// @param step How far one press moves it.
/// @return True on the frame the value changed.
bool Ui_SliderFloat(const char* label, float* value, float minimum, float maximum, float step);

/// A value edited one step at a time, with an exact step rather than a grain.
/// @param label The name, which is also the widget identity.
/// @param value Read for the current value, written when it changes.
/// @param minimum Lowest value.
/// @param maximum Highest value.
/// @param step How far one press moves it.
/// @return True on the frame the value changed.
bool Ui_Stepper(const char* label, int* value, int minimum, int maximum, int step);

/// One of a set of mutually exclusive choices.
/// @param label The name, which is also the widget identity.
/// @param value Read for the current choice, written when this one is picked.
/// @param option The value this row selects.
/// @return True on the frame it is picked.
bool Ui_Radio(const char* label, int* value, int option);

/// A row whose right-hand side is a colour swatch.
/// @param label The name.
/// @param color The colour to show.
void Ui_ColorSwatch(const char* label, UiRgba color);

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

/// Text wrapped to the width available, broken at spaces.
/// @param text The prose to draw.
/// @param role Which colour role to draw it in.
void Ui_LabelWrapped(const char* text, UiColor role);

/// @param text The line to draw.
/// @param align How to sit it within the available width.
/// @param role Which colour role to draw it in.
void Ui_LabelAligned(const char* text, UiAlign align, UiColor role);

/// A line cut short with a trailing marker when it does not fit.
/// @param text The line to draw.
/// @param role Which colour role to draw it in.
void Ui_LabelEllipsized(const char* text, UiColor role);

/// @param format A printf-style format string.
void Ui_LabelFormat(const char* format, ...) __attribute__((format(printf, 1, 2)));

/// A label on the left and a formatted value right-aligned against the edge.
/// @param label The name.
/// @param format A printf-style format string for the value.
void Ui_LabelValueFormat(const char* label, const char* format, ...) __attribute__((format(printf, 2, 3)));

// ---------------------------------------------------------------------------
// Layers
// ---------------------------------------------------------------------------

/// Open the overlay layer. Everything drawn until it closes is appended after
/// the interface, so it draws above all of it whatever order it was built in.
void Ui_BeginOverlay();

void Ui_EndOverlay();

/// Open a modal panel centred on the screen, over a dimmed backdrop.
/// @param title Drawn as the panel header.
/// @param w Width in pixels.
/// @param h Height in pixels.
/// @return True while the modal is open, so its contents should be drawn.
bool Ui_BeginModal(const char* title, int w, int h);

void Ui_EndModal();

/// Queue a notification. Shown one at a time in the corner, oldest first.
/// @param text The line to show.
/// @param seconds How long to show it.
void Ui_Toast(const char* text, float seconds);

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
/// @param text The string; characters outside the font draw a substitute.
/// @param role Which colour role to draw in.
/// @return The x position just past the string.
int Ui_Text(int x, int y, int scale, const char* text, UiColor role);

/// @param x Left edge of the region in screen pixels.
/// @param y Top edge in screen pixels.
/// @param w Region width, which the text is aligned within.
/// @param scale Whole-pixel size of one font dot.
/// @param text The string to draw.
/// @param align How to sit it within the region.
/// @param role Which colour role to draw in.
/// @return The x position just past the string.
int Ui_TextAligned(int x, int y, int w, int scale, const char* text, UiAlign align, UiColor role);

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

/// @param theme Which built-in theme to build.
/// @return Its colours and metrics. Defined by the generated theme table.
UiStyle Ui_BuiltinTheme(UiBuiltinTheme theme);

/// @param theme Which built-in theme to name.
/// @return Its declared name.
const char* Ui_BuiltinThemeName(UiBuiltinTheme theme);

/// @param role Which text role to read.
/// @return The font key the declaration assigns to that role.
const char* Ui_BuiltinRoleFont(UiFontRole role);

/// @return The built-in default theme, so a caller can restore it after editing.
UiStyle Ui_DefaultStyle();

/// Replace the live theme with one of the themes built into the binary. Needs
/// no filesystem, which is what makes it available before one exists.
/// @param theme Which built-in theme to use.
void Ui_SetBuiltinTheme(UiBuiltinTheme theme);

/// Replace the live theme with a cooked one.
///
/// The file is validated whole — identity, layout version, size, integrity and
/// the sanity of its values — before anything is written, and committed in one
/// step, so a theme that fails any check leaves the live theme exactly as it
/// was. A refusal is reported naming the check that failed.
/// @param key Asset key of the cooked theme.
/// @return False when the theme is absent, fails a check, or the resource
///         subsystem is not loaded.
bool Ui_LoadTheme(const char* key);

/// @return The name of the theme in use.
const char* Ui_ThemeName();

/// Decode and validate a cooked theme payload into a style block.
///
/// The resource subsystem's entry point for a theme asset. A theme is copied
/// into live state rather than parsed, so this is where identity, layout
/// version, size and integrity are checked; nothing downstream would catch a
/// bad one.
/// @param data The payload, immediately after the asset header.
/// @param size Payload length in bytes.
/// @param outStyle Filled on success, untouched on failure.
/// @return False when any check fails; the reason is reported.
bool Ui_ThemeDecode(const void* data, size_t size, UiStyle* outStyle);

/// @param role Which text role to read.
/// @return The asset key of the font serving that role, or an empty string when
///         the role falls through to the built-in font.
const char* Ui_RoleFont(UiFontRole role);

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

/// Whole-pixel dimensions of one cell of the **built-in** font at scale 1.
///
/// These describe the fallback only. Nothing should lay out against them: a
/// cooked font is proportional and supplies its own metrics, so use
/// Ui_TextWidth and Ui_TextHeight instead.
enum : int
{
    UI_FALLBACK_GLYPH_W = 5,
    UI_FALLBACK_GLYPH_H = 7,
    UI_FALLBACK_GLYPH_ADVANCE = 6
};

/// @return Whether a cooked font is in use; false means the built-in one is.
bool Ui_FontIsCooked();

/// @param scale Whole-pixel size of one font dot.
/// @param text The string to measure.
/// @param maxWidth The width to fit within, in screen pixels.
/// @return How many bytes of `text` fit within maxWidth.
int Ui_TextFit(int scale, const char* text, int maxWidth);

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

/// @return How many focusable widgets were registered this frame.
uint32_t Ui_FocusablesUsed();

/// @return How many focusable widgets fit on this platform.
uint32_t Ui_FocusableBudget();

/// @return How many widgets currently remember something between frames.
uint32_t Ui_StatesUsed();

/// @return How many may on this platform.
uint32_t Ui_StateBudget();

/// @return The overlay quads used so far this frame.
uint32_t Ui_OverlayQuadsUsed();

/// @return The per-frame overlay quad budget on this platform.
uint32_t Ui_OverlayQuadBudget();

/// @return The deepest the clip stack has been this frame.
uint32_t Ui_ClipDepthUsed();

/// @return How deeply containers may nest on this platform.
uint32_t Ui_ClipDepthBudget();

/// @return The quads used so far this frame.
uint32_t Ui_QuadsUsed();

/// @return The per-frame quad budget on this platform.
uint32_t Ui_QuadBudget();
