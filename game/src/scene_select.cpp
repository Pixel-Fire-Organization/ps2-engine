// scene_select — the boot scene: a rect-font menu to pick which test runs.
#include "DebugFont.h"
#include "GameAPI.h"
#include "Scene.h"

namespace
{
    // Both NTSC (640x448) and PAL (640x512) are 640 wide, so centre horizontally
    // on 640 and anchor rows from the top (height differs by region).
    const int SCREEN_W = 640;

    int s_cursor = 0; // index into the selectable scenes (all except the selector)
    bool s_upHeld = false, s_downHeld = false, s_confirmHeld = false;

    // Selectable scenes = every scene except index 0 (this selector itself).
    int SelectableCount() { return SceneManager_Count() - 1; }
    int SceneIndexForCursor(int c) { return c + 1; }
} // namespace

void Scene_Select_Init()
{
    s_cursor = 0;
    s_upHeld = s_downHeld = s_confirmHeld = false;
}

void Scene_Select_Update(float dt)
{
    (void)dt;
    const int n = SelectableCount();
    if (n <= 0)
        return;

    // --- input (rising-edge debounced) ---
    const bool up = game::IsPadPressed(0, "dpad_up");
    const bool down = game::IsPadPressed(0, "dpad_down");
    const bool confirm = game::IsPadPressed(0, "x");

    if (up)
    {
        if (!s_upHeld)
        {
            s_cursor = (s_cursor + n - 1) % n;
            s_upHeld = true;
        }
    }
    else
        s_upHeld = false;

    if (down)
    {
        if (!s_downHeld)
        {
            s_cursor = (s_cursor + 1) % n;
            s_downHeld = true;
        }
    }
    else
        s_downHeld = false;

    if (confirm)
    {
        if (!s_confirmHeld)
        {
            s_confirmHeld = true;
            SceneManager_SetScene(SceneIndexForCursor(s_cursor));
            return; // the chosen scene is now active
        }
    }
    else
        s_confirmHeld = false;

    // --- draw ---
    game::Clear(12, 14, 20);

    const int titleScale = 3;
    const char* title = "PS2 ENGINE TESTBED";
    font::DrawText((SCREEN_W - font::TextWidth(titleScale, title)) / 2, 50, titleScale, title, 120, 200, 255);

    const int rowScale = 3;
    const int rowH = 40;
    const int top = 150;
    for (int i = 0; i < n; ++i)
    {
        const char* name = SceneManager_Name(SceneIndexForCursor(i));
        const int y = top + i * rowH;
        int r = 150, g = 160, b = 170;
        if (i == s_cursor)
        {
            game::DrawRect(40, y - 6, SCREEN_W - 80, font::GLYPH_H * rowScale + 12, 28, 56, 84);
            font::DrawText(52, y, rowScale, ">", 255, 240, 120);
            r = 255;
            g = 240;
            b = 120;
        }
        font::DrawText(96, y, rowScale, name, r, g, b);
    }

    const int footScale = 2;
    const char* footer = "DPAD MOVE   X SELECT";
    font::DrawText((SCREEN_W - font::TextWidth(footScale, footer)) / 2, top + n * rowH + 50, footScale, footer, 110, 130, 150);
}
