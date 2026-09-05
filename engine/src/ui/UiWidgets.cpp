#include "EngineUi.h"

#include <cstdio>

#include "EngineDebug.h"
#include "EngineInput.h"
#include "UiInternal.h"

void UI::Reset()
{
    m_count = 0;
    m_dropped = 0;
}

bool UI::Add(const UiQuad& quad)
{
    if (m_count >= UI_MAX_QUADS)
    {
        ++m_dropped;
        return false;
    }
    m_quads[m_count++] = quad;
    return true;
}

namespace
{
    int RowHeight()
    {
        const UiStyle& style = Ui_GetStyle();
        return Ui_TextHeight(style.textScale) + style.rowPadding * 2;
    }

    UiRgba RowBackground(bool focused, bool hovered, bool held)
    {
        if (held)
            return Ui_GetColor(UiColor::ItemActive);
        if (hovered)
            return Ui_GetColor(UiColor::ItemHovered);
        if (focused)
            return Ui_GetColor(UiColor::Focus);
        return Ui_GetColor(UiColor::ItemBackground);
    }

    /// Draw the shared body of an activatable row and report what happened to it.
    /// @param label The row text, which also carries its identity.
    /// @param highlight Whether to draw it as the current choice.
    /// @param outX Receives the row's left edge.
    /// @param outY Receives the row's top edge.
    /// @param outW Receives the row's width.
    /// @return True on the frame the row is activated.
    bool ActivatableRow(const char* label, bool highlight, int* outX, int* outY, int* outW)
    {
        const UiStyle& style = Ui_GetStyle();
        const int height = RowHeight();
        int x = 0;
        int y = 0;
        int w = 0;
        if (!UiInternal_CanDraw() || !UiInternal_TakeRow(height, &x, &y, &w))
            return false;

        UiFrameState& state = UiInternal_State();
        const uint32_t id = UiInternal_Id(label);
        const bool focused = UiInternal_RegisterFocusable(id);
        const bool hovered = UiInternal_PointerOver(x, y, w, height);

        if (hovered)
        {
            state.hotId = id;
            if (state.pointerMoved)
            {
                state.focusId = id;
                state.focusIndex = static_cast<int>(state.focusableCount) - 1;
                state.navDelta = 0;
            }
        }

        const bool held = hovered && state.pointer.down;
        UiInternal_PushRect(x, y, w, height, RowBackground(focused || highlight, hovered, held));
        if (focused)
            UiInternal_PushBorder(x, y, w, height, style.borderWidth, Ui_GetColor(UiColor::Border));

        *outX = x;
        *outY = y;
        *outW = w;

        const bool byFocus = focused && state.accept;
        const bool byPointer = hovered && state.pointer.pressed;
        return byFocus || byPointer;
    }
} // namespace

void Ui_BeginPanel(const char* title, int x, int y, int w, int h)
{
    if (!UiInternal_CanDraw())
        return;

    UiFrameState& state = UiInternal_State();
    if (state.inPanel)
    {
        Engine_LogError("Ui: BeginPanel called inside an open panel");
        return;
    }

    const UiStyle& style = Ui_GetStyle();
    state.inPanel = true;
    state.panelX = x;
    state.panelY = y;
    state.panelW = w;
    state.panelH = h;
    state.contentX = x + style.panelPadding;
    state.contentW = w - style.panelPadding * 2;
    state.cursorY = y + style.panelPadding;

    UiInternal_PushRect(x, y, w, h, Ui_GetColor(UiColor::PanelBackground));
    UiInternal_PushBorder(x, y, w, h, style.borderWidth, Ui_GetColor(UiColor::Border));

    if (title && title[0])
    {
        const int height = Ui_TextHeight(style.textScale);
        UiFont_Draw(state.contentX, state.cursorY, style.textScale, title, Ui_GetColor(UiColor::Header));
        state.cursorY += height + style.itemSpacing;
        UiInternal_PushRect(state.contentX, state.cursorY, state.contentW, style.borderWidth, Ui_GetColor(UiColor::Border));
        state.cursorY += style.borderWidth + style.itemSpacing;
    }
}

