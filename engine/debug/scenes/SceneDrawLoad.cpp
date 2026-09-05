#include <cmath>
#include <cstdio>

#include "EngineCore.h"
#include "EngineUi.h"
#include "TestbedScene.h"
#include "graphics/Renderer.h"
#include "platform/Platform.h"

namespace
{
    const int PANEL_MARGIN = 16;
    const int PANEL_WIDTH = 250;
    const float ORBIT_RADIUS = 12.0f;
    const float ORBIT_RATE = 0.25f;
    const float RING_SPREAD = 7.0f;
    const float MARKER_SIZE = 0.35f;

    // Half the draw list, so the count can be pushed to the ceiling without the
    // rest of the frame being what overflows it.
    const int MAX_COUNT = GFX_MAX_DRAW_LIST_LENGTH;

    int s_Count = 64;
    float s_Time = 0.0f;
} // namespace

void Scene_DrawLoad_Init()
{
    s_Count = 64;
    s_Time = 0.0f;
}

void Scene_DrawLoad_Update(float dt)
{
    s_Time += dt;

    Renderer* renderer = Engine_GetRenderer();
    const Platform* platform = Engine_GetPlatform();
    if (!renderer || !platform)
        return;

    renderer->ClearFrame(Color3{0.06f, 0.07f, 0.10f});

    Camera3D camera;
    camera.position = Vector3{ORBIT_RADIUS * sinf(s_Time * ORBIT_RATE), 6.0f, ORBIT_RADIUS * cosf(s_Time * ORBIT_RATE)};
    camera.target = Vector3{0.0f, 0.0f, 0.0f};
    camera.up = Vector3{0.0f, 1.0f, 0.0f};
    camera.fovy = 55.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    renderer->SetCamera3D(static_cast<CameraID>(0), camera);
    renderer->SetActiveCamera3D(static_cast<CameraID>(0));

    for (int i = 0; i < s_Count; ++i)
    {
        const float turn = static_cast<float>(i) * 0.618f;
        const float radius = RING_SPREAD * (0.2f + 0.8f * (static_cast<float>(i % 64) / 64.0f));
        const float x = radius * sinf(turn * 6.283f);
        const float z = radius * cosf(turn * 6.283f);
        const float y = sinf(s_Time + static_cast<float>(i) * 0.05f) * 2.0f;
        const float shade = static_cast<float>(i % 32) / 32.0f;
        renderer->AddPrimitiveToDrawList(Primitive3D::Cube, Vector3{x, y, z}, Vector3{0.0f, turn, 0.0f},
                                         Vector3{MARKER_SIZE, MARKER_SIZE, MARKER_SIZE}, Color3{0.3f + shade * 0.7f, 0.8f - shade * 0.4f, 0.9f});
    }

    const int screenH = static_cast<int>(platform->GetConstant(PlatformConstant::ScreenHeight));
    Ui_BeginPanel("DRAW LOAD", PANEL_MARGIN, PANEL_MARGIN, PANEL_WIDTH, screenH - PANEL_MARGIN * 2);

    // Clamped to the ceiling, never past it: this measures the cost of a full
    // draw list, it does not test what happens beyond one.
    Ui_SliderInt("COUNT", &s_Count, 1, MAX_COUNT);
    Ui_Bar("OF DRAW LIST", s_Count, MAX_COUNT);

    const DrawStats stats = renderer->GetLastStats();
    char text[48];
    snprintf(text, sizeof(text), "%u", static_cast<unsigned>(stats.primitiveCount));
    Ui_LabelValue("SUBMITTED", text);
    snprintf(text, sizeof(text), "%u", static_cast<unsigned>(stats.entriesCulled));
    Ui_LabelValue("CULLED", text);
    snprintf(text, sizeof(text), "%u", static_cast<unsigned>(stats.trisSubmitted));
    Ui_LabelValue("TRIS", text);
    snprintf(text, sizeof(text), "%.1f", static_cast<double>(Engine_GetFPS()));
    Ui_LabelValue("FPS", text);
    Ui_Separator();
    Ui_Label("DPAD LEFT RIGHT CHANGES");
    Ui_Label("THE COUNT STOPS AT THE");
    Ui_Label("DRAW LIST CEILING");
    Ui_EndPanel();
}
