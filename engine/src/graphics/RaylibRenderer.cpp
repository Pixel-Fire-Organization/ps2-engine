#include "../include/graphics/RaylibRenderer.h"
#include <raylib.h>

#include "../include/graphics/DrawList.h"
#include "EngineApp.h"
#include "Macros.h"


RaylibRenderer::RaylibRenderer(const EngineConfig& config)
{
    Engine_LogInfo("Video Mode: %dx%d (%s)", GFX_SCREEN_WIDTH, GFX_SCREEN_HEIGHT, GFX_SCREEN_REGION_STR);

    InitWindow(GFX_SCREEN_WIDTH, GFX_SCREEN_HEIGHT, config.windowTitle);
    Engine_LogInfo("Waiting for window ready!");
    auto windowReadyBase = GetTime();
    while (!IsWindowReady())
        ;
    m_initialized = IsWindowReady();
    Engine_LogInfo("Window Ready in %d ms", GetTime() - windowReadyBase);
}

static void AddPrimitive(DrawLists& lists, Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color, int32_t textureId)
{
    PrimitiveDrawEntry entry;
    entry.transform = Transform3D(position, rotation, scale);
    entry.color = color;
    entry.type = primitive;
    entry.textureId = textureId;
    lists.AddPrimitive(entry);
}

void RaylibRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale)
{
    AddPrimitive(m_drawLists, primitive, position, rotation, scale, {1.f, 1.f, 1.f}, -1);
}
void RaylibRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color)
{
    AddPrimitive(m_drawLists, primitive, position, rotation, scale, color, -1);
}
void RaylibRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, int32_t textureId)
{
    AddPrimitive(m_drawLists, primitive, position, rotation, scale, {1.f, 1.f, 1.f}, textureId);
}
void RaylibRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color, int32_t textureId)
{
    AddPrimitive(m_drawLists, primitive, position, rotation, scale, color, textureId);
}
void RaylibRenderer::AddUIToDrawList(const UI& ui, const Vector2& offset, const Vector2& scale)
{
    UNUSED_VAR(ui);
    UNUSED_VAR(offset);
    UNUSED_VAR(scale);
}
void RaylibRenderer::AddLevelToDrawList(const Level& level) { UNUSED_VAR(level); }
void RaylibRenderer::AddModelToDrawList(int32_t modelId, const Vector3& position, const Vector3& rotation, const Vector3& scale)
{
    UNUSED_VAR(modelId);
    UNUSED_VAR(position);
    UNUSED_VAR(rotation);
    UNUSED_VAR(scale);
}
void RaylibRenderer::AddSkyToDrawList(int32_t resourceId) { UNUSED_VAR(resourceId); }

void RaylibRenderer::ClearDrawLists() { m_drawLists.Reset(false); }
void RaylibRenderer::Render() { m_drawLists.Render(); }

CameraID RaylibRenderer::AddCamera() { return -1; }

void RaylibRenderer::SetActiveCamera3D(CameraID id, const Camera3D& camera)
{
    UNUSED_VAR(id);
    m_drawLists.SetActiveCamera3D(camera);
}

void RaylibRenderer::SetActiveCamera2D(CameraID id, const Camera2D& camera)
{
    UNUSED_VAR(id);
    m_drawLists.SetActiveCamera2D(camera);
}

void RaylibRenderer::SetCameraState(CameraID id, bool enabled, const Vector2& pos, const Vector2& target)
{
    UNUSED_VAR(id);
    UNUSED_VAR(enabled);
    UNUSED_VAR(pos);
    UNUSED_VAR(target);
}

void RaylibRenderer::ResetCameraState(CameraID id) { UNUSED_VAR(id); }

bool RaylibRenderer::IsInitialized() const { return m_initialized; }
void RaylibRenderer::Shutdown()
{
    CloseWindow();
    m_initialized = false;
}