void Ui_EndPanel()
{
    if (!UiInternal_CanDraw())
        return;
    UiInternal_State().inPanel = false;
}

void Ui_Spacing(int pixels)
{
    if (!UiInternal_CanDraw())
        return;
    UiInternal_State().cursorY += pixels;
}

void Ui_Separator()
{
    const UiStyle& style = Ui_GetStyle();
    int x = 0;
    int y = 0;
    int w = 0;
    if (!UiInternal_CanDraw() || !UiInternal_TakeRow(style.borderWidth, &x, &y, &w))
        return;
    UiInternal_PushRect(x, y, w, style.borderWidth, Ui_GetColor(UiColor::Border));
}

void Ui_Header(const char* text)
{
    const UiStyle& style = Ui_GetStyle();
    const int height = Ui_TextHeight(style.textScale);
    int x = 0;
    int y = 0;
    int w = 0;
    if (!UiInternal_CanDraw() || !UiInternal_TakeRow(height + style.borderWidth + style.rowPadding, &x, &y, &w))
        return;

    UiFont_Draw(x, y, style.textScale, text, Ui_GetColor(UiColor::Header));
    UiInternal_PushRect(x, y + height + style.rowPadding - style.borderWidth, w, style.borderWidth, Ui_GetColor(UiColor::Border));
}

int Ui_ContentWidth() { return UiInternal_CanDraw() ? UiInternal_State().contentW : 0; }

int Ui_CursorY() { return UiInternal_CanDraw() ? UiInternal_State().cursorY : 0; }

void Ui_LabelColored(const char* text, UiColor role)
{
    const UiStyle& style = Ui_GetStyle();
    int x = 0;
    int y = 0;
    int w = 0;
    if (!UiInternal_CanDraw() || !UiInternal_TakeRow(Ui_TextHeight(style.textScale), &x, &y, &w))
        return;
    UiFont_Draw(x, y, style.textScale, text, Ui_GetColor(role));
}

void Ui_Label(const char* text) { Ui_LabelColored(text, UiColor::Text); }

void Ui_LabelValue(const char* label, const char* value)
{
    const UiStyle& style = Ui_GetStyle();
    int x = 0;
    int y = 0;
    int w = 0;
    if (!UiInternal_CanDraw() || !UiInternal_TakeRow(Ui_TextHeight(style.textScale), &x, &y, &w))
        return;

    UiFont_Draw(x, y, style.textScale, label, Ui_GetColor(UiColor::TextDim));
    const int valueWidth = Ui_TextWidth(style.textScale, value);
    UiFont_Draw(x + w - valueWidth, y, style.textScale, value, Ui_GetColor(UiColor::Text));
}

bool Ui_Button(const char* label)
{
    const UiStyle& style = Ui_GetStyle();
    int x = 0;
    int y = 0;
    int w = 0;
    const bool activated = ActivatableRow(label, false, &x, &y, &w);
    if (w == 0)
        return false;

    const int textWidth = Ui_TextWidth(style.textScale, label);
    UiFont_Draw(x + (w - textWidth) / 2, y + style.rowPadding, style.textScale, label, Ui_GetColor(UiColor::Text));
    return activated;
}

bool Ui_Selectable(const char* label, bool selected)
{
    const UiStyle& style = Ui_GetStyle();
    int x = 0;
    int y = 0;
    int w = 0;
    const bool activated = ActivatableRow(label, selected, &x, &y, &w);
    if (w == 0)
        return false;

    const int caretWidth = Ui_TextWidth(style.textScale, "> ");
    const UiColor role = selected ? UiColor::TextAccent : UiColor::Text;
    if (selected)
        UiFont_Draw(x + style.rowPadding, y + style.rowPadding, style.textScale, ">", Ui_GetColor(UiColor::TextAccent));
    UiFont_Draw(x + style.rowPadding + caretWidth, y + style.rowPadding, style.textScale, label, Ui_GetColor(role));
    return activated;
}

