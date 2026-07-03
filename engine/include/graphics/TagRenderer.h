#pragma once

#include "DrawList.h"
#include "EngineCore.h"
#include "Renderer.h"

#ifdef RENDERER_BACKEND_GIFTAG

extern "C"
{
#include <draw.h>
#include <graph.h>
#include <packet2.h>
}

// GIFTAG renderer — bypasses ps2gl and drives the GS directly. GS setup and
// packet assembly use the PS2SDK graph/draw/packet2 libraries; vertex transform
// is done on the EE with plain column-major float math (the same look-at / frustum
// convention as the PS2GL path), so it does NOT depend on math3d's matrix
// convention. Feature-parity target with PS2GL: primitives, models, textures, skybox.
//
// NOTE: first cut of the direct-GS path — structurally complete, but its GS
// register / packet details still need on-hardware (PCSX2) verification per the
// plan's verification pass.
class TagRenderer final : public Renderer
{
    bool m_initialized = false;
    bool m_inFrame = false;

    Color3 m_clearColor{0.0f, 0.0f, 0.0f};

    // Double-buffered frame buffers + a shared depth buffer.
    framebuffer_t m_frame[2];
    zbuffer_t m_z;
    int m_drawBuffer = 0; // index of the buffer currently being drawn to

    // Per-frame geometry packet (DMA chain) and a small packet for env setup.
    packet2_t* m_geom = nullptr;
    packet2_t* m_env = nullptr;

    // EE transform scratch (sized to the per-frame vertex cap).
    xyz_t* m_xyz = nullptr; // GS fixed-point xyz per emitted vertex
    uint32_t* m_srcIdx = nullptr; // source vertex index per emitted vertex (for UVs)
    float* m_q = nullptr; // 1/clip.w per emitted vertex — GS perspective-correct factor

    // Simple GS-VRAM bump allocator for textures (words), after frame/z buffers.
    uint32_t m_vramTexNext = 0;

    // Texture registry — Texture2D.id is (index + 1); 0 means "invalid".
    static constexpr uint16_t TAG_MAX_TEXTURES = 32;
    struct TexEntry
    {
        bool inUse;
        uint32_t gsAddr; // GS word address
        int width;
        int height;
        int psm; // GS pixel storage mode
    };
    TexEntry m_textures[TAG_MAX_TEXTURES]{};

    // Queued 2D rectangles (see GLRenderer for why DrawRect2D must be deferred).
    static constexpr uint16_t TAG_MAX_2D_RECTS = 64;
    struct Rect2D
    {
        int32_t x, y, w, h;
        Color3 color;
    };
    Rect2D m_rects2D[TAG_MAX_2D_RECTS];
    uint16_t m_rect2DCount = 0;

    uint16_t m_frameVertsUsed = 0;
    uint16_t m_framePrimCount = 0;
    uint16_t m_frameModelMeshCount = 0;

    // Column-major (OpenGL-style) matrix builders — identical convention to the
    // PS2GL renderer's ApplyProjection/ApplyCameraTransform.
    static void Mult4x4(float out[16], const float a[16], const float b[16]);
    void BuildViewMatrix(float out[16], const Camera3D& camera) const;
    void BuildProjMatrix(float out[16]) const;
    static void BuildModelMatrix(float out[16], const Vector3& pos, const Vector3& rot, const Vector3& scl);

    // Transform + emit one unindexed triangle list (verts: 3 floats/vertex;
    // uvs: 2 floats/vertex or null). mvp is the combined model→clip matrix.
    void DrawTriangles(const float mvp[16], const float* verts, const float* uvs,
                       uint32_t vertexCount, Color3 color, uint32_t textureId);
    void BindTexture(uint32_t textureId);
    void FlushRects2D();

public:
    TagRenderer() = delete;
    explicit TagRenderer(const EngineConfig& config);

    TagRenderer(const TagRenderer&) = delete;
    TagRenderer(TagRenderer&&) = delete;
    TagRenderer& operator=(const TagRenderer&) = delete;
    TagRenderer& operator=(TagRenderer&&) = delete;

    ~TagRenderer() override = default;

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

    uint32_t UploadTexture(const void* pixels, int width, int height, PixelFormat format) override;
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
};

#endif // RENDERER_BACKEND_GIFTAG
