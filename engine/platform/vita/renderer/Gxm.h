#pragma once

#include "EngineCore.h"
#include "graphics/Renderer.h"
#include "graphics/StagedGeometry.h"

extern "C" {
#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>
}

#define GXM_MAX_RESIDENT_TEXTURES 256

/// Default Vita backend, driving sceGxm directly with build-time shaders.
class GxmRenderer final : public Renderer
{
public:
    GxmRenderer() = delete;
    explicit GxmRenderer(const EngineConfig& config);
    ~GxmRenderer() override = default;

    GxmRenderer(const GxmRenderer&) = delete;
    GxmRenderer(GxmRenderer&&) = delete;
    GxmRenderer& operator=(const GxmRenderer&) = delete;
    GxmRenderer& operator=(GxmRenderer&&) = delete;

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

    /// Upload a cooked texture into graphics memory.
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
    struct DisplayBuffer
    {
        void* address;
        SceUID uid;
        SceGxmColorSurface surface;
        SceGxmSyncObject* sync;
    };

    struct Texture
    {
        SceGxmTexture texture;
        void* data;
        SceUID uid;
        bool used;
    };

    bool InitPrimitives();
    bool InitGraphics();
    bool InitRenderTarget();
    bool InitShaders();
    bool InitBuffers();
    void DestroyGraphics();

    void DrawStagedGeometry();
    void DrawClearQuad();
    void UploadVertices();

    SceGxmContext* m_context;
    void* m_vdmRing;
    void* m_vertexRing;
    void* m_fragmentRing;
    void* m_fragmentUsseRing;
    SceUID m_vdmRingUid;
    SceUID m_vertexRingUid;
    SceUID m_fragmentRingUid;
    SceUID m_fragmentUsseRingUid;
    void* m_hostMem;

    SceGxmRenderTarget* m_renderTarget;
    DisplayBuffer m_displayBuffers[GFX_GXM_DISPLAY_BUFFERS];
    uint32_t m_backBufferIndex;
    uint32_t m_frontBufferIndex;

    void* m_depthData;
    SceUID m_depthUid;
    SceGxmDepthStencilSurface m_depthSurface;

    SceGxmShaderPatcher* m_shaderPatcher;
    void* m_patcherBuffer;
    void* m_patcherVertexUsse;
    void* m_patcherFragmentUsse;
    SceUID m_patcherBufferUid;
    SceUID m_patcherVertexUsseUid;
    SceUID m_patcherFragmentUsseUid;

    SceGxmShaderPatcherId m_vertexProgramId;
    SceGxmShaderPatcherId m_fragmentProgramId;
    SceGxmVertexProgram* m_vertexProgram;
    SceGxmFragmentProgram* m_fragmentProgram;
    const SceGxmProgramParameter* m_viewProjParam;

    void* m_vertexBuffer;
    void* m_indexBuffer;
    SceUID m_vertexBufferUid;
    SceUID m_indexBufferUid;

    Texture m_textures[GXM_MAX_RESIDENT_TEXTURES];
    uint32_t m_whiteTexture;

    StagedGeometry m_geometry;

    Color3 m_clearColor;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_frameVertices;
    uint32_t m_frame3DVertices;
    uint32_t m_frame2DVertices;
    uint32_t m_reportedOverflow;

    DrawStats m_frameStats;
    bool m_sceneActive;
    bool m_initialized;
};
