#include "../include/graphics/TagRenderer.h"

// Only compile the GIFTAG renderer body for that backend build.
#ifdef RENDERER_BACKEND_GIFTAG

#include <cmath>
#include <cstring>
#include <malloc.h>

extern "C"
{
#include <dma.h>
#include <gs_psm.h>
#include <packet2_utils.h>
}

#include "../include/graphics/PrimitiveGeometry.h"
#include "EngineDebug.h"
#include "EngineMemory.h"
#include "EngineResource.h"
#include "Macros.h"

namespace
{
constexpr float TAG_DEG_TO_RAD = 0.017453292519943295f;
constexpr float TAG_NEAR = 0.1f;
constexpr float TAG_FAR = 1000.0f;
constexpr float TAG_W_EPS = 0.001f; // near-plane reject threshold on clip.w
constexpr float TAG_Z_MAX = 16777215.0f; // 24-bit usable Z range (GS_ZBUF_32)

// GS register indices (for GIF REGLIST descriptors).
constexpr uint64_t GSREG_RGBAQ = 0x01;
constexpr uint64_t GSREG_ST = 0x02;
constexpr uint64_t GSREG_XYZ2 = 0x05;

// Build a GIFtag low word (see tGifTag in ps2s/gs.h for the field layout).
inline uint64_t GifTagLo(uint32_t nloop, uint32_t prim, uint32_t nreg, bool pre)
{
    return (static_cast<uint64_t>(nloop) & 0x7FFF)
        | (1ull << 15) /* EOP */
        | (pre ? (1ull << 46) : 0ull) /* PRE */
        | (static_cast<uint64_t>(prim) << 47) /* PRIM */
        | (1ull << 58) /* FLG = REGLIST */
        | (static_cast<uint64_t>(nreg & 0xF) << 60);
}

inline uint64_t FloatBits(float f)
{
    uint32_t b;
    std::memcpy(&b, &f, sizeof(b));
    return b;
}
} // namespace

// ---------------------------------------------------------------------------
// Matrix helpers (column-major, OpenGL convention — same as the PS2GL path).
// ---------------------------------------------------------------------------
void TagRenderer::Mult4x4(float out[16], const float a[16], const float b[16])
{
    float tmp[16];
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            tmp[c * 4 + r] = a[0 * 4 + r] * b[c * 4 + 0] + a[1 * 4 + r] * b[c * 4 + 1] + a[2 * 4 + r] * b[c * 4 + 2] + a[3 * 4 + r] * b[c * 4 + 3];
    std::memcpy(out, tmp, sizeof(tmp));
}

void TagRenderer::BuildProjMatrix(float out[16]) const
{
    const float aspect = static_cast<float>(GFX_SCREEN_WIDTH) / static_cast<float>(GFX_SCREEN_HEIGHT);
    const float top = TAG_NEAR * std::tan(45.0f * 0.5f * TAG_DEG_TO_RAD);
    const float right = top * aspect;

    std::memset(out, 0, sizeof(float) * 16);
    out[0] = TAG_NEAR / right;
    out[5] = TAG_NEAR / top;
    out[10] = -(TAG_FAR + TAG_NEAR) / (TAG_FAR - TAG_NEAR);
    out[11] = -1.0f;
    out[14] = -(2.0f * TAG_FAR * TAG_NEAR) / (TAG_FAR - TAG_NEAR);
}

