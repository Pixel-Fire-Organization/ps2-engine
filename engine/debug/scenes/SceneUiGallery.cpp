#include <cstdio>

#include "EngineUi.h"
#include "TestbedScene.h"
#include "platform/Platform.h"

namespace
{
    const int PANEL_MARGIN = 16;
    const int PAGE_COUNT = 2;
    const int PLOT_SAMPLES = 48;
    const int BOX_HEIGHT = 90;

    bool s_Toggle = true;
    int s_Slider = 32;
    float s_Wave[PLOT_SAMPLES];
    float s_Phase = 0.0f;

    const char* const kGlyphRows[] = {
        "ABCDEFGHIJKLM",
        "NOPQRSTUVWXYZ",
        "0123456789",
        "+-/*=<>()[]",
        ".,:;!?%_#@",
    };

    const int kGlyphRowCount = static_cast<int>(sizeof(kGlyphRows) / sizeof(kGlyphRows[0]));

    void DrawWidgets()
    {
        Ui_Header("WIDGETS");
        Ui_Label("LABEL");
        Ui_LabelColored("DIM LABEL", UiColor::TextDim);
        Ui_LabelColored("WARNING", UiColor::TextWarn);
        Ui_LabelValue("LABEL VALUE", "RIGHT");
        Ui_Separator();
        Ui_Button("BUTTON");
        Ui_Selectable("SELECTABLE", false);
        Ui_Selectable("SELECTED", true);
        Ui_Checkbox("CHECKBOX", &s_Toggle);
        Ui_SliderInt("SLIDER", &s_Slider, 0, 100);
        Ui_Bar("BAR", s_Slider, 100);
        Ui_Plot("PLOT", s_Wave, PLOT_SAMPLES, -1.0f, 1.0f, 0.0f);
        Ui_BeginPointBox("POINT BOX", BOX_HEIGHT);
        Ui_Point(0.5f, 0.5f, UiColor::TextAccent);
        Ui_Point(0.1f, 0.9f, UiColor::BarFill);
        Ui_Point(0.9f, 0.1f, UiColor::TextWarn);
        Ui_EndPointBox();
    }

    void DrawGlyphs(int x, int y)
    {
        int cursorY = y;
        for (int scale = 1; scale <= 3; ++scale)
        {
            for (int row = 0; row < kGlyphRowCount; ++row)
            {
                Ui_Text(x, cursorY, scale, kGlyphRows[row], UiColor::Text);
                cursorY += Ui_TextHeight(scale) + 2;
            }
            cursorY += 6;
        }
    }
} // namespace

void Scene_UiGallery_Init()
{
    s_Phase = 0.0f;
    for (int i = 0; i < PLOT_SAMPLES; ++i)
        s_Wave[i] = 0.0f;
}

void Scene_UiGallery_Update(float dt)
{
    s_Phase += dt;
    for (int i = 0; i < PLOT_SAMPLES - 1; ++i)
        s_Wave[i] = s_Wave[i + 1];
    s_Wave[PLOT_SAMPLES - 1] = (s_Phase - static_cast<float>(static_cast<int>(s_Phase))) * 2.0f - 1.0f;

    const Platform* platform = Engine_GetPlatform();
    if (!platform)
        return;

    const int page = Testbed_Page(PAGE_COUNT);
    const int screenW = static_cast<int>(platform->GetConstant(PlatformConstant::ScreenWidth));
    const int screenH = static_cast<int>(platform->GetConstant(PlatformConstant::ScreenHeight));

    Ui_Rect(0, 0, screenW, screenH, UiColor::WindowBackground);

    char title[64];
    snprintf(title, sizeof(title), "%s   L1 R1 PAGE %d OF %d", (page == 0) ? "WIDGETS" : "GLYPHS", page + 1, PAGE_COUNT);

    if (page == 0)
    {
        Ui_BeginPanel(title, PANEL_MARGIN, PANEL_MARGIN, screenW - PANEL_MARGIN * 2, screenH - PANEL_MARGIN * 2);
        DrawWidgets();

        char text[48];
        snprintf(text, sizeof(text), "%u OF %u", static_cast<unsigned>(Ui_QuadsUsed()), static_cast<unsigned>(Ui_QuadBudget()));
        Ui_LabelValue("QUADS", text);
        Ui_EndPanel();
        return;
    }

    Ui_BeginPanel(title, PANEL_MARGIN, PANEL_MARGIN, screenW - PANEL_MARGIN * 2, screenH - PANEL_MARGIN * 2);
    Ui_Label("SCALES 1 2 3");
    Ui_EndPanel();
    DrawGlyphs(PANEL_MARGIN * 2, PANEL_MARGIN + 40);
}
