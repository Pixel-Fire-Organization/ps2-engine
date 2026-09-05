#include "EngineTestbed.h"

#include <cstdio>

#include "EngineApp.h"
#include "EngineCore.h"
#include "EngineDebug.h"
#include "EngineInput.h"
#include "EngineSubsystems.h"
#include "EngineUi.h"
#include "GameAPI.h"
#include "TestbedScene.h"
#include "platform/Platform.h"

namespace
{
    enum class Mode : uint8_t
    {
        Closed = 0,
        Menu,
        Scene
    };

    const int MENU_MARGIN = 16;
    const int MENU_TITLE_HEIGHT = 34;
    const int MENU_FOOTER_HEIGHT = 26;
    const int MENU_COLUMN_GAP = 8;
    const int EXIT_CHOSEN = -2;

    bool s_Enabled = false;
    Mode s_Mode = Mode::Closed;
    int s_ActiveScene = -1;
    int s_Page = 0;
    char s_ChordText[64] = "unavailable";

    void LeaveActiveScene()
    {
        if (s_ActiveScene < 0)
            return;

        int count = 0;
        const TestbedScene* scenes = Testbed_Catalogue(&count);
        if (s_ActiveScene < count && scenes[s_ActiveScene].shutdown)
            scenes[s_ActiveScene].shutdown();
        s_ActiveScene = -1;
    }

    void EnterScene(int index)
    {
        int count = 0;
        const TestbedScene* scenes = Testbed_Catalogue(&count);
        if (index < 0 || index >= count)
            return;

        Engine_ResetRuntimeState();
        s_ActiveScene = index;
        s_Page = 0;
        s_Mode = Mode::Scene;
        Engine_LogInfo("[Testbed] entering '%s'", scenes[index].name);
        if (scenes[index].init)
            scenes[index].init();
    }

    void OpenMenu()
    {
        LeaveActiveScene();
        Engine_ResetRuntimeState();
        s_Mode = Mode::Menu;
    }

    void CloseToGame()
    {
        LeaveActiveScene();
        s_Mode = Mode::Closed;
        Engine_ResetRuntimeState();
        Engine_LogInfo("[Testbed] closed; restarting the game");
        GameInit();
    }

    /// Draw one column of the catalogue.
    /// @param x Left edge in screen pixels.
    /// @param y Top edge in screen pixels.
    /// @param w Column width in pixels.
    /// @param h Column height in pixels.
    /// @param first First category to draw in this column.
    /// @param last One past the last category to draw in this column.
    /// @return The scene index the player chose, or -1.
    int DrawColumn(int x, int y, int w, int h, int first, int last)
    {
        int chosen = -1;
        int count = 0;
        const TestbedScene* scenes = Testbed_Catalogue(&count);

        Ui_BeginPanel("", x, y, w, h);
        for (int category = first; category < last; ++category)
        {
            const TestbedCategory group = static_cast<TestbedCategory>(category);
            bool headerDrawn = false;
            for (int i = 0; i < count; ++i)
            {
                if (scenes[i].category != group)
                    continue;
                if (!headerDrawn)
                {
                    Ui_Header(Testbed_CategoryName(group));
                    headerDrawn = true;
                }
                if (Ui_Selectable(scenes[i].name, false))
                    chosen = i;
            }
        }
        if (first == 0)
        {
            Ui_Spacing(8);
            if (Ui_Button("EXIT GAME"))
                chosen = EXIT_CHOSEN;
        }
        Ui_EndPanel();
        return chosen;
    }

