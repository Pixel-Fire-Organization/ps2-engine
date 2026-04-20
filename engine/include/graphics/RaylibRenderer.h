#pragma once

#ifdef USE_RAYLIB
    #include "DrawList.h"
    #include "EngineCore.h"
    #include "Renderer.h"

class RaylibRenderer final : public Renderer
{
    DrawLists m_drawLists;

    bool m_initialized = false;

public:
    RaylibRenderer() = delete;
    explicit RaylibRenderer(const EngineConfig& config);

    RaylibRenderer(const RaylibRenderer&) = delete;
    RaylibRenderer(RaylibRenderer&&) = delete;

    RaylibRenderer& operator=(const RaylibRenderer&) = delete;
    RaylibRenderer& operator=(RaylibRenderer&&) = delete;

    ~RaylibRenderer() override = default;

    void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale) override;
    void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color) override;
    void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, int32_t textureId) override;
    void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color, int32_t textureId) override;
    void AddUIToDrawList(const UI& ui, const Vector2& offset, const Vector2& scale) override;
    void AddLevelToDrawList(const Level& level) override;
    void AddModelToDrawList(int32_t modelId, const Vector3& position, const Vector3& rotation, const Vector3& scale) override;
    void AddSkyToDrawList(int32_t resourceId) override;
    void ClearDrawLists() override;

    void Render() override;

    CameraID AddCamera() override;
    void SetActiveCamera3D(CameraID id, const Camera3D& camera) override;
    void SetActiveCamera2D(CameraID id, const Camera2D& camera) override;

    void SetCameraState(CameraID id, bool enabled, const Vector2& pos, const Vector2& target) override;
    void ResetCameraState(CameraID id) override;

    bool IsInitialized() const override;
    void Shutdown() override;
};

#else
    #error Use of Raylib is turned off
#endif
