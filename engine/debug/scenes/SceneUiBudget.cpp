#include "EngineUi.h"
#include "TestbedScene.h"

namespace
{
    const int PLOT_SAMPLES = 64;
    // Deliberately tight: thirty rows always overruns a cap of twenty quads
    // regardless of which font is active, so this scene exercises the drop
    // path rather than merely declaring it exists.
    const int CAPPED_LIST_QUAD_CAP = 20;
    const int CAPPED_LIST_ROW_COUNT = 30;

    float s_Quads[PLOT_SAMPLES];
    int s_Filled = 0;
    bool s_ShowModal = false;
    int s_Stepper = 8;
    float s_Slider = 0.5f;
    int s_Radio = 0;

    void Push(float value)
    {
        if (s_Filled < PLOT_SAMPLES)
        {
            s_Quads[s_Filled++] = value;
            return;
        }
        for (int i = 1; i < PLOT_SAMPLES; ++i)
            s_Quads[i - 1] = s_Quads[i];
        s_Quads[PLOT_SAMPLES - 1] = value;
    }

    void DrawBudgets()
    {
        Ui_Bar("QUADS", static_cast<int>(Ui_QuadsUsed()), static_cast<int>(Ui_QuadBudget()));
        Ui_Bar("OVERLAY", static_cast<int>(Ui_OverlayQuadsUsed()), static_cast<int>(Ui_OverlayQuadBudget()));
        Ui_Bar("FOCUSABLES", static_cast<int>(Ui_FocusablesUsed()), static_cast<int>(Ui_FocusableBudget()));
        Ui_Bar("STATES", static_cast<int>(Ui_StatesUsed()), static_cast<int>(Ui_StateBudget()));
        Ui_Bar("CLIP DEPTH", static_cast<int>(Ui_ClipDepthUsed()), static_cast<int>(Ui_ClipDepthBudget()));
        Ui_Bar("RUNS", static_cast<int>(Ui_RunsUsed()), static_cast<int>(Ui_RunBudget()));

        Ui_Separator();
        Ui_LabelValue("FONT", Ui_FontIsCooked() ? "COOKED" : "BUILT-IN");
        Ui_LabelValue("THEME", Ui_ThemeName());
        Ui_LabelValueFormat("TEXT HEIGHT", "%d PX", Ui_TextHeight(Ui_GetStyle().textScale));
        Ui_LabelValueFormat("SAMPLE COST", "%d QUADS", Ui_MeasureTextQuads(Ui_GetStyle().textScale, "ENGINE TESTBED"));

        Ui_Separator();
        Ui_Plot("QUADS PER FRAME", s_Quads, s_Filled, 0.0f, static_cast<float>(Ui_QuadBudget()), 0.0f);
    }

    void DrawThemes()
    {
        Ui_Label("BUILT IN, NO FILESYSTEM NEEDED");
        for (uint8_t i = 0; i < static_cast<uint8_t>(UiBuiltinTheme::Count); ++i)
        {
            const UiBuiltinTheme theme = static_cast<UiBuiltinTheme>(i);
            if (Ui_Selectable(Ui_BuiltinThemeName(theme), false))
                Ui_SetBuiltinTheme(theme);
        }

        Ui_Separator();
        Ui_Label("COOKED, LOADED ON COMMAND");
        if (Ui_Button("LOAD THEME_SLATE"))
            Ui_LoadTheme("RASSETS\\THEME_SLATE.PS2A");
        if (Ui_Button("LOAD THEME_CONTRAST"))
            Ui_LoadTheme("RASSETS\\THEME_CONTRAST.PS2A");
        if (Ui_Button("LOAD A THEME THAT IS NOT THERE"))
            Ui_LoadTheme("RASSETS\\THEME_NOPE.PS2A");
    }

    void DrawWidgets()
    {
        if (Ui_BeginTree("A TREE", true))
        {
            Ui_Label("NESTED ROW ONE");
            Ui_Label("NESTED ROW TWO");
            Ui_EndTree();
        }

        Ui_Stepper("STEPPER", &s_Stepper, 0, 32, 1);
        Ui_SliderFloat("SLIDER", &s_Slider, 0.0f, 1.0f, 0.05f);
        Ui_Radio("FIRST", &s_Radio, 0);
        Ui_Radio("SECOND", &s_Radio, 1);
        Ui_ColorSwatch("ACCENT", Ui_GetColor(UiColor::TextAccent));

        Ui_Separator();
        if (Ui_Button("TOAST"))
            Ui_Toast("A NOTIFICATION", 2.5f);
        if (Ui_Button("MODAL"))
            s_ShowModal = true;

        Ui_Separator();
        Ui_Header("A CONTAINER CAPPED WITH Ui_BeginBudget");
        Ui_BeginBudget(CAPPED_LIST_QUAD_CAP);
        for (int i = 0; i < CAPPED_LIST_ROW_COUNT; ++i)
            Ui_LabelValueFormat("ROW", "%d", i);
        Ui_EndBudget();
        Ui_LabelValueFormat("CONTAINER QUADS", "%u OF %d", static_cast<unsigned>(Ui_ContainerQuadsUsed()), CAPPED_LIST_QUAD_CAP);
        Ui_LabelValue("A FEW MORE QUADS WOULD FIT", Ui_WouldFit(4) ? "YES" : "NO");

        Ui_Separator();
        Ui_LabelWrapped("This scene is the observable proof of the interface budget: every bar above is a real ceiling, and a screen that overruns one is reported once per frame.", UiColor::TextDim);
    }

    void DrawScrolling()
    {
        Ui_Label("SCROLLS RATHER THAN PAGES");
        if (Ui_BeginScroll("rows", Ui_ContentHeight()))
        {
            for (int i = 0; i < 60; ++i)
                Ui_LabelValueFormat("ROW", "%d", i);
            Ui_EndScroll();
        }
    }
} // namespace

void Scene_UiBudget_Init()
{
    s_Filled = 0;
    s_ShowModal = false;
}

void Scene_UiBudget_Update(float dt)
{
    (void)dt;

    Ui_Rect(0, 0, Ui_ScreenWidth(), Ui_ScreenHeight(), UiColor::WindowBackground);
    Push(static_cast<float>(Ui_QuadsUsed()));

    static const char* const PAGES[] = {"BUDGETS", "WIDGETS", "SCROLL", "THEMES"};

    Ui_BeginPanelSlot("UI BUDGET", UiPanelSlot::Full);
    const int page = Testbed_Tabs("pages", PAGES, 4);
    switch (page)
    {
    case 0:
        DrawBudgets();
        break;
    case 1:
        DrawWidgets();
        break;
    case 2:
        DrawScrolling();
        break;
    default:
        DrawThemes();
        break;
    }
    Ui_EndPanel();

    if (s_ShowModal && Ui_BeginModal("MODAL", 320, 140))
    {
        Ui_LabelWrapped("A modal dims what is behind it, which is the first thing per-quad transparency bought.", UiColor::Text);
        Ui_Spacing(8);
        if (Ui_Button("CLOSE") || Ui_WasBackPressed())
            s_ShowModal = false;
        Ui_EndModal();
    }
}

void Scene_UiBudget_Shutdown() {}