void TagRenderer::BuildViewMatrix(float out[16], const Camera3D& camera) const
{
    auto sub = [](const Vector3& a, const Vector3& b) { return Vector3{a.x - b.x, a.y - b.y, a.z - b.z}; };
    auto cross = [](const Vector3& a, const Vector3& b) {
        return Vector3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    };
    auto dot = [](const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
    auto norm = [](const Vector3& v) {
        const float l2 = v.x * v.x + v.y * v.y + v.z * v.z;
        if (l2 <= 0.0f)
            return Vector3{0.0f, 0.0f, 0.0f};
        const float inv = 1.0f / std::sqrt(l2);
        return Vector3{v.x * inv, v.y * inv, v.z * inv};
    };

    const Vector3 fwd = norm(sub(camera.target, camera.position));
    const Vector3 side = norm(cross(fwd, camera.up));
    const Vector3 up = cross(side, fwd);

    out[0] = side.x; out[4] = side.y; out[8] = side.z; out[12] = -dot(side, camera.position);
    out[1] = up.x; out[5] = up.y; out[9] = up.z; out[13] = -dot(up, camera.position);
    out[2] = -fwd.x; out[6] = -fwd.y; out[10] = -fwd.z; out[14] = dot(fwd, camera.position);
    out[3] = 0.0f; out[7] = 0.0f; out[11] = 0.0f; out[15] = 1.0f;
}

void TagRenderer::BuildModelMatrix(float out[16], const Vector3& pos, const Vector3& rot, const Vector3& scl)
{
    const float rx = rot.x * TAG_DEG_TO_RAD;
    const float ry = rot.y * TAG_DEG_TO_RAD;
    const float rz = rot.z * TAG_DEG_TO_RAD;
    const float cx = std::cos(rx), sx = std::sin(rx);
    const float cy = std::cos(ry), sy = std::sin(ry);
    const float cz = std::cos(rz), sz = std::sin(rz);

    // R = Rz * Ry * Rx (column-major); then apply scale (columns) and translation.
    const float r00 = cy * cz;
    const float r01 = cy * sz;
    const float r02 = -sy;
    const float r10 = sx * sy * cz - cx * sz;
    const float r11 = sx * sy * sz + cx * cz;
    const float r12 = sx * cy;
    const float r20 = cx * sy * cz + sx * sz;
    const float r21 = cx * sy * sz - sx * cz;
    const float r22 = cx * cy;

    out[0] = r00 * scl.x; out[1] = r10 * scl.x; out[2] = r20 * scl.x; out[3] = 0.0f;
    out[4] = r01 * scl.y; out[5] = r11 * scl.y; out[6] = r21 * scl.y; out[7] = 0.0f;
    out[8] = r02 * scl.z; out[9] = r12 * scl.z; out[10] = r22 * scl.z; out[11] = 0.0f;
    out[12] = pos.x; out[13] = pos.y; out[14] = pos.z; out[15] = 1.0f;
}

// ---------------------------------------------------------------------------
// Construction / GS setup
// ---------------------------------------------------------------------------
static void AddPrimitive(DrawLists& lists, Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color, int32_t textureId)
{
    PrimitiveDrawEntry entry;
    entry.transform = Transform3D(position, rotation, scale);
    entry.color = color;
    entry.type = primitive;
    entry.textureId = textureId;
    lists.AddPrimitive(entry);
}

TagRenderer::TagRenderer(const EngineConfig& config)
{
    UNUSED_VAR(config);
    Engine_LogInfo("TagRenderer: Initializing GIFTAG renderer (%dx%d %s)", GFX_SCREEN_WIDTH, GFX_SCREEN_HEIGHT, GFX_SCREEN_REGION_STR);

    const bool pal = (GFX_SCREEN_HEIGHT == GFX_SCREEN_PAL_HEIGHT);

    // --- GS buffers ---
    for (int i = 0; i < 2; ++i)
    {
        m_frame[i].width = GFX_SCREEN_WIDTH;
        m_frame[i].height = GFX_SCREEN_HEIGHT;
        m_frame[i].mask = 0;
        m_frame[i].psm = GS_PSM_32;
        m_frame[i].address = graph_vram_allocate(GFX_SCREEN_WIDTH, GFX_SCREEN_HEIGHT, GS_PSM_32, GRAPH_ALIGN_PAGE);
    }

    m_z.enable = DRAW_ENABLE;
    m_z.method = ZTEST_METHOD_GREATER_EQUAL; // inverted depth: nearer = larger Z
    m_z.zsm = GS_ZBUF_32;
    m_z.mask = 0;
    m_z.address = graph_vram_allocate(GFX_SCREEN_WIDTH, GFX_SCREEN_HEIGHT, GS_ZBUF_32, GRAPH_ALIGN_PAGE);

    // --- video mode + initial display buffer ---
    graph_set_mode(GRAPH_MODE_INTERLACED, pal ? GRAPH_MODE_PAL : GRAPH_MODE_NTSC, GRAPH_MODE_FIELD, GRAPH_DISABLE);
    graph_set_framebuffer_filtered(m_frame[0].address, m_frame[0].width, m_frame[0].psm, 0, 0);
    graph_enable_output();

    // --- DMA + packets ---
    dma_channel_initialize(DMA_CHANNEL_GIF, nullptr, 0);
    dma_channel_fast_waits(DMA_CHANNEL_GIF);

    // P2_MODE_NORMAL: both packets are flat GIFtag+data content (no embedded
    // DMA chain tags), sent as a single contiguous transfer to the GIF channel.
    // P2_MODE_CHAIN would require each block to start with a dma_tag_t, which
    // we do not write — using it here would make the DMAC misparse our GIFtags
    // as chain tags.
    m_geom = packet2_create(GFX_GIFTAG_PACKET_QWORDS, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0);
    m_env = packet2_create(64, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0);
    if (!m_geom || !m_env)
    {
        Engine_LogError("TagRenderer: failed to create packets.");
        return;
    }

    // --- transform scratch ---
    m_xyz = static_cast<xyz_t*>(memalign(16, sizeof(xyz_t) * GFX_GIFTAG_MAX_VERTS));
    m_srcIdx = static_cast<uint32_t*>(memalign(16, sizeof(uint32_t) * GFX_GIFTAG_MAX_VERTS));
    m_q = static_cast<float*>(memalign(16, sizeof(float) * GFX_GIFTAG_MAX_VERTS));
    if (!m_xyz || !m_srcIdx || !m_q)
    {
        Engine_Panic("TagRenderer: out of memory for transform scratch");
        return;
    }

    // Separated primitive geometry lives in the renderer arena; reserve it, then
    // let DrawLists de-interleave the MODEL_* tables into it.
    float* megaBatch = static_cast<float*>(Engine_GetSlot(ARENA_RENDERER, 0));
    if (!megaBatch)
    {
        Engine_Panic("TagRenderer: Failed to retrieve ARENA_RENDERER slot 0!");
        return;
    }
    const size_t bytes = (PRIMITIVE_CUBE_VERTEX_COUNT + PRIMITIVE_SPHERE_VERTEX_COUNT + PRIMITIVE_CYLINDER_VERTEX_COUNT) * (3 + 3 + 2) * sizeof(float);
    Engine_LoadToSlot(ARENA_RENDERER, 0, nullptr, bytes);
    m_drawLists.Init(megaBatch);

    m_initialized = true;
}

void TagRenderer::Shutdown()
{
    if (m_geom)
        packet2_free(m_geom);
    if (m_env)
        packet2_free(m_env);
    m_geom = m_env = nullptr;
    free(m_xyz);
    free(m_srcIdx);
    free(m_q);
    m_xyz = nullptr;
    m_srcIdx = nullptr;
    m_q = nullptr;
    m_drawLists.Shutdown();
    graph_shutdown();
    m_initialized = false;
}

RendererType TagRenderer::GetRendererType() const { return RendererType::Tag; }

// ---------------------------------------------------------------------------
// Draw-list submission (identical bookkeeping to the PS2GL renderer)
// ---------------------------------------------------------------------------
void TagRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale)
{
    AddPrimitive(m_drawLists, primitive, position, rotation, scale, {1.0f, 1.0f, 1.0f}, -1);
}
void TagRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color)
{
    AddPrimitive(m_drawLists, primitive, position, rotation, scale, color, -1);
}
void TagRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, int32_t textureId)
{
    AddPrimitive(m_drawLists, primitive, position, rotation, scale, {1.0f, 1.0f, 1.0f}, textureId);
}
void TagRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color, int32_t textureId)
{
    AddPrimitive(m_drawLists, primitive, position, rotation, scale, color, textureId);
}
void TagRenderer::AddUIToDrawList(const UI& ui, const Vector2& offset, const Vector2& scale)
{
    UIDrawEntry entry{};
    entry.ui = ui;
    entry.offset = offset;
    entry.scale = scale.x;
    m_drawLists.AddUIDraw(entry);
}
void TagRenderer::AddLevelToDrawList(const Level& level) { UNUSED_VAR(level); }
void TagRenderer::AddModelToDrawList(int32_t modelId, const Vector3& position, const Vector3& rotation, const Vector3& scale)
{
    ModelDrawEntry entry;
    entry.resourceId = modelId;
    entry.transform = Transform3D(position, rotation, scale);
    m_drawLists.AddModel(entry);
}
void TagRenderer::AddSkyToDrawList(int32_t resourceId) { m_drawLists.SetSkyboxTexture(resourceId); }
void TagRenderer::ClearDrawLists() { m_drawLists.Reset(false); }

