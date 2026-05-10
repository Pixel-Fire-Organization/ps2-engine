#pragma once

#ifdef USE_RAYLIB

    #include "DrawList.h"
    #include "EngineCore.h"
    #include "Renderer.h"

class RaylibRenderer final : public Renderer
{
    bool m_initialized = false;

    // ARENA_RENDERER scratch buffer — ps2gl requires stride=0 arrays.
    float* m_megaBatch = nullptr;

    // Shared per-frame glCallList counter — guards CurPacket against overflow.
    // Reset each Render(); both RenderPrimitives and RenderModels increment it.
    uint16_t m_frameDrawCallsUsed = 0;

    uint16_t m_framePrimCount = 0;
    uint16_t m_frameModelMeshCount = 0;

    struct ModelDListEntry
    {
        int32_t resourceId = -1;
        unsigned int handles[GFX_MAX_MODEL_MESH_COUNT];
        uint8_t meshCount = 0;
    };

    ModelDListEntry m_modelDListCache[GFX_MAX_CACHED_MODELS]{};
    uint8_t m_modelDListCacheCount = 0;

    ModelDListEntry* FindOrCompileModelDLists(const Model* model, int32_t resourceId);
    void ClearModelDListCache();

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

    DrawStats GetLastStats() const override;
    Camera3D GetActiveCamera3D() const override;

protected:
    void RenderSkybox(const DrawLists& lists) override;
    void RenderPrimitives(DrawLists& lists) override;
    void RenderModels(const DrawLists& lists) override;
    void RenderUI(const DrawLists& lists) override;
};
#endif