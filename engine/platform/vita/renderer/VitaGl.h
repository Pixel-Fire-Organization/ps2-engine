#pragma once

#include "EngineCore.h"
#include "graphics/Renderer.h"
#include "graphics/StagedGeometry.h"

#define VITAGL_MAX_RESIDENT_TEXTURES 256

#define VITAGL_LEGACY_POOL_BYTES (1 * 1024 * 1024)

/// Fallback Vita backend: a fixed-function subset over sceGxm, provided by the
/// vendored vitaGL library. Requires the player-supplied shader compiler.
class VitaGlRenderer final : public Renderer
{
public:
    VitaGlRenderer() = delete;
    explicit VitaGlRenderer(const EngineConfig& config);
    ~VitaGlRenderer() override = default;

    VitaGlRenderer(const VitaGlRenderer&) = delete;
    VitaGlRenderer(VitaGlRenderer&&) = delete;
    VitaGlRenderer& operator=(const VitaGlRenderer&) = delete;
    VitaGlRenderer& operator=(VitaGlRenderer&&) = delete;

    RendererType GetRendererType() const override;

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
    void BeginFrame() override;
    void EndFrame() override;
    void DrawDebugOverlay() override;
    void ClearFrame(const Color3& color) override;
    void DrawRect2D(int32_t x, int32_t y, int32_t width, int32_t height, const Color3& color) override;
    void DrawGrid(int32_t slices, float spacing) override;

    void SetCamera3D(CameraID id, const Camera3D& camera) override;
    void SetActiveCamera3D(CameraID id) override;
    void SetActiveCamera2D(const Camera2D& camera) override;

    /// Upload a cooked texture.
    /// @param upload Source texture; expanded to RGBA8.
    /// @return A handle, or 0 on failure.
    uint32_t UploadTexture(const TextureUpload& upload) override;
    void ReleaseTexture(uint32_t handle) override;

    bool IsInitialized() const override;
    void Shutdown() override;

    DrawStats GetLastStats() const override;
    Camera3D GetActiveCamera3D() const override;

protected:
    void RenderSkybox(const DrawLists& lists) override;
    void RenderPrimitives(DrawLists& lists) override;
    void RenderModels(const DrawLists& lists) override;
    void RenderUI(const DrawLists& lists) override;

private:
    bool CreateWhiteTexture();
    void BindVertexArrays(const StagedGeometry::Vertex* base);
    void DrawStagedGeometry();

    uint32_t m_whiteTexture;
    uint32_t m_textures[VITAGL_MAX_RESIDENT_TEXTURES];

    StagedGeometry m_geometry;

    Color3 m_clearColor;
    uint32_t m_width;
    uint32_t m_height;

    DrawStats m_frameStats;
    bool m_initialized;
};
