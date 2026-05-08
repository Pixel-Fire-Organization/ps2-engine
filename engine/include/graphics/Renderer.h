#pragma once

#include "EngineGraphics.h"
#include "EngineLevel.h"
#include "DrawList.h"

class Renderer {
public:
    Renderer() = default;
    Renderer(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;

    void operator=(const Renderer&) = delete;
    void operator=(Renderer&&) = delete;

    virtual ~Renderer() = default;

    virtual void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale) = 0;
    virtual void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color) = 0;
    virtual void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, int32_t textureId) = 0;
    virtual void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color, int32_t textureId) = 0;
    virtual void AddUIToDrawList(const UI& ui, const Vector2& offset, const Vector2& scale) = 0;
    virtual void AddLevelToDrawList(const Level& level) = 0;
    virtual void AddModelToDrawList(int32_t modelId, const Vector3& position, const Vector3& rotation, const Vector3& scale) = 0;
    virtual void AddSkyToDrawList(int32_t resourceId) = 0;
    virtual void ClearDrawLists() = 0;

    virtual void Render() = 0;

    virtual void SetActiveCamera3D(CameraID id, const Camera3D& camera) = 0;
    virtual void SetActiveCamera2D(CameraID id, const Camera2D& camera) = 0;

    virtual CameraID AddCamera() = 0;
    virtual void SetCameraState(CameraID id, bool enabled, const Vector2& pos, const Vector2& target) = 0;
    virtual void ResetCameraState(CameraID id) = 0;

    virtual bool IsInitialized() const = 0;
    virtual void Shutdown() = 0;

    virtual DrawStats GetLastStats() const = 0;
    virtual Camera3D GetActiveCamera3D() const = 0;

protected:
    DrawLists m_drawLists;

    virtual void RenderSkybox(const DrawLists& lists) = 0;
    virtual void RenderPrimitives(DrawLists& lists) = 0;
    virtual void RenderModels(const DrawLists& lists) = 0;
    virtual void RenderUI(const DrawLists& lists) = 0;
};