bool Ui_Checkbox(const char* label, bool* value)
{
    const UiStyle& style = Ui_GetStyle();
    int x = 0;
    int y = 0;
    int w = 0;
    const bool activated = ActivatableRow(label, false, &x, &y, &w);
    if (w == 0)
        return false;

    const int box = Ui_TextHeight(style.textScale);
    const int boxX = x + w - box - style.rowPadding;
    const int boxY = y + style.rowPadding;
    UiFont_Draw(x + style.rowPadding, boxY, style.textScale, label, Ui_GetColor(UiColor::Text));
    UiInternal_PushBorder(boxX, boxY, box, box, style.borderWidth, Ui_GetColor(UiColor::Border));

    const bool on = value && *value;
    if (on)
        UiInternal_PushRect(boxX + 2, boxY + 2, box - 4, box - 4, Ui_GetColor(UiColor::TextAccent));

    if (activated && value)
    {
        *value = !*value;
        return true;
    }
    return false;
}

bool Ui_SliderInt(const char* label, int* value, int minimum, int maximum)
{
    const UiStyle& style = Ui_GetStyle();
    int x = 0;
    int y = 0;
    int w = 0;
    const bool activated = ActivatableRow(label, false, &x, &y, &w);
    if (w == 0 || !value)
        return false;

    UiFrameState& state = UiInternal_State();
    const uint32_t id = UiInternal_Id(label);
    const bool focused = state.focusId == id;

    char readout[64];
    snprintf(readout, sizeof(readout), "%d", *value);
    UiFont_Draw(x + style.rowPadding, y + style.rowPadding, style.textScale, label, Ui_GetColor(UiColor::Text));
    const int readoutWidth = Ui_TextWidth(style.textScale, readout);
    UiFont_Draw(x + w - readoutWidth - style.rowPadding, y + style.rowPadding, style.textScale, readout, Ui_GetColor(UiColor::TextAccent));

    if (!focused)
        return false;

    int step = 0;
    if (WasGamePadButtonPressed(0, GamepadButton::DPadRight))
        step = 1;
    if (WasGamePadButtonPressed(0, GamepadButton::DPadLeft))
        step = -1;

    const int range = maximum - minimum;
    const int grain = (range > 32) ? (range / 32) : 1;
    step *= grain;

    if (step == 0)
        return activated;

    int next = *value + step;
    if (next < minimum)
        next = minimum;
    if (next > maximum)
        next = maximum;
    if (next == *value)
        return false;

    *value = next;
    return true;
}

void Ui_Bar(const char* label, int value, int maximum)
{
    const UiStyle& style = Ui_GetStyle();
    const int textHeight = Ui_TextHeight(style.textScale);
    const int height = textHeight + style.rowPadding + style.barHeight;
    int x = 0;
    int y = 0;
    int w = 0;
    if (!UiInternal_CanDraw() || !UiInternal_TakeRow(height, &x, &y, &w))
        return;

    char readout[64];
    snprintf(readout, sizeof(readout), "%d / %d", value, maximum);
    UiFont_Draw(x, y, style.textScale, label, Ui_GetColor(UiColor::TextDim));
    const int readoutWidth = Ui_TextWidth(style.textScale, readout);
    UiFont_Draw(x + w - readoutWidth, y, style.textScale, readout, Ui_GetColor(UiColor::Text));

    const int trackY = y + textHeight + style.rowPadding;
    UiInternal_PushRect(x, trackY, w, style.barHeight, Ui_GetColor(UiColor::BarTrack));

    if (maximum <= 0 || value <= 0)
        return;

    int filled = static_cast<int>((static_cast<long long>(value) * w) / maximum);
    if (filled > w)
        filled = w;
    const bool warn = (static_cast<long long>(value) * 4) >= (static_cast<long long>(maximum) * 3);
    UiInternal_PushRect(x, trackY, filled, style.barHeight, Ui_GetColor(warn ? UiColor::BarFillWarn : UiColor::BarFill));
}