    void DrawMenu()
    {
        const Platform* platform = Engine_GetPlatform();
        const int screenW = platform ? static_cast<int>(platform->GetConstant(PlatformConstant::ScreenWidth)) : GFX_SCREEN_WIDTH;
        const int screenH = platform ? static_cast<int>(platform->GetConstant(PlatformConstant::ScreenHeight)) : GFX_SCREEN_HEIGHT;
        const UiStyle& style = Ui_GetStyle();

        Ui_Rect(0, 0, screenW, screenH, UiColor::WindowBackground);

        const char* title = "ENGINE TESTBED";
        Ui_Text((screenW - Ui_TextWidth(3, title)) / 2, MENU_MARGIN, 3, title, UiColor::Header);

        const int top = MENU_MARGIN + MENU_TITLE_HEIGHT;
        const int height = screenH - top - MENU_FOOTER_HEIGHT - MENU_MARGIN;
        const int width = (screenW - MENU_MARGIN * 2 - MENU_COLUMN_GAP) / 2;
        const int half = static_cast<int>(TestbedCategory::Count) / 2;

        int chosen = DrawColumn(MENU_MARGIN, top, width, height, 0, half);
        const int right = DrawColumn(MENU_MARGIN + width + MENU_COLUMN_GAP, top, width, height, half, static_cast<int>(TestbedCategory::Count));
        if (right >= 0)
            chosen = right;

        char footer[128];
        snprintf(footer, sizeof(footer), "X SELECT   O CLOSE   %s TOGGLES", s_ChordText);
        Ui_Text((screenW - Ui_TextWidth(style.textScale, footer)) / 2, screenH - MENU_FOOTER_HEIGHT, style.textScale, footer, UiColor::TextDim);

        if (chosen == EXIT_CHOSEN)
        {
            Engine_LogInfo("[Testbed] exit requested");
            EngineApp_OnExitRequested();
            return;
        }
        if (chosen >= 0)
        {
            EnterScene(chosen);
            return;
        }
        if (Ui_WasBackPressed())
            CloseToGame();
    }

    void RunScene(float dt)
    {
        int count = 0;
        const TestbedScene* scenes = Testbed_Catalogue(&count);
        if (s_ActiveScene < 0 || s_ActiveScene >= count)
        {
            OpenMenu();
            return;
        }

        if (scenes[s_ActiveScene].update)
            scenes[s_ActiveScene].update(dt);

        if (Ui_WasBackPressed())
            OpenMenu();
    }
} // namespace

int Testbed_Page(int pageCount)
{
    if (pageCount <= 1)
        return 0;

    if (WasGamePadButtonPressed(0, GamepadButton::R1))
        s_Page = (s_Page + 1) % pageCount;
    if (WasGamePadButtonPressed(0, GamepadButton::L1))
        s_Page = (s_Page + pageCount - 1) % pageCount;
    if (s_Page >= pageCount)
        s_Page = 0;
    return s_Page;
}

void Engine_Testbed_RequestSubsystems()
{
    Engine_Subsystem_Enable(EngineSubsystem::Input);
    Engine_Subsystem_Enable(EngineSubsystem::Ui);
    Engine_Subsystem_Enable(EngineSubsystem::Testbed);
}

bool Engine_Testbed_Init()
{
    s_Mode = Mode::Closed;
    s_ActiveScene = -1;
    Engine_Debug_DescribeChord(DebugChord::DebugMenu, s_ChordText, sizeof(s_ChordText));

    const Platform* platform = Engine_GetPlatform();
    if (!platform || platform->GetDebugChord(DebugChord::DebugMenu) == 0u)
    {
        Engine_LogError("[Testbed] this platform offers no debug-menu chord; the testbed is unreachable.");
        s_Enabled = false;
        return false;
    }

    s_Enabled = true;
    Engine_LogInfo("[Testbed] ready. Hold %s to open.", s_ChordText);
    return true;
}

void Engine_Testbed_Shutdown()
{
    LeaveActiveScene();
    s_Mode = Mode::Closed;
    s_Enabled = false;
}

bool Engine_Testbed_IsOpen() { return s_Enabled && s_Mode != Mode::Closed; }

void Engine_Testbed_Update(float dt)
{
    if (!s_Enabled)
        return;

    if (Engine_Debug_WasChordPressed(DebugChord::DebugMenu))
    {
        if (s_Mode == Mode::Closed)
            OpenMenu();
        else
            CloseToGame();
        return;
    }

    if (s_Mode == Mode::Menu)
        DrawMenu();
    else if (s_Mode == Mode::Scene)
        RunScene(dt);
}
