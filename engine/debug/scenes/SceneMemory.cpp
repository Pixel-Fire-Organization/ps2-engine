#include <cstdio>

#include "EngineMemory.h"
#include "EngineResource.h"
#include "EngineUi.h"
#include "TestbedScene.h"
#include "platform/Platform.h"

namespace
{
    const int PANEL_MARGIN = 16;
    const int COLUMN_GAP = 8;

    int Kilobytes(size_t bytes) { return static_cast<int>(bytes / 1024u); }

    void ArenaBar(const char* label, ArenaType type)
    {
        size_t capacity = 0;
        size_t used = 0;
        Engine_GetArenaStats(type, &capacity, &used);
        Ui_Bar(label, Kilobytes(used), Kilobytes(capacity));
    }
} // namespace

void Scene_Memory_Init() {}

void Scene_Memory_Update(float dt)
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

    Ui_BeginPanel("ARENAS AND POOL", PANEL_MARGIN, PANEL_MARGIN, width, height);
    ArenaBar("CONFIG KB", ARENA_CONFIG);
    ArenaBar("LEVEL DATA KB", ARENA_LEVEL_DATA);
    ArenaBar("RENDERER KB", ARENA_RENDERER);

    size_t poolCapacity = 0;
    size_t poolUsed = 0;
    Engine_GetPoolStatsMain(&poolCapacity, &poolUsed);
    Ui_Bar("MAIN POOL KB", Kilobytes(poolUsed), Kilobytes(poolCapacity));
    Ui_Separator();
    Ui_Label("THESE SHOULD READ THE");
    Ui_Label("SAME ON ENTRY TO EVERY");
    Ui_Label("SCENE. THAT IS THE");
    Ui_Label("RESET WORKING.");
    Ui_Label("THE RENDERER ARENA IS");
    Ui_Label("NEVER EMPTIED.");
    Ui_EndPanel();

    Ui_BeginPanel("HEAP AND TEXTURES", PANEL_MARGIN + width + COLUMN_GAP, PANEL_MARGIN, width, height);

    size_t heapTotal = 0;
    size_t heapUsed = 0;
    size_t heapFree = 0;
    Engine_GetHeapStats(&heapTotal, &heapUsed, &heapFree);
    Ui_Bar("HEAP KB", Kilobytes(heapUsed), Kilobytes(heapTotal));

    char text[48];
    snprintf(text, sizeof(text), "%d KB", Kilobytes(heapFree));
    Ui_LabelValue("HEAP FREE", text);
    snprintf(text, sizeof(text), "%u KB", static_cast<unsigned>(platform->GetConstant(PlatformConstant::MemoryTotalBudget) / 1024u));
    Ui_LabelValue("BUDGET", text);

    Ui_Separator();
    Ui_Header("TEXTURES");
    Ui_Bar("TEXTURE KB", static_cast<int>(Engine_Resource_GetTextureBudgetUsed() / 1024u), static_cast<int>(Engine_Resource_GetTextureBudget() / 1024u));

    Ui_Separator();
    Ui_Header("INTERFACE");
    Ui_Bar("UI QUADS", static_cast<int>(Ui_QuadsUsed()), static_cast<int>(Ui_QuadBudget()));
    Ui_EndPanel();
}