// ---------------------------------------------------------------------------
// Frame lifecycle
// ---------------------------------------------------------------------------
void TagRenderer::BeginFrame()
{
    m_inFrame = true;
    m_frameVertsUsed = 0;
    m_framePrimCount = 0;
    m_frameModelMeshCount = 0;

    packet2_reset(m_geom, 0);

    // Draw environment for the current back buffer + clear.
    packet2_update(m_geom, draw_setup_environment(m_geom->next, 0, &m_frame[m_drawBuffer], &m_z));
    packet2_update(m_geom, draw_primitive_xyoffset(m_geom->next, 0, 2048 - (GFX_SCREEN_WIDTH / 2), 2048 - (GFX_SCREEN_HEIGHT / 2)));

    const int cr = static_cast<int>(m_clearColor.r * 255.0f);
    const int cg = static_cast<int>(m_clearColor.g * 255.0f);
    const int cb = static_cast<int>(m_clearColor.b * 255.0f);
    packet2_update(m_geom, draw_clear(m_geom->next, 0,
                                      2048.0f - (GFX_SCREEN_WIDTH / 2), 2048.0f - (GFX_SCREEN_HEIGHT / 2),
                                      GFX_SCREEN_WIDTH, GFX_SCREEN_HEIGHT, cr, cg, cb));
}

