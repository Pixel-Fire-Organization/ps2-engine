#include "EngineAchievementUi.h"

#include <cstdio>
#include <cstring>

#include "EngineAchievement.h"
#include "EngineUi.h"
#include "platform/Platform.h"

namespace
{
    const uint32_t TOAST_QUEUE = 8;
    const float TOAST_SECONDS = 4.0f;
    const int TOAST_WIDTH = 220;
    const int TOAST_MARGIN = 12;
    const int SCREEN_MARGIN_RATIO = 10;
    const int ROWS_PER_PAGE = 8;

    uint8_t s_Queue[TOAST_QUEUE];
    uint32_t s_Queued = 0;
    float s_Elapsed = 0.0f;

    bool s_ScreenOpen = false;
    uint32_t s_Page = 0;

    const char* GradeName(AchievementGrade grade)
    {
        switch (grade)
        {
        case AchievementGrade::Platinum:
            return "PLATINUM";
        case AchievementGrade::Gold:
            return "GOLD";
        case AchievementGrade::Silver:
            return "SILVER";
        case AchievementGrade::Bronze:
            return "BRONZE";
        }
        return "BRONZE";
    }

    void DrawToast()
    {
        if (s_Queued == 0)
            return;

        Platform* platform = Engine_GetPlatform();
        if (!platform)
            return;

        const uint32_t id = s_Queue[0];
        const int screenW = static_cast<int>(platform->GetConstant(PlatformConstant::ScreenWidth));
        const int scale = Ui_GetStyle().textScale;
        const int lineHeight = Ui_TextHeight(scale);
        const int height = lineHeight * 3 + Ui_GetStyle().panelPadding * 2;

        Ui_BeginPanel("ACHIEVEMENT UNLOCKED", screenW - TOAST_WIDTH - TOAST_MARGIN, TOAST_MARGIN, TOAST_WIDTH, height);
        Ui_LabelColored(Engine_Achievement_GetName(id), UiColor::TextAccent);
        Ui_Label(GradeName(Engine_Achievement_GetGrade(id)));
        Ui_EndPanel();
    }

    void AdvanceToast(float dt)
    {
        if (s_Queued == 0)
            return;

        s_Elapsed += dt;
        if (s_Elapsed < TOAST_SECONDS)
            return;

        s_Elapsed = 0.0f;
        for (uint32_t i = 1; i < s_Queued; ++i)
            s_Queue[i - 1] = s_Queue[i];
        --s_Queued;
    }

    void DrawScreen()
    {
        Platform* platform = Engine_GetPlatform();
        if (!platform)
            return;

        const int screenW = static_cast<int>(platform->GetConstant(PlatformConstant::ScreenWidth));
        const int screenH = static_cast<int>(platform->GetConstant(PlatformConstant::ScreenHeight));
        const int margin = screenW / SCREEN_MARGIN_RATIO;

        const uint32_t count = Engine_Achievement_GetCount();
        const uint32_t pages = (count + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE;
        if (s_Page >= pages && pages > 0)
            s_Page = pages - 1;

        Ui_BeginPanel("ACHIEVEMENTS", margin, margin, screenW - margin * 2, screenH - margin * 2);

        uint32_t earned = 0;
        for (uint32_t i = 0; i < count; ++i)
        {
            if (Engine_Achievement_IsUnlocked(i))
                ++earned;
        }

        char summary[64];
        snprintf(summary, sizeof(summary), "%u OF %u", static_cast<unsigned>(earned), static_cast<unsigned>(count));
        Ui_LabelValue("EARNED", summary);
        if (!Engine_Achievement_IsPersistent())
            Ui_LabelColored("NOT SAVED ON THIS SYSTEM", UiColor::TextWarn);
        Ui_Separator();

        const uint32_t first = s_Page * ROWS_PER_PAGE;
        for (uint32_t i = first; i < count && i < first + ROWS_PER_PAGE; ++i)
        {
            const bool unlocked = Engine_Achievement_IsUnlocked(i);
            const bool concealed = Engine_Achievement_IsHidden(i) && !unlocked;

            Ui_LabelValue(concealed ? "HIDDEN" : Engine_Achievement_GetName(i), unlocked ? "EARNED" : "LOCKED");
            if (!concealed)
                Ui_LabelColored(Engine_Achievement_GetDetail(i), UiColor::TextDim);
        }

        Ui_Separator();
        if (pages > 1)
        {
            char pageText[32];
            snprintf(pageText, sizeof(pageText), "PAGE %u OF %u", static_cast<unsigned>(s_Page + 1),
                     static_cast<unsigned>(pages));
            Ui_Label(pageText);
            if (Ui_Button("NEXT PAGE"))
                s_Page = (s_Page + 1) % pages;
        }

        if (Ui_Button("CLOSE") || Ui_WasBackPressed())
            Engine_Achievement_CloseScreen();

        Ui_EndPanel();
    }
}

void Engine_AchievementUi_Notify(uint32_t id)
{
    if (s_Queued >= TOAST_QUEUE)
        return;
    if (s_Queued == 0)
        s_Elapsed = 0.0f;
    s_Queue[s_Queued++] = static_cast<uint8_t>(id);
}

void Engine_AchievementUi_Reset()
{
    s_Queued = 0;
    s_Elapsed = 0.0f;
    s_ScreenOpen = false;
    s_Page = 0;
}

void Engine_Achievement_OpenScreen()
{
    s_ScreenOpen = true;
    s_Page = 0;
}

void Engine_Achievement_CloseScreen() { s_ScreenOpen = false; }

bool Engine_Achievement_IsScreenOpen() { return s_ScreenOpen; }

void Engine_Achievement_Update(float dt)
{
    if (!Engine_Ui_IsActive())
        return;

    if (s_ScreenOpen)
        DrawScreen();

    DrawToast();
    AdvanceToast(dt);
}
