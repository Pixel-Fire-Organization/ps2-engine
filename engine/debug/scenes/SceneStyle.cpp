#include <cstdio>

#include "EngineUi.h"
#include "TestbedScene.h"
#include "platform/Platform.h"

namespace
{
    const int PANEL_MARGIN = 16;
    const int COLUMN_GAP = 8;
    const int SWATCH_HEIGHT = 18;

    int s_Role = 0;

    void ChannelSlider(const char* label, uint8_t* channel)
    {
        int value = static_cast<int>(*channel);
        if (Ui_SliderInt(label, &value, 0, 255))
            *channel = static_cast<uint8_t>(value);
    }
} // namespace

void Scene_Style_Init() { s_Role = 0; }

void Scene_Style_Update(float dt)
{
    (void)dt;

    const Platform* platform = Engine_GetPlatform();
    if (!platform)
        return;

    const int screenW = static_cast<int>(platform->GetConstant(PlatformConstant::ScreenWidth));
    const int screenH = static_cast<int>(platform->GetConstant(PlatformConstant::ScreenHeight));

    Ui_Rect(0, 0, screenW, screenH, UiColor::WindowBackground);

    const int width = (screenW - PANEL_MARGIN * 2 - COLUMN_GAP) / 2;
    const int height = screenH - PANEL_MARGIN * 2;

    Ui_BeginPanel("ROLES", PANEL_MARGIN, PANEL_MARGIN, width, height);

    const int roles = static_cast<int>(UiColor::Count);
    if (Ui_BeginScroll("roles", Ui_ContentHeight()))
    {
        for (int i = 0; i < roles; ++i)
        {
            const UiColor role = static_cast<UiColor>(i);
            if (Ui_Selectable(Ui_ColorName(role), i == s_Role))
                s_Role = i;
        }
        Ui_EndScroll();
    }
    Ui_EndPanel();

    Ui_BeginPanel("EDIT", PANEL_MARGIN + width + COLUMN_GAP, PANEL_MARGIN, width, height);
    if (s_Role < 0 || s_Role >= roles)
        s_Role = 0;

    const UiColor role = static_cast<UiColor>(s_Role);
    Ui_LabelValue("ROLE", Ui_ColorName(role));

    UiStyle style = Ui_GetStyle();
    UiRgba* colour = &style.colors[s_Role];
    ChannelSlider("RED", &colour->r);
    ChannelSlider("GREEN", &colour->g);
    ChannelSlider("BLUE", &colour->b);
    Ui_SetStyle(style);

    Ui_RectRgba(PANEL_MARGIN + width + COLUMN_GAP * 2, Ui_CursorY(), width - COLUMN_GAP * 3, SWATCH_HEIGHT * 2, style.colors[s_Role]);
    Ui_Spacing(SWATCH_HEIGHT * 2 + 6);

    if (Ui_Button("RESET THEME"))
        Ui_SetStyle(Ui_DefaultStyle());

    Ui_Separator();
    Ui_Label("DPAD LEFT RIGHT EDITS");
    Ui_EndPanel();
}
