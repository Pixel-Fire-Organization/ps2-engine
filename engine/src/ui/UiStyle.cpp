#include "EngineUi.h"

namespace
{
    UiStyle s_Style = Ui_BuiltinTheme(UiBuiltinTheme::MIDNIGHT);

    const char* const s_ColorNames[static_cast<uint8_t>(UiColor::Count)] = {
        "WINDOW BG", "PANEL BG",   "BORDER",     "HEADER",     "TEXT",     "TEXT DIM",
        "TEXT ACCENT", "TEXT WARN", "TEXT DISABLED", "ITEM BG",   "ITEM HOVER", "ITEM ACTIVE", "FOCUS",
        "BAR TRACK", "BAR FILL",   "BAR WARN",   "CURSOR",     "CURSOR EDGE",
    };
} // namespace

UiStyle Ui_DefaultStyle() { return Ui_BuiltinTheme(UiBuiltinTheme::MIDNIGHT); }

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