void TagRenderer::EndFrame()
{
    FlushRects2D();

    // Finish + dispatch this frame's chain.
    packet2_update(m_geom, draw_finish(m_geom->next));
    dma_channel_send_packet2(m_geom, DMA_CHANNEL_GIF, 1);
    dma_channel_wait(DMA_CHANNEL_GIF, 0);
    draw_wait_finish();

    graph_wait_vsync();
    // Display the buffer we just drew; draw into the other next frame.
    graph_set_framebuffer_filtered(m_frame[m_drawBuffer].address, m_frame[m_drawBuffer].width, m_frame[m_drawBuffer].psm, 0, 0);
    m_drawBuffer ^= 1;

    m_inFrame = false;
}

void TagRenderer::DrawDebugOverlay() {}

void TagRenderer::ClearFrame(const Color3& color)
{
    // Stashed for BeginFrame() (which emits the actual GS clear). Unlike the
    // PS2GL path there is no in-frame immediate clear — the panic loop's clear
    // is picked up by the next BeginFrame().
    m_clearColor = color;
}

void TagRenderer::DrawRect2D(int32_t x, int32_t y, int32_t width, int32_t height, const Color3& color)
{
    if (m_rect2DCount >= TAG_MAX_2D_RECTS)
    {
        Engine_LogError("TagRenderer: 2D rect queue full (%u).", TAG_MAX_2D_RECTS);
        return;
    }
    m_rects2D[m_rect2DCount++] = Rect2D{x, y, width, height, color};
}

