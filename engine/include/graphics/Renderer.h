#pragma once

#include "Primitives.h"
#include "UI.h"
#include "EngineLevel.h"
#include "raylib.h"

class Renderer {
public:
    Renderer() = default;
    Renderer(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;

    void operator=(const Renderer&) = delete;
    void operator=(Renderer&&) = delete;

    virtual ~Renderer() = 0;

    virtual void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale) = 0;
    virtual void AddUIToDrawList(const UI& ui, const Vector2& offset, const Vector2& scale) = 0;
    virtual void AddLevelToDrawList(const Level& level) = 0;
    virtual void AddModelToDrawList(int32_t modelId, const Vector3& position, const Vector3& rotation, const Vector3& scale) = 0;
    virtual void AddSkyToDrawList(int32_t resourceId);
    virtual void ClearDrawLists() = 0;

    virtual void Render() = 0;

    virtual CameraID AddCamera();
    virtual void SetCameraState(CameraID id, bool enabled, const Vector2& pos, const Vector2& target) = 0;
    virtual void ResetCameraState(CameraID id) = 0;
};
