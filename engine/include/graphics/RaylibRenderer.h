#pragma once

#ifdef USE_RAYLIB

    #include "DrawList.h"
    #include "EngineCore.h"
    #include "Renderer.h"

class RaylibRenderer final : public Renderer
{
    bool m_initialized = false;

    // ARENA_RENDERER scratch buffer — holds separated geometry arrays
    // for ps2gl glDrawArrays (stride must be 0, no interleaved data).
    float* m_megaBatch = nullptr;

    // -----------------------------------------------------------------------
    // Per-frame shared draw-call budget (guards CurPacket from overflow).
    // Reset to 0 at the start of Render(); both RenderPrimitives and
    // RenderModels increment it and check it against GFX_DRAW_CALL_BUDGET.
    // -----------------------------------------------------------------------
    uint16_t m_frameDrawCallsUsed = 0;

    // Per-frame render-stat accumulators — committed to DrawStats at the
    // end of Render() once all sub-functions have finished.
    uint16_t m_framePrimCount = 0;
    uint16_t m_frameModelMeshCount = 0;

    // -----------------------------------------------------------------------
    // Model display list cache
    // -----------------------------------------------------------------------
    // Maps each unique model resource ID to the ps2gl display list handles
    // for its meshes.  Compiled on the first draw of each model; reused every
    // subsequent frame.  Freeing happens in ClearModelDListCache().
    //
    // Only unindexed meshes (mesh.indices == nullptr) are compiled.
    // Indexed meshes are skipped — glDrawElements() is a hard mError() in
    // ps2gl and pglDrawIndexedArrays() only supports unsigned-byte indices
    // (< 256 vertices), which is too small for real models.
    // -----------------------------------------------------------------------
    struct ModelDListEntry
    {
        int32_t resourceId = -1;
        unsigned int handles[GFX_MAX_MODEL_MESH_COUNT];
        uint8_t meshCount = 0;
    };

    ModelDListEntry m_modelDListCache[GFX_MAX_CACHED_MODELS]{};
    uint8_t m_modelDListCacheCount = 0;

    // Find an existing DList cache entry or compile a new one.
    // Returns nullptr when the cache is full or every mesh is unsupported.
    ModelDListEntry* FindOrCompileModelDLists(const Model* model, int32_t resourceId);

    // Free all compiled model display lists and reset the cache table.
    // Call this on level unload or before reloading models.
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