void TagRenderer::FlushRects2D()
{
    if (m_rect2DCount == 0)
        return;

    // Sprite (untextured) primitives in screen space. XYOFFSET is already set to
    // (2048 - W/2, 2048 - H/2), so screen pixels map through 12.4 window coords.
    for (uint16_t i = 0; i < m_rect2DCount; ++i)
    {
        const Rect2D& r = m_rects2D[i];
        const uint16_t x0 = static_cast<uint16_t>(r.x * 16);
        const uint16_t y0 = static_cast<uint16_t>(r.y * 16);
        const uint16_t x1 = static_cast<uint16_t>((r.x + r.w) * 16);
        const uint16_t y1 = static_cast<uint16_t>((r.y + r.h) * 16);

        const uint8_t cr = static_cast<uint8_t>(r.color.r * 255.0f);
        const uint8_t cg = static_cast<uint8_t>(r.color.g * 255.0f);
        const uint8_t cb = static_cast<uint8_t>(r.color.b * 255.0f);
        const uint64_t rgbaq = static_cast<uint64_t>(cr) | (static_cast<uint64_t>(cg) << 8) | (static_cast<uint64_t>(cb) << 16) | (static_cast<uint64_t>(0x80) << 24) | (FloatBits(1.0f) << 32);

        // PRIM = sprite (6), gouraud off, no texture. NLOOP=2 (two vertices).
        const uint32_t prim = 6u;
        packet2_add_u64(m_geom, GifTagLo(2, prim, 2, true));
        packet2_add_u64(m_geom, GSREG_RGBAQ | (GSREG_XYZ2 << 4));
        packet2_add_u64(m_geom, rgbaq);
        packet2_add_u64(m_geom, static_cast<uint64_t>(x0) | (static_cast<uint64_t>(y0) << 16));
        packet2_add_u64(m_geom, rgbaq);
        packet2_add_u64(m_geom, static_cast<uint64_t>(x1) | (static_cast<uint64_t>(y1) << 16));
    }

    m_rect2DCount = 0;
}

void TagRenderer::DrawGrid(int32_t slices, float spacing)
{
    UNUSED_VAR(slices);
    UNUSED_VAR(spacing);
}

void TagRenderer::Render()
{
    float view[16], proj[16], vp[16];
    BuildViewMatrix(view, m_drawLists.GetCamera3D());
    BuildProjMatrix(proj);
    Mult4x4(vp, proj, view); // clip = P * V * (M * v)

    RenderSkybox(m_drawLists);

    // Store vp so Render* helpers can compose with each object's model matrix.
    // (Passed explicitly via the DrawTriangles path below.)
    // Primitives:
    {
        DrawLists& lists = m_drawLists;
        const uint16_t uCount = lists.GetUntexturedCount();
        const uint16_t tCount = lists.GetTexturedCount();
        const PrimitiveDrawEntry* uPrims = lists.GetUntexturedPrims();
        const PrimitiveDrawEntry* tPrims = lists.GetTexturedPrims();

        auto drawPrim = [&](const PrimitiveDrawEntry& e, uint32_t texId) {
            const PrimitiveArrays arr = lists.GetPrimitiveArrays(e.type);
            float model[16], mvp[16];
            BuildModelMatrix(model, e.transform.GetPosition(), e.transform.GetRotation(), e.transform.GetScale());
            Mult4x4(mvp, vp, model);
            DrawTriangles(mvp, arr.verts, texId ? arr.uvs : nullptr, arr.vertexCount, e.color, texId);
            ++m_framePrimCount;
        };

        for (uint16_t i = 0; i < uCount; ++i)
            drawPrim(uPrims[i], 0);

        for (uint16_t i = 0; i < tCount; ++i)
        {
            uint32_t texId = 0;
            const auto* tex = static_cast<const Texture2D*>(Engine_Resource_Get(tPrims[i].textureId));
            if (tex)
                texId = tex->id;
            drawPrim(tPrims[i], texId);
        }
    }

    RenderModels(m_drawLists);

    DrawStats stats{};
    stats.primitiveCount = m_framePrimCount;
    stats.modelCount = m_frameModelMeshCount;
    m_drawLists.SetLastStats(stats);

    RenderUI(m_drawLists);
    m_drawLists.Reset(false);
}

