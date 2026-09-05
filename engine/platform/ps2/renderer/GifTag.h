#pragma once

#include "EngineCore.h"
#include "graphics/DrawList.h"
#include "graphics/Renderer.h"


extern "C" {
#include <draw.h>
#include <graph.h>
#include <packet2.h>
}

class GifTagRenderer final : public Renderer
{
    bool m_initialized = false;
    bool m_inFrame = false;

    Color3 m_clearColor{0.0f, 0.0f, 0.0f};

    // Double-buffered frame buffers + a shared depth buffer.
    framebuffer_t m_frame[2];
    zbuffer_t m_z;
    int m_drawBuffer = 0; // framebuffer index the current frame draws into
    int m_displayBuffer = 0; // framebuffer index of the frame kicked but not yet shown

    // Double-buffered geometry packets: the current one (m_geom) is being built
    // while the other may still be DMA'ing to the GS. m_env is a small packet
    // for texture-upload / environment transfers.
    packet2_t* m_geomBuf[GFX_GIFTAG_PACKET_BUFFERS] = {nullptr, nullptr};
    packet2_t* m_geom = nullptr; // == m_geomBuf[m_geomIndex]
    packet2_t* m_env = nullptr;
    uint8_t m_geomIndex = 0; // which geometry packet is current
    bool m_framePending = false; // a kicked frame is still owned by the GS

    // EE transform scratch (sized to the per-frame vertex cap).
    xyz_t* m_xyz = nullptr; // GS fixed-point xyz per emitted vertex
    uint32_t* m_srcIdx = nullptr; // source vertex index per emitted vertex (for UVs)
    float* m_q = nullptr; // 1/clip.w per emitted vertex — GS perspective-correct factor

    // VU0 macro-mode batch transform (math3d calculate_vertices) for the strip
    // path. Enabled only if an init-time self-test confirms VU0 reproduces the
    // scalar clip coords; otherwise the proven scalar path is used. m_vu0Xform
    // buffers hold one batch of clip-space / vec4 input verts (16-byte aligned).
    bool m_useVu0 = false;
    void* m_clipBatch = nullptr; // VECTOR[GFX_GIFTAG_XFORM_BATCH] clip output
    void* m_vecBatch = nullptr; // VECTOR[GFX_GIFTAG_XFORM_BATCH] vec4 input (repack)
    void SelfTestVu0Transform();
    // Fill m_xyz[0..count) / m_q[0..count) with screen-space verts for a strip.
    void TransformStrip(const float mvp[16], const float* verts, int components, uint32_t count);

    // GS-VRAM first-fit free-list for textures. A single heap block is claimed
    // from graph (after the frame/z buffers) and sub-allocated here, so releasing
    // a texture reclaims its VRAM in any order — graph_vram_free is FIFO-only and
    // cannot. One extent per texture (its mips + CLUT are packed contiguously).
    uint32_t m_texHeapBase = 0; // GS word address of the texture heap
    uint32_t m_texHeapWords = 0; // heap size in GS words
    static constexpr int TAG_MAX_VRAM_EXTENTS = 128;
    struct VramExtent
    {
        uint32_t addr; // GS word address
        uint32_t words; // size in GS words
        bool used;
    };
    VramExtent m_vramExtents[TAG_MAX_VRAM_EXTENTS]{};
    int m_vramExtentCount = 0;

    // First-fit allocate `words` (64-word aligned) from the texture heap; 0 on
    // failure. Free returns the extent to the pool and coalesces neighbours.
    uint32_t VramAlloc(uint32_t words);
    void VramFree(uint32_t addr);

    // Texture registry — Texture2D.id is (index + 1); 0 means "invalid".
    static constexpr uint16_t TAG_MAX_TEXTURES = 64;
    struct TexEntry
    {
        bool inUse;
        uint32_t gsAddr; // GS word address of mip level 0
        uint32_t mipAddr[TEX_MAX_MIP_LEVELS]; // GS word address per mip level
        uint8_t mipCount; // 1..TEX_MAX_MIP_LEVELS
        uint32_t clutAddr; // GS word address of the CLUT (0 = none / not PAL8)
        uint32_t vramBase; // heap extent base to free on release
        int width;
        int height;
        int psm; // GS pixel storage mode
    };
    TexEntry m_textures[TAG_MAX_TEXTURES]{};

    static constexpr uint16_t TAG_MAX_2D_RECTS = 4096;
    struct Rect2D
    {
        int32_t x, y, w, h;
        Color3 color;
    };
    Rect2D m_rects2D[TAG_MAX_2D_RECTS];
    uint16_t m_rect2DCount = 0;
    uint16_t m_droppedRects2D = 0;

    uint16_t m_frameVertsUsed = 0;
    uint16_t m_frameDroppedObjects = 0; // objects dropped this frame (budget); logged once/frame

    // EE backface culling for the triangle-list path (primitives + list-topology
    // models). Strips are left uncull ed — the GS Z-rejects their backfaces.
    bool m_backfaceCull = true;

    // Texture id bound into the geometry packet most recently this frame; skips
    // redundant TEX0/TEX1 writes when consecutive draws share a texture (draws
    // are texture-sorted in Render). 0 = nothing bound yet this frame.
    uint32_t m_lastBoundTex = 0;

    // Per-frame throughput counters; snapshotted into DrawLists at EndFrame so
    // the PerfLogger reads a complete frame (Render() resets the draw lists
    // before the packet is dispatched).
    DrawStats m_frameStats{};

    // Model matrix (rotation + scale + translation). View / projection /
    // matrix-multiply come from the shared Frustum helpers so the CPU frustum
    // used for culling matches what the GS rasterizes.
    static void BuildModelMatrix(float out[16], const Vector3& pos, const Vector3& rot, const Vector3& scl);

    // Transform + emit one unindexed triangle list. `components` is the position
    // stride in floats (3 for primitives / legacy models, 4 for baked v2). uvs
    // is 2 floats/vertex or null. mvp is the combined model→clip matrix.
    void DrawTriangles(const float mvp[16], const float* verts, int components, const float* uvs, uint32_t vertexCount, Color3 color, uint32_t textureId);
    // Transform + emit one triangle strip (PRIM type 4). Near-plane rejection
    // splits the strip into maximal runs of visible vertices, one GIF REGLIST
    // per run (the common all-visible case is a single tag).
    void DrawStrip(const float mvp[16], const float* verts, int components, const float* uvs, uint32_t vertexCount, Color3 color, uint32_t textureId);
    void BindTexture(uint32_t textureId);
    void FlushRects2D();

    // True if the current geometry packet has room for `qwNeeded` more qwords
    // (leaving GFX_GIFTAG_PACKET_MARGIN_QW free). Worst-case guard against a
    // DMA overrun before emitting an object.
    bool PacketHasSpace(uint32_t qwNeeded) const;

public:
    GifTagRenderer() = delete;
    explicit GifTagRenderer(const EngineConfig& config);

    GifTagRenderer(const GifTagRenderer&) = delete;
    GifTagRenderer(GifTagRenderer&&) = delete;
    GifTagRenderer& operator=(const GifTagRenderer&) = delete;
    GifTagRenderer& operator=(GifTagRenderer&&) = delete;

    ~GifTagRenderer() override = default;

    RendererType GetRendererType() const override;

    void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale) override;
    void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color) override;
    void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, int32_t textureId) override;
    void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color, int32_t textureId) override;
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
};
