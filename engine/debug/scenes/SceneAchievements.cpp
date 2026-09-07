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

    int s_LastId = -1;
    bool s_LastOk = false;
} // namespace

void Scene_Achievements_Init()
{
    s_LastId = -1;
    s_LastOk = false;
}

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

    const bool available = Engine_Achievement_IsAvailable();
    const uint32_t count = Engine_Achievement_GetCount();
    const char* reason = Engine_Achievement_GetUnavailableReason();

    Ui_LabelValue("RECORDING", available ? "YES" : "NO");

    char text[48];
    snprintf(text, sizeof(text), "%u", static_cast<unsigned>(count));
    Ui_LabelValue("DECLARED", text);

    if (!available)
    {
        Ui_Separator();
        Ui_LabelColored("CANNOT RECORD BECAUSE", UiColor::TextWarn);
        Testbed_DrawWrapped(reason ? reason : "REASON NOT REPORTED", UiColor::TextDim);
    }

    Ui_Separator();

    const uint32_t shown = (count < IDS_SHOWN) ? count : IDS_SHOWN;
    for (uint32_t id = 0; id < shown; ++id)
    {
        const bool unlocked = Engine_Achievement_IsUnlocked(id);

        char label[32];
        snprintf(label, sizeof(label), "TROPHY %u", static_cast<unsigned>(id));

        if (Ui_SelectableValue(label, unlocked ? "DONE" : (available ? "UNLOCK" : "DECLARED"), unlocked) && available)
        {
            s_LastId = static_cast<int>(id);
            s_LastOk = Engine_Achievement_Unlock(id);
        }
    }
    if (count == 0)
        Ui_LabelColored("THIS TITLE DECLARES NONE", UiColor::TextDim);

    if (s_LastId >= 0)
    {
        char outcome[48];
        snprintf(outcome, sizeof(outcome), "TROPHY %d", s_LastId);
        Ui_Separator();
        Ui_LabelValue(outcome, s_LastOk ? "ACCEPTED" : "REFUSED");
        if (!s_LastOk)
            Testbed_DrawWrapped("REFUSED IS REPORTED, NOT SILENT. THE LOG CARRIES THE CODE. A PLATINUM CANNOT BE "
                                "UNLOCKED DIRECTLY - THE CONSOLE AWARDS IT WHEN EVERY OTHER TROPHY IS UNLOCKED.",
                                UiColor::TextDim);
    }
    Ui_EndPanel();

    Ui_BeginPanel("NOTES", PANEL_MARGIN + width + COLUMN_GAP, PANEL_MARGIN, width, height);
    Ui_LabelValue("SUBSYSTEM", Engine_Subsystem_IsEnabled(EngineSubsystem::Achievement) ? "REQUESTED" : "NOT REQUESTED");
    Ui_Separator();
    Ui_Label("DECLARED IS WHAT THIS");
    Ui_Label("TITLE PACKAGED.");
    Ui_Label("RECORDING IS WHETHER THE");
    Ui_Label("CONSOLE WILL ACCEPT ONE.");
    Ui_Label("THEY FAIL SEPARATELY AND");
    Ui_Label("NEED OPPOSITE FIXES.");
    Ui_Separator();
    Ui_Label("UNLOCKING IS ONE WAY");
    Ui_Label("AND IDEMPOTENT.");
    Ui_Label("NO PROGRESS, NO TIERS,");
    Ui_Label("NO METADATA.");
    Ui_EndPanel();
}
