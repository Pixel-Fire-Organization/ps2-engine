#include <cstdio>

#include "EngineAchievement.h"
#include "EngineSubsystems.h"
#include "EngineUi.h"
#include "TestbedScene.h"
#include "platform/Platform.h"

namespace
{
    const int PANEL_MARGIN = 16;
    const int COLUMN_GAP = 8;
    const uint32_t IDS_SHOWN = 8;
} // namespace

void Scene_Achievements_Init() {}

void Scene_Achievements_Update(float dt)
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

    Ui_BeginPanel("ACHIEVEMENTS", PANEL_MARGIN, PANEL_MARGIN, width, height);

    if (!Engine_Subsystem_IsEnabled(EngineSubsystem::Achievement))
    {
        Testbed_DrawUnavailable("ACHIEVEMENT SUBSYSTEM");
        Ui_Label("THE GAME DID NOT ASK");
        Ui_Label("FOR IT. THAT IS NOT");
        Ui_Label("THE SAME AS THE");
        Ui_Label("PLATFORM LACKING IT.");
        Ui_EndPanel();
        return;
    }

    const bool available = Engine_Achievement_IsAvailable();
    const uint32_t count = Engine_Achievement_GetCount();

    Ui_LabelValue("AVAILABLE", available ? "YES" : "NO");

    char text[48];
    snprintf(text, sizeof(text), "%u", static_cast<unsigned>(count));
    Ui_LabelValue("DECLARED", text);
    Ui_Separator();

    const uint32_t shown = (count < IDS_SHOWN) ? count : IDS_SHOWN;
    for (uint32_t id = 0; id < shown; ++id)
    {
        char row[48];
        snprintf(row, sizeof(row), "UNLOCK %u %s", static_cast<unsigned>(id), Engine_Achievement_IsUnlocked(id) ? "DONE" : "-");
        if (Ui_Selectable(row, Engine_Achievement_IsUnlocked(id)))
            Engine_Achievement_Unlock(id);
    }
    if (shown == 0)
        Ui_LabelColored("NONE DECLARED HERE", UiColor::TextDim);
    Ui_EndPanel();

    Ui_BeginPanel("NOTES", PANEL_MARGIN + width + COLUMN_GAP, PANEL_MARGIN, width, height);
    Ui_Label("UNLOCKING IS ONE WAY");
    Ui_Label("AND IDEMPOTENT.");
    Ui_Separator();
    Ui_Label("ON MOST PLATFORMS THIS");
    Ui_Label("REPORTS UNAVAILABLE AND");
    Ui_Label("EVERY CALL RETURNS NO.");
    Ui_Label("THAT PATH IS THE COMMON");
    Ui_Label("ONE AND IS EXERCISED ON");
    Ui_Label("EVERY RUN.");
    Ui_Separator();
    Ui_Label("THERE IS NO PROGRESS,");
    Ui_Label("NO TIERS AND NO WAY TO");
    Ui_Label("ENUMERATE METADATA.");
    Ui_EndPanel();
}
