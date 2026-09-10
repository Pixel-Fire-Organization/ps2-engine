#include <cstdio>
#include <cstring>

#include "EngineCore.h"
#include "EngineIO.h"
#include "EngineResource.h"
#include "EngineSubsystems.h"
#include "EngineUi.h"
#include "TestbedScene.h"
#include "platform/Platform.h"

namespace
{
    const int PANEL_MARGIN = 16;
    const int COLUMN_GAP = 8;
    const int SLOTS = 4;
    const uint32_t TABLE_ROWS_SHOWN = 8;
    const int KEY_CHARS_SHOWN = 18;
    const char* const TEXTURE_PATH = "RASSETS\\BOX.PS2A";

    const char* TypeName(ResourceType type)
    {
        switch (type)
        {
        case RES_TEXTURE:
            return "TEX";
        case RES_MODEL:
            return "MDL";
        case RES_SOUND:
            return "SND";
        case RES_FONT:
            return "FNT";
        case RES_THEME:
            return "THM";
        }
        return "?";
    }

    /// @param key The canonical key.
    /// @return Its tail, which is the part that identifies it in a narrow panel.
    const char* ShortKey(const char* key)
    {
        const int length = static_cast<int>(strlen(key));
        return (length <= KEY_CHARS_SHOWN) ? key : (key + length - KEY_CHARS_SHOWN);
    }

    int32_t s_Handles[SLOTS];

    void LoadInto(int slot)
    {
        char path[IO_FILE_MAX_PATH];
        if (!Engine_BuildPath(Engine_GetResourceLocationToken(), TEXTURE_PATH, path, sizeof(path)))
            return;
        s_Handles[slot] = Engine_Resource_Load(RES_TEXTURE, path);
    }
} // namespace

void Scene_Resources_Init()
{
    for (int i = 0; i < SLOTS; ++i)
        s_Handles[i] = -1;
}

void Scene_Resources_Update(float dt)
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

    Ui_BeginPanel("RESOURCES", PANEL_MARGIN, PANEL_MARGIN, width, height);
    if (!Engine_Subsystem_IsEnabled(EngineSubsystem::Resource))
    {
        Testbed_DrawUnavailable("RESOURCE SUBSYSTEM");
        Ui_EndPanel();
        return;
    }

    Ui_Label(TEXTURE_PATH);
    Ui_Separator();

    for (int i = 0; i < SLOTS; ++i)
    {
        char label[32];
        snprintf(label, sizeof(label), "SLOT %d", i);

        if (s_Handles[i] < 0)
        {
            char row[48];
            snprintf(row, sizeof(row), "%s LOAD", label);
            if (Ui_Selectable(row, false))
                LoadInto(i);
            continue;
        }

        char row[64];
        snprintf(row, sizeof(row), "%s H%d %s UNLOAD", label, static_cast<int>(s_Handles[i]), Engine_Resource_IsReady(s_Handles[i]) ? "READY" : "LOADING");
        if (Ui_Selectable(row, true))
        {
            Engine_Resource_Unload(s_Handles[i]);
            s_Handles[i] = -1;
        }
    }
    Ui_EndPanel();

    Ui_BeginPanel("BUDGET", PANEL_MARGIN + width + COLUMN_GAP, PANEL_MARGIN, width, height);
    Ui_Bar("TEXTURE KB", static_cast<int>(Engine_Resource_GetTextureBudgetUsed() / 1024u), static_cast<int>(Engine_Resource_GetTextureBudget() / 1024u));

    char text[48];
    snprintf(text, sizeof(text), "%u", static_cast<unsigned>(RES_MAX_ENTRIES));
    Ui_LabelValue("HANDLE SLOTS", text);
    snprintf(text, sizeof(text), "%u KB", static_cast<unsigned>(platform->GetConstant(PlatformConstant::MaxTextureBytes) / 1024u));
    Ui_LabelValue("MAX TEXTURE", text);
    Ui_Separator();
    Ui_Header("LIVE TABLE");

    const uint32_t capacity = Engine_Resource_GetCapacity();
    uint32_t live = 0;
    uint32_t shown = 0;
    for (uint32_t i = 0; i < capacity; ++i)
    {
        ResourceInfo info;
        if (!Engine_Resource_GetInfo(static_cast<int32_t>(i), &info))
            continue;
        ++live;
        if (shown >= TABLE_ROWS_SHOWN)
            continue;
        ++shown;

        char row[64];
        snprintf(row, sizeof(row), "%s%s R%u", info.pinned ? "PIN " : "", TypeName(info.type), static_cast<unsigned>(info.refCount));
        Ui_LabelValue(row, ShortKey(info.key));
    }

    if (live == 0)
        Ui_LabelColored("NOTHING RESIDENT", UiColor::TextDim);

    snprintf(text, sizeof(text), "%u / %u", static_cast<unsigned>(live), static_cast<unsigned>(capacity));
    Ui_LabelValue("SLOTS USED", text);
    Ui_EndPanel();
}