// Transform + emit one unindexed triangle list.
void TagRenderer::DrawTriangles(const float mvp[16], const float* verts, const float* uvs,
                                uint32_t vertexCount, Color3 color, uint32_t textureId)
{
    if (!verts || vertexCount < 3)
        return;

    if (static_cast<uint32_t>(m_frameVertsUsed) + vertexCount > static_cast<uint32_t>(GFX_GIFTAG_MAX_VERTS))
    {
        Engine_LogError("TagRenderer: vertex budget exceeded (%u + %u > %u); dropping object.",
            m_frameVertsUsed, vertexCount, static_cast<unsigned>(GFX_GIFTAG_MAX_VERTS));
        return;
    }

    const bool textured = (uvs != nullptr && textureId != 0);
    uint32_t emitted = 0;

    for (uint32_t tri = 0; tri + 2 < vertexCount; tri += 3)
    {
        float cw[3];
        float nx[3], ny[3], nz[3];
        bool ok = true;
        for (int j = 0; j < 3; ++j)
        {
            const float* v = verts + (tri + j) * 3;
            const float x = v[0], y = v[1], z = v[2];
            const float clipX = mvp[0] * x + mvp[4] * y + mvp[8] * z + mvp[12];
            const float clipY = mvp[1] * x + mvp[5] * y + mvp[9] * z + mvp[13];
            const float clipZ = mvp[2] * x + mvp[6] * y + mvp[10] * z + mvp[14];
            const float clipW = mvp[3] * x + mvp[7] * y + mvp[11] * z + mvp[15];
            if (clipW <= TAG_W_EPS)
                ok = false;
            cw[j] = clipW;
            nx[j] = clipX;
            ny[j] = clipY;
            nz[j] = clipZ;
        }
        if (!ok)
            continue; // crude whole-triangle near-plane cull (no clipping yet)

        for (int j = 0; j < 3; ++j)
        {
            const float inv = 1.0f / cw[j];
            const float ndcX = nx[j] * inv;
            const float ndcY = ny[j] * inv;
            const float ndcZ = nz[j] * inv;
            const float sx = (ndcX * 0.5f + 0.5f) * GFX_SCREEN_WIDTH;
            const float sy = (1.0f - (ndcY * 0.5f + 0.5f)) * GFX_SCREEN_HEIGHT;
            const float zc = ndcZ * 0.5f + 0.5f; // 0 near .. 1 far
            m_xyz[emitted].x = static_cast<uint16_t>(sx * 16.0f);
            m_xyz[emitted].y = static_cast<uint16_t>(sy * 16.0f);
            m_xyz[emitted].z = static_cast<uint32_t>((1.0f - zc) * TAG_Z_MAX); // invert for GEQUAL
            m_srcIdx[emitted] = tri + j;
            m_q[emitted] = inv; // 1/clip.w — reused for perspective-correct ST below
            ++emitted;
        }
    }

    if (emitted == 0)
        return;

    m_frameVertsUsed += emitted;

    if (textured)
        BindTexture(textureId);

    const uint8_t r = static_cast<uint8_t>(color.r * 255.0f);
    const uint8_t g = static_cast<uint8_t>(color.g * 255.0f);
    const uint8_t b = static_cast<uint8_t>(color.b * 255.0f);
    // RGB+A packed once; Q (bits 32-63) varies per vertex below.
    const uint64_t rgbaLo = static_cast<uint64_t>(r) | (static_cast<uint64_t>(g) << 8) | (static_cast<uint64_t>(b) << 16) | (static_cast<uint64_t>(0x80) << 24);

    // PRIM: triangle(3), gouraud(IIP bit3), texture(TME bit4 if textured).
    const uint32_t prim = 3u | (1u << 3) | (textured ? (1u << 4) : 0u);
    const uint32_t nreg = textured ? 3u : 2u;
    const uint64_t reglist = textured ? (GSREG_ST | (GSREG_RGBAQ << 4) | (GSREG_XYZ2 << 8))
                                      : (GSREG_RGBAQ | (GSREG_XYZ2 << 4));

    packet2_add_u64(m_geom, GifTagLo(emitted, prim, nreg, true));
    packet2_add_u64(m_geom, reglist);

    for (uint32_t k = 0; k < emitted; ++k)
    {
        const float q = m_q[k];
        if (textured)
        {
            // GS ST mode expects S=u/w, T=v/w (i.e. pre-divided by the same Q used
            // for the RGBAQ below); the rasterizer multiplies back by 1/Q per pixel
            // to recover perspective-correct texture coordinates.
            const float* uv = uvs + m_srcIdx[k] * 2;
            packet2_add_u64(m_geom, FloatBits(uv[0] * q) | (FloatBits(uv[1] * q) << 32));
        }
        packet2_add_u64(m_geom, rgbaLo | (FloatBits(q) << 32));
        uint64_t xyzWord;
        std::memcpy(&xyzWord, &m_xyz[k], sizeof(xyzWord));
        packet2_add_u64(m_geom, xyzWord);
    }

    // REGLIST data must end on a qword boundary.
    if (((emitted * nreg) & 1u) != 0u)
        packet2_add_u64(m_geom, 0);
}