void Ui_Plot(const char* label, const float* values, int count, float minimum, float maximum, float marker)
{
    const UiStyle& style = Ui_GetStyle();
    const int textHeight = Ui_TextHeight(style.textScale);
    const int plotHeight = textHeight * 4;
    const int height = textHeight + style.rowPadding + plotHeight;
    int x = 0;
    int y = 0;
    int w = 0;
    if (!UiInternal_CanDraw() || !UiInternal_TakeRow(height, &x, &y, &w))
        return;

    UiFont_Draw(x, y, style.textScale, label, Ui_GetColor(UiColor::TextDim));

    const int plotY = y + textHeight + style.rowPadding;
    UiInternal_PushRect(x, plotY, w, plotHeight, Ui_GetColor(UiColor::BarTrack));
    UiInternal_PushBorder(x, plotY, w, plotHeight, style.borderWidth, Ui_GetColor(UiColor::Border));

    const float span = maximum - minimum;
    if (span <= 0.0f || !values || count <= 0)
        return;

    if (marker >= minimum && marker <= maximum)
    {
        const int markerY = plotY + plotHeight - 1 - static_cast<int>(((marker - minimum) / span) * static_cast<float>(plotHeight - 1));
        UiInternal_PushRect(x, markerY, w, 1, Ui_GetColor(UiColor::TextWarn));
    }

    const int columns = (count < w) ? count : w;
    for (int i = 0; i < columns; ++i)
    {
        const int sample = (count * i) / columns;
        float normalised = (values[sample] - minimum) / span;
        if (normalised < 0.0f)
            normalised = 0.0f;
        if (normalised > 1.0f)
            normalised = 1.0f;

        const int barHeight = static_cast<int>(normalised * static_cast<float>(plotHeight - 2));
        const int columnW = (w / columns) > 0 ? (w / columns) : 1;
        UiInternal_PushRect(x + i * columnW, plotY + plotHeight - 1 - barHeight, columnW, 1 + (barHeight > 0 ? 1 : 0), Ui_GetColor(UiColor::BarFill));
    }
}

void Ui_BeginPointBox(const char* label, int height)
{
    const UiStyle& style = Ui_GetStyle();
    const int textHeight = Ui_TextHeight(style.textScale);
    int x = 0;
    int y = 0;
    int w = 0;
    if (!UiInternal_CanDraw() || !UiInternal_TakeRow(textHeight + style.rowPadding + height, &x, &y, &w))
        return;

    UiFrameState& state = UiInternal_State();
    UiFont_Draw(x, y, style.textScale, label, Ui_GetColor(UiColor::TextDim));

    state.inBox = true;
    state.boxX = x;
    state.boxY = y + textHeight + style.rowPadding;
    state.boxW = w;
    state.boxH = height;

    UiInternal_PushRect(state.boxX, state.boxY, w, height, Ui_GetColor(UiColor::BarTrack));
    UiInternal_PushBorder(state.boxX, state.boxY, w, height, style.borderWidth, Ui_GetColor(UiColor::Border));
}

void Ui_Point(float nx, float ny, UiColor role)
{
    if (!UiInternal_CanDraw())
        return;
    UiFrameState& state = UiInternal_State();
    if (!state.inBox)
        return;

    const int size = Ui_GetStyle().cursorSize / 2 + 1;
    const int px = state.boxX + static_cast<int>(nx * static_cast<float>(state.boxW - 1));
    const int py = state.boxY + static_cast<int>(ny * static_cast<float>(state.boxH - 1));
    UiInternal_PushRect(px - size, py - size, size * 2, size * 2, Ui_GetColor(role));
}

void Ui_EndPointBox()
{
    if (!UiInternal_CanDraw())
        return;
    UiInternal_State().inBox = false;
}

void Ui_Rect(int x, int y, int w, int h, UiColor role) { UiInternal_PushRect(x, y, w, h, Ui_GetColor(role)); }

void Ui_RectRgba(int x, int y, int w, int h, UiRgba color) { UiInternal_PushRect(x, y, w, h, color); }

int Ui_Text(int x, int y, int scale, const char* text, UiColor role) { return UiFont_Draw(x, y, scale, text, Ui_GetColor(role)); }
