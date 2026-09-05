#include <cmath>
#include <cstdio>

#include "EngineCore.h"
#include "EngineResource.h"
#include "EngineUi.h"
#include "TestbedScene.h"
#include "graphics/Renderer.h"
#include "platform/Platform.h"

namespace
{
    const int PANEL_MARGIN = 16;
    const int PANEL_WIDTH = 220;
    const float SPACING = 4.0f;
    const float SIZE = 2.0f;
    const float SPIN_RATE = 0.6f;

    int32_t s_Texture = -1;
    float s_Spin = 0.0f;

    void Shape(Primitive3D shape, float x, float y, Color3 color, int32_t texture)
    {
        Renderer* renderer = Engine_GetRenderer();
        if (!renderer)
            return;

        const Vector3 position = Vector3{x, y, 0.0f};
        const Vector3 rotation = Vector3{0.0f, s_Spin, 0.0f};
        const Vector3 scale = Vector3{SIZE, SIZE, SIZE};
        if (texture >= 0)
            renderer->AddPrimitiveToDrawList(shape, position, rotation, scale, color, texture);
        else
            renderer->AddPrimitiveToDrawList(shape, position, rotation, scale, color);
    }
} // namespace

void Scene_Primitives_Init()
{
    s_Spin = 0.0f;
    s_Texture = -1;

    char path[IO_FILE_MAX_PATH];
    if (Engine_BuildPath(Engine_GetResourceLocationToken(), "RASSETS\\BOX.PS2A", path, sizeof(path)))
        s_Texture = Engine_Resource_Load(RES_TEXTURE, path);
}

void Scene_Primitives_Update(float dt)
{
    s_Spin += dt * SPIN_RATE;

    Renderer* renderer = Engine_GetRenderer();
    const Platform* platform = Engine_GetPlatform();
    if (!renderer || !platform)
        return;

    renderer->ClearFrame(Color3{0.08f, 0.09f, 0.12f});

    Camera3D camera;
    camera.position = Vector3{0.0f, 3.0f, 14.0f};
    camera.target = Vector3{0.0f, 0.0f, 0.0f};
    camera.up = Vector3{0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    renderer->SetCamera3D(static_cast<CameraID>(0), camera);
    renderer->SetActiveCamera3D(static_cast<CameraID>(0));
    renderer->DrawGrid(20, 1.0f);

    const bool textured = (s_Texture >= 0) && Engine_Resource_IsReady(s_Texture);

    Shape(Primitive3D::Cube, -SPACING, 2.0f, Color3{0.85f, 0.25f, 0.25f}, -1);
    Shape(Primitive3D::Sphere, 0.0f, 2.0f, Color3{0.25f, 0.75f, 0.45f}, -1);
    Shape(Primitive3D::Cylinder, SPACING, 2.0f, Color3{0.35f, 0.55f, 0.9f}, -1);

    Shape(Primitive3D::Cube, -SPACING, -2.0f, Color3{1.0f, 1.0f, 1.0f}, textured ? s_Texture : -1);
    Shape(Primitive3D::Sphere, 0.0f, -2.0f, Color3{1.0f, 1.0f, 1.0f}, textured ? s_Texture : -1);
    Shape(Primitive3D::Cylinder, SPACING, -2.0f, Color3{1.0f, 1.0f, 1.0f}, textured ? s_Texture : -1);

    const int screenH = static_cast<int>(platform->GetConstant(PlatformConstant::ScreenHeight));
    Ui_BeginPanel("PRIMITIVES", PANEL_MARGIN, PANEL_MARGIN, PANEL_WIDTH, screenH / 2);
    Ui_Label("TOP ROW COLOURED");
    Ui_Label("LOWER ROW TEXTURED");
    Ui_LabelValue("TEXTURE", textured ? "READY" : (s_Texture >= 0 ? "LOADING" : "MISSING"));

    const DrawStats stats = renderer->GetLastStats();
    char text[32];
    snprintf(text, sizeof(text), "%u", static_cast<unsigned>(stats.primitiveCount));
    Ui_LabelValue("PRIMS", text);
    snprintf(text, sizeof(text), "%u", static_cast<unsigned>(stats.trisSubmitted));
    Ui_LabelValue("TRIS", text);
    Ui_EndPanel();
}