void TagRenderer::BindTexture(uint32_t textureId)
{
    if (textureId == 0 || textureId > TAG_MAX_TEXTURES)
        return;
    const TexEntry& t = m_textures[textureId - 1];
    if (!t.inUse)
        return;

    texbuffer_t tb;
    tb.address = t.gsAddr;
    tb.width = (t.width < 64) ? 64 : t.width; // TBW min 64
    tb.psm = t.psm;
    tb.info.width = draw_log2(t.width);
    tb.info.height = draw_log2(t.height);
    tb.info.components = TEXTURE_COMPONENTS_RGBA;
    tb.info.function = TEXTURE_FUNCTION_MODULATE;

    lod_t lod;
    lod.calculation = LOD_USE_K; // fixed level (no mipmaps baked yet)
    lod.max_level = 0;
    lod.mag_filter = LOD_MAG_LINEAR;
    lod.min_filter = LOD_MIN_LINEAR;
    lod.mipmap_select = LOD_MIPMAP_REGISTER;
    lod.l = 0;
    lod.k = 0.0f;

    packet2_update(m_geom, draw_texture_sampling(m_geom->next, 0, &lod));
    packet2_update(m_geom, draw_texturebuffer(m_geom->next, 0, &tb, nullptr));
}

void TagRenderer::RenderSkybox(const DrawLists& lists)
{
    if (lists.GetSkyboxResourceId() == -1)
        return;

    const auto* tex = static_cast<const Texture2D*>(Engine_Resource_Get(lists.GetSkyboxResourceId()));
    if (!tex || tex->id == 0)
        return;

    // Camera-centered cube. Uses the shared cube geometry; drawn first so the
    // scene overwrites it. (Depth handling for the skybox is a verification TODO.)
    const Camera3D& camera = lists.GetCamera3D();
    float view[16], proj[16], vp[16], model[16], mvp[16];
    BuildViewMatrix(view, camera);
    BuildProjMatrix(proj);
    Mult4x4(vp, proj, view);
    BuildModelMatrix(model, camera.position, Vector3{0, 0, 0}, Vector3{500.0f, 500.0f, 500.0f});
    Mult4x4(mvp, vp, model);

    const PrimitiveArrays cube = lists.GetPrimitiveArrays(Primitive3D::Cube);
    DrawTriangles(mvp, cube.verts, cube.uvs, cube.vertexCount, Color3{1.0f, 1.0f, 1.0f}, tex->id);
}

void TagRenderer::RenderPrimitives(DrawLists& lists)
{
    // Primitive submission is inlined in Render() (it needs the view-projection
    // matrix already composed there); nothing to do here.
    UNUSED_VAR(lists);
}

