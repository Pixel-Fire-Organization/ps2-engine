#include "EngineUi.h"

namespace
{
    UiRgba Rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        UiRgba c;
        c.r = r;
        c.g = g;
        c.b = b;
        c.a = a;
        return c;
    }

    UiStyle MakeDefault()
    {
        UiStyle s;

        s.colors[static_cast<uint8_t>(UiColor::WindowBackground)] = Rgba(12, 14, 20, 255);
        s.colors[static_cast<uint8_t>(UiColor::PanelBackground)] = Rgba(22, 26, 36, 255);
        s.colors[static_cast<uint8_t>(UiColor::Border)] = Rgba(56, 68, 92, 255);
        s.colors[static_cast<uint8_t>(UiColor::Header)] = Rgba(120, 200, 255, 255);
        s.colors[static_cast<uint8_t>(UiColor::Text)] = Rgba(212, 220, 232, 255);
        s.colors[static_cast<uint8_t>(UiColor::TextDim)] = Rgba(126, 138, 156, 255);
        s.colors[static_cast<uint8_t>(UiColor::TextAccent)] = Rgba(255, 240, 120, 255);
        s.colors[static_cast<uint8_t>(UiColor::TextWarn)] = Rgba(255, 128, 96, 255);
        s.colors[static_cast<uint8_t>(UiColor::ItemBackground)] = Rgba(32, 38, 52, 255);
        s.colors[static_cast<uint8_t>(UiColor::ItemHovered)] = Rgba(46, 58, 80, 255);
        s.colors[static_cast<uint8_t>(UiColor::ItemActive)] = Rgba(64, 84, 116, 255);
        s.colors[static_cast<uint8_t>(UiColor::Focus)] = Rgba(28, 56, 84, 255);
        s.colors[static_cast<uint8_t>(UiColor::BarTrack)] = Rgba(34, 40, 54, 255);
        s.colors[static_cast<uint8_t>(UiColor::BarFill)] = Rgba(96, 200, 148, 255);
        s.colors[static_cast<uint8_t>(UiColor::BarFillWarn)] = Rgba(232, 168, 72, 255);
        s.colors[static_cast<uint8_t>(UiColor::Cursor)] = Rgba(255, 255, 255, 255);
        s.colors[static_cast<uint8_t>(UiColor::CursorOutline)] = Rgba(16, 18, 24, 255);

        s.panelPadding = 8;
        s.itemSpacing = 4;
        s.borderWidth = 1;
        s.textScale = 2;
        s.rowPadding = 3;
        s.barHeight = 8;
        s.cursorSize = 10;
        return s;
    }

    UiStyle s_Style = MakeDefault();

    const char* const s_ColorNames[static_cast<uint8_t>(UiColor::Count)] = {
        "WINDOW BG", "PANEL BG",   "BORDER",     "HEADER",     "TEXT",     "TEXT DIM",
        "TEXT ACCENT", "TEXT WARN", "ITEM BG",   "ITEM HOVER", "ITEM ACTIVE", "FOCUS",
        "BAR TRACK", "BAR FILL",   "BAR WARN",   "CURSOR",     "CURSOR EDGE",
    };
} // namespace

UiStyle Ui_DefaultStyle() { return MakeDefault(); }

const UiStyle& Ui_GetStyle() { return s_Style; }

void Ui_SetStyle(const UiStyle& style) { s_Style = style; }

UiRgba Ui_GetColor(UiColor role)
{
    const uint8_t index = static_cast<uint8_t>(role);
    if (index >= static_cast<uint8_t>(UiColor::Count))
        return s_Style.colors[static_cast<uint8_t>(UiColor::Text)];
    return s_Style.colors[index];
}

void Ui_SetColor(UiColor role, UiRgba value)
{
    const uint8_t index = static_cast<uint8_t>(role);
    if (index < static_cast<uint8_t>(UiColor::Count))
        s_Style.colors[index] = value;
}

const char* Ui_ColorName(UiColor role)
{
    const uint8_t index = static_cast<uint8_t>(role);
    return (index < static_cast<uint8_t>(UiColor::Count)) ? s_ColorNames[index] : "?";
}
