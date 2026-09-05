#include <cstdio>

#include "EngineCore.h"
#include "EngineUi.h"
#include "TestbedScene.h"
#include "graphics/Renderer.h"
#include "platform/Platform.h"

namespace
{
    const int PANEL_MARGIN = 16;

    const char* RendererName(RendererType type)
    {
        switch (type)
        {
        case RendererType::Null:
            return "NULL";
        case RendererType::Ps2Gl:
            return "PS2GL";
        case RendererType::GifTag:
            return "GIFTAG";
        case RendererType::OpenGl:
            return "OPENGL";
        case RendererType::WebGpu:
            return "WEBGPU";
        case RendererType::Gxm:
            return "GXM";
        case RendererType::VitaGl:
            return "VITAGL";
        }
        return "?";
    }

    void Row(const char* label, uint32_t value)
    {
        char text[32];
        snprintf(text, sizeof(text), "%u", static_cast<unsigned>(value));
        Ui_LabelValue(label, text);
    }

    void CapabilityRow(const Platform& platform, const char* label, PlatformCapability key)
    {
        Ui_LabelValue(label, platform.HasCapability(key) ? "YES" : "NO");
    }
} // namespace

void Scene_PlatformInfo_Init() {}

void Scene_PlatformInfo_Update(float dt)
{
    (void)dt;

    const Platform* platform = Engine_GetPlatform();
    if (!platform)
        return;

    const int screenW = static_cast<int>(platform->GetConstant(PlatformConstant::ScreenWidth));
    const int screenH = static_cast<int>(platform->GetConstant(PlatformConstant::ScreenHeight));
    const Renderer* renderer = Engine_GetRenderer();

    Ui_Rect(0, 0, screenW, screenH, UiColor::WindowBackground);

    const int width = (screenW - PANEL_MARGIN * 3) / 2;
    const int height = screenH - PANEL_MARGIN * 2;

    Ui_BeginPanel("PLATFORM", PANEL_MARGIN, PANEL_MARGIN, width, height);
    Ui_LabelValue("NAME", platform->GetName());
    Ui_LabelValue("RENDERER", renderer ? RendererName(renderer->GetRendererType()) : "NONE");

    char resolution[32];
    snprintf(resolution, sizeof(resolution), "%dX%d", screenW, screenH);
    Ui_LabelValue("FRAMEBUFFER", resolution);

    char aspect[32];
    snprintf(aspect, sizeof(aspect), "%u:%u", static_cast<unsigned>(platform->GetConstant(PlatformConstant::DisplayAspectX)),
             static_cast<unsigned>(platform->GetConstant(PlatformConstant::DisplayAspectY)));
    Ui_LabelValue("DISPLAY ASPECT", aspect);

    Row("FRAME BUDGET US", platform->GetConstant(PlatformConstant::TargetFrameMicros));
    Row("PAD PORTS", platform->GetConstant(PlatformConstant::MaxGamepadPorts));
    Ui_Separator();
    Ui_Header("CAPABILITIES");
    CapabilityRow(*platform, "GAMEPAD", PlatformCapability::Gamepad);
    CapabilityRow(*platform, "KEYBOARD", PlatformCapability::Keyboard);
    CapabilityRow(*platform, "MOUSE", PlatformCapability::Mouse);
    CapabilityRow(*platform, "TOUCH", PlatformCapability::Touch);
    CapabilityRow(*platform, "ANALOG TRIGGERS", PlatformCapability::AnalogTriggers);
    CapabilityRow(*platform, "RESIZABLE WINDOW", PlatformCapability::ResizableWindow);
    CapabilityRow(*platform, "ASYNC IO", PlatformCapability::AsyncIo);
    CapabilityRow(*platform, "FILE WRITE", PlatformCapability::FileWrite);
    Ui_EndPanel();

    Ui_BeginPanel("BUDGETS", PANEL_MARGIN * 2 + width, PANEL_MARGIN, width, height);
    Row("TOTAL KB", platform->GetConstant(PlatformConstant::MemoryTotalBudget) / 1024u);
    Row("CONFIG KB", platform->GetConstant(PlatformConstant::MemoryArenaConfigSize) / 1024u);
    Row("LEVEL DATA KB", platform->GetConstant(PlatformConstant::MemoryArenaLevelDataSize) / 1024u);
    Row("RENDERER KB", platform->GetConstant(PlatformConstant::MemoryArenaRendererSize) / 1024u);
    Row("POOL KB", platform->GetConstant(PlatformConstant::MemoryPoolMainSize) / 1024u);
    Row("SLOT ALIGN KB", platform->GetConstant(PlatformConstant::MemoryArenaSlotAlignment) / 1024u);
    Ui_Separator();
    Ui_Header("GRAPHICS");
    Row("TEXTURE KB", platform->GetConstant(PlatformConstant::TextureBudgetBytes) / 1024u);
    Row("MAX TEXTURE KB", platform->GetConstant(PlatformConstant::MaxTextureBytes) / 1024u);
    Row("MAX TEXTURE W", platform->GetConstant(PlatformConstant::MaxTextureWidth));
    Row("MAX TEXTURE H", platform->GetConstant(PlatformConstant::MaxTextureHeight));
    Row("DRAW LIST", GFX_MAX_DRAW_LIST_LENGTH);
    Row("UI QUAD BUDGET", Ui_QuadBudget());
    Row("UI QUADS USED", Ui_QuadsUsed());
    Ui_EndPanel();
}