void TagRenderer::RenderModels(const DrawLists& lists)
{
    const uint16_t mCount = lists.GetModelCount();
    if (mCount == 0)
        return;

    float view[16], proj[16], vp[16];
    BuildViewMatrix(view, lists.GetCamera3D());
    BuildProjMatrix(proj);
    Mult4x4(vp, proj, view);

    const ModelDrawEntry* entries = lists.GetModels();
    uint16_t meshDraws = 0;

    for (uint16_t i = 0; i < mCount; ++i)
    {
        const ModelDrawEntry& entry = entries[i];
        const auto* model = static_cast<const Model*>(Engine_Resource_Get(entry.resourceId));
        if (!model)
            continue;

        float mm[16], mvp[16];
        BuildModelMatrix(mm, entry.transform.GetPosition(), entry.transform.GetRotation(), entry.transform.GetScale());
        Mult4x4(mvp, vp, mm);

        for (int meshIdx = 0; meshIdx < model->meshCount; ++meshIdx)
        {
            const Mesh& mesh = model->meshes[meshIdx];
            if (mesh.indices != nullptr || !mesh.vertices || mesh.vertexCount == 0)
                continue;

            uint32_t texId = 0;
            const int matIdx = (model->meshMaterial) ? model->meshMaterial[meshIdx] : 0;
            if (model->materials)
            {
                // Resolve the diffuse texture from its resource handle at draw time.
                const int32_t texResId = model->materials[matIdx].maps[MATERIAL_MAP_DIFFUSE].textureResourceId;
                if (texResId >= 0)
                {
                    const auto* t = static_cast<const Texture2D*>(Engine_Resource_Get(texResId));
                    if (t)
                        texId = t->id;
                }
            }

            DrawTriangles(mvp, mesh.vertices, texId ? mesh.texcoords : nullptr,
                          static_cast<uint32_t>(mesh.vertexCount), Color3{1.0f, 1.0f, 1.0f}, texId);
            ++meshDraws;
        }
    }

    m_frameModelMeshCount = meshDraws;
}

void TagRenderer::RenderUI(const DrawLists& lists) { UNUSED_VAR(lists); }

// ---------------------------------------------------------------------------
// Textures — upload to GS VRAM and bind. VRAM is bump-allocated via graph; per
// texture release does not reclaim VRAM in this first cut (verification TODO).
// ---------------------------------------------------------------------------
uint32_t TagRenderer::UploadTexture(const void* pixels, int width, int height, PixelFormat format)
{
    int slot = -1;
    for (int i = 0; i < TAG_MAX_TEXTURES; ++i)
    {
        if (!m_textures[i].inUse)
        {
            slot = i;
            break;
        }
    }
    if (slot < 0)
    {
        Engine_LogError("TagRenderer: texture registry full (%u).", TAG_MAX_TEXTURES);
        return 0;
    }

    const int psm = (format == PixelFormat::RGBA16) ? GS_PSM_16 : GS_PSM_32;
    const uint32_t gsAddr = graph_vram_allocate(width, height, psm, GRAPH_ALIGN_BLOCK);
    if (gsAddr == 0u || gsAddr == static_cast<uint32_t>(-1))
    {
        Engine_LogError("TagRenderer: GS VRAM alloc failed for %dx%d texture.", width, height);
        return 0;
    }

    // Upload the pixels via a one-shot image transfer.
    packet2_reset(m_env, 0);
    packet2_update(m_env, draw_texture_transfer(m_env->next, const_cast<void*>(pixels), width, height, psm, gsAddr, width));
    packet2_update(m_env, draw_texture_flush(m_env->next));
    dma_channel_send_packet2(m_env, DMA_CHANNEL_GIF, 1);
    dma_channel_wait(DMA_CHANNEL_GIF, 0);

    m_textures[slot].inUse = true;
    m_textures[slot].gsAddr = gsAddr;
    m_textures[slot].width = width;
    m_textures[slot].height = height;
    m_textures[slot].psm = psm;

    return static_cast<uint32_t>(slot + 1);
}

void TagRenderer::ReleaseTexture(uint32_t handle)
{
    if (handle == 0 || handle > TAG_MAX_TEXTURES)
        return;
    // NOTE: GS VRAM is not individually reclaimed here (bump allocator). The
    // slot is freed for reuse; VRAM is recovered on Shutdown/graph re-init.
    m_textures[handle - 1].inUse = false;
}

void TagRenderer::SetCamera3D(CameraID id, const Camera3D& camera) { m_drawLists.SetCamera3D(id, camera); }
void TagRenderer::SetActiveCamera3D(CameraID id) { m_drawLists.SetActiveCamera3D(id); }
void TagRenderer::SetActiveCamera2D(const Camera2D& camera) { m_drawLists.SetActiveCamera2D(camera); }

bool TagRenderer::IsInitialized() const { return m_initialized; }
DrawStats TagRenderer::GetLastStats() const { return m_drawLists.GetLastStats(); }
Camera3D TagRenderer::GetActiveCamera3D() const { return m_drawLists.GetCamera3D(); }

#endif // RENDERER_BACKEND_GIFTAG
