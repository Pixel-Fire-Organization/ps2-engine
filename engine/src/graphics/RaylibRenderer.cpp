#include "../include/graphics/RaylibRenderer.h"
#include <cstring>
#include <raylib.h>
#include <rlgl.h>

#include <GL/gl.h>

#include "../include/graphics/DrawList.h"
#include "../include/graphics/PrimitiveGeometry.h"
#include "EngineApp.h"
#include "EngineDebug.h"
#include "EngineMemory.h"
#include "EngineResource.h"
#include "Macros.h"


RaylibRenderer::RaylibRenderer(const EngineConfig& config)
{
    Engine_LogInfo("Video Mode: %dx%d (%s)", GFX_SCREEN_WIDTH, GFX_SCREEN_HEIGHT, GFX_SCREEN_REGION_STR);

    InitWindow(GFX_SCREEN_WIDTH, GFX_SCREEN_HEIGHT, config.windowTitle);
    Engine_LogInfo("Waiting for window ready!");
    auto windowReadyBase = GetTime();
    while (!IsWindowReady())
        ;
    m_initialized = IsWindowReady();
    Engine_LogInfo("Window Ready in %d ms", GetTime() - windowReadyBase);

    // Retrieve the renderer arena slot used for separated geometry arrays.
    m_megaBatch = static_cast<float*>(Engine_GetSlot(ARENA_RENDERER, 0));
    if (!m_megaBatch)
    {
        Engine_Panic("RaylibRenderer: Failed to retrieve ARENA_RENDERER slot 0!");
    }

    // Compute the actual bytes needed for separated geometry arrays
    // (cube + sphere + cylinder; each: verts 3f + norms 3f + uvs 2f per vertex).
    const size_t cubeBytes = PRIMITIVE_CUBE_VERTEX_COUNT * (3 + 3 + 2) * sizeof(float);
    const size_t sphereBytes = PRIMITIVE_SPHERE_VERTEX_COUNT * (3 + 3 + 2) * sizeof(float);
    const size_t cylBytes = PRIMITIVE_CYLINDER_VERTEX_COUNT * (3 + 3 + 2) * sizeof(float);
    const size_t totalBytes = cubeBytes + sphereBytes + cylBytes;

    // Register the slot usage in memory stats.
    Engine_LoadToSlot(ARENA_RENDERER, 0, nullptr, totalBytes);
    Engine_LogInfo("RaylibRenderer: Separated geometry arrays: %zu bytes in ARENA_RENDERER", totalBytes);

    // Build separated arrays and compile ps2gl display lists.
    m_drawLists.Init(m_megaBatch);
}

void RaylibRenderer::Shutdown()
{
    m_drawLists.Shutdown();
    CloseWindow();
    m_initialized = false;
}

static void AddPrimitive(DrawLists& lists, Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color, int32_t textureId)
{
    PrimitiveDrawEntry entry;
    entry.transform = Transform3D(position, rotation, scale);
    entry.color = color;
    entry.type = primitive;
    entry.textureId = textureId;
    lists.AddPrimitive(entry);
}

void RaylibRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale)
{
    AddPrimitive(m_drawLists, primitive, position, rotation, scale, {1.f, 1.f, 1.f}, -1);
}
void RaylibRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color)
{
    AddPrimitive(m_drawLists, primitive, position, rotation, scale, color, -1);
}
void RaylibRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, int32_t textureId)
{
    AddPrimitive(m_drawLists, primitive, position, rotation, scale, {1.f, 1.f, 1.f}, textureId);
}
void RaylibRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color, int32_t textureId)
{
    AddPrimitive(m_drawLists, primitive, position, rotation, scale, color, textureId);
}
void RaylibRenderer::AddUIToDrawList(const UI& ui, const Vector2& offset, const Vector2& scale)
{
    UIDrawEntry entry{};
    entry.ui = ui;
    entry.offset = offset;
    entry.scale = scale.x; // Simplified
    m_drawLists.AddUIDraw(entry);
}
void RaylibRenderer::AddLevelToDrawList(const Level& level) { UNUSED_VAR(level); }
void RaylibRenderer::AddModelToDrawList(int32_t modelId, const Vector3& position, const Vector3& rotation, const Vector3& scale)
{
    ModelDrawEntry entry;
    entry.resourceId = modelId;
    entry.transform = Transform3D(position, rotation, scale);
    m_drawLists.AddModel(entry);
}
void RaylibRenderer::AddSkyToDrawList(int32_t resourceId) { m_drawLists.SetSkyboxTexture(resourceId); }

void RaylibRenderer::ClearDrawLists() { m_drawLists.Reset(false); }

void RaylibRenderer::Render()
{
    RenderSkybox(m_drawLists);

    BeginMode3D(m_drawLists.GetCamera3D());
    {
        rlDrawRenderBatchActive();
        rlEnableBackfaceCulling();
        rlSetCullFace(RL_CULL_FACE_FRONT);
        rlEnableDepthTest();
        rlEnableDepthMask();

        RenderPrimitives(m_drawLists);
        RenderModels(m_drawLists);

        rlDisableTexture();
        rlDrawRenderBatchActive();
        rlDisableBackfaceCulling();
        rlDisableDepthTest();
        rlDisableDepthMask();
        rlSetCullFace(RL_CULL_FACE_BACK);
    }
    EndMode3D();

    BeginMode2D(m_drawLists.GetCamera2D());
    RenderUI(m_drawLists);
    EndMode2D();

    m_drawLists.Reset(false);
}

void RaylibRenderer::RenderSkybox(const DrawLists& lists)
{
    if (lists.GetSkyboxResourceId() == -1)
        return;

    const auto* tex = static_cast<const Texture2D*>(Engine_Resource_Get(lists.GetSkyboxResourceId()));
    if (!tex || tex->id == 0)
        return;

    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlEnableTexture(tex->id);

    const Camera3D& camera = lists.GetCamera3D();
    rlPushMatrix();
    rlTranslatef(camera.position.x, camera.position.y, camera.position.z);
    rlScalef(500.f, 500.f, 500.f);

    rlBegin(RL_TRIANGLES);
    rlColor4ub(255, 255, 255, 255);
    for (uint32_t i = 0; i < PRIMITIVE_CUBE_VERTEX_COUNT; ++i)
    {
        const float* v = MODEL_CUBE + i * PRIMITIVE_VERTEX_STRIDE;
        rlTexCoord2f(v[6], v[7]);
        rlNormal3f(v[3], v[4], v[5]);
        rlVertex3f(v[0], v[1], v[2]);
    }
    rlEnd();

    rlPopMatrix();

    rlDisableTexture();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

void RaylibRenderer::RenderPrimitives(DrawLists& lists)
{
    const uint16_t uCount = lists.GetUntexturedCount();
    const uint16_t tCount = lists.GetTexturedCount();

    if (uCount == 0 && tCount == 0)
        return;

    // -----------------------------------------------------------------------
    // Optimisation strategy (PS2-specific):
    //
    // Instead of CPU-transforming all vertices into world space and calling
    // rlBegin/rlEnd for each primitive, we:
    //   1. Push the per-primitive TRS onto ps2gl's modelview matrix stack.
    //   2. Call glCallList — the VU1 handles the transform in hardware.
    //   3. Pop the matrix to restore the camera view.
    //
    // Each display list was compiled at startup with glDrawArrays pointing at
    // the stride-0 vertex/normal/UV arrays extracted from MODEL_*.  On the
    // first glCallList the VIF1 DMA packet is built and cached; subsequent
    // calls just DMA the cached packet, costing near-zero EE cycles.
    //
    // Per-primitive colour is forwarded through glColor4f, which sets
    // ps2gl's current colour used as vertex colour when no GL_COLOR_ARRAY
    // is active.
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    // VIF1 DMA budget enforcement (PS2-specific):
    //
    // ps2gl's main frame DMA packet (CurPacket) is fixed at
    // GFX_PGL_MAIN_PACKET_QWORDS = 65,000 qwords (kDmaPacketMaxQwordLength).
    // Every glCallList writes ~GFX_QWORDS_PER_DRAWCALL qwords into it:
    //   - AddVu1RendererContext: 77 qwords VU1 context (matrix + lights)
    //   - DMA/VIF overhead headers: ~3 qwords
    //   - DMA CALL tag to geometry packet: ~2 qwords
    // When the packet overflows in a release build, mErrorIf is a no-op and
    // the packet writes past its end, corrupting heap memory.  The result is
    // "Vif1: Unknown VifCmd!" followed by TLB misses and a crash to pc=0x0.
    // GFX_DRAW_CALL_BUDGET = 640 keeps total qword usage ≤ 52,480, leaving
    // ~12,520 qwords for rlgl, DrawGrid, UI, and other rendering overhead.
    // -----------------------------------------------------------------------
    const uint16_t totalPrims = uCount + tCount;
    if (totalPrims > GFX_DRAW_CALL_BUDGET)
    {
        Engine_LogError("RenderPrimitives: draw call budget exceeded (%u > %u). "
                        "Excess primitives dropped to prevent VIF1 DMA overflow crash. "
                        "Reduce primitive counts to stay within GFX_DRAW_CALL_BUDGET.",
                        totalPrims, (uint16_t)GFX_DRAW_CALL_BUDGET);
    }

    // Distribute budget: fill untextured first, then textured with remainder.
    const uint16_t uRender = (uCount <= GFX_DRAW_CALL_BUDGET) ? uCount : (uint16_t)GFX_DRAW_CALL_BUDGET;
    const uint16_t tBudget = (uRender < GFX_DRAW_CALL_BUDGET) ? (uint16_t)(GFX_DRAW_CALL_BUDGET - uRender) : 0;
    const uint16_t tRender = (tCount <= tBudget) ? tCount : tBudget;

    // 1. Untextured primitives
    if (uRender > 0)
    {
        rlSetTexture(rlGetTextureIdDefault());
        const PrimitiveDrawEntry* prims = lists.GetUntexturedPrims();

        for (uint16_t i = 0; i < uRender; ++i)
        {
            const auto& entry = prims[i];
            const Vector3 pos = entry.transform.GetPosition();
            const Vector3 rot = entry.transform.GetRotation();
            const Vector3 scl = entry.transform.GetScale();

            glColor4f(entry.color.r, entry.color.g, entry.color.b, 1.f);

            glPushMatrix();
            glTranslatef(pos.x, pos.y, pos.z);
            if (rot.x != 0.f)
                glRotatef(rot.x, 1.f, 0.f, 0.f);
            if (rot.y != 0.f)
                glRotatef(rot.y, 0.f, 1.f, 0.f);
            if (rot.z != 0.f)
                glRotatef(rot.z, 0.f, 0.f, 1.f);
            glScalef(scl.x, scl.y, scl.z);
            glCallList(lists.GetListForType(entry.type));
            glPopMatrix();
        }
    }

    // 2. Textured primitives (batched by texture to minimise state changes)
    if (tRender > 0)
    {
        int32_t lastTexId = -2;
        const PrimitiveDrawEntry* prims = lists.GetTexturedPrims();

        for (uint16_t i = 0; i < tRender; ++i)
        {
            const auto& entry = prims[i];

            if (entry.textureId != lastTexId)
            {
                const auto* tex = static_cast<const Texture2D*>(Engine_Resource_Get(entry.textureId));
                if (tex && tex->id != 0)
                    rlEnableTexture(tex->id);
                else
                {
                    Engine_LogError("Tried to access texture ID (%i), but it wasn't initialized!", entry.textureId);
                    rlSetTexture(rlGetTextureIdDefault());
                }
                lastTexId = entry.textureId;
            }

            const Vector3 pos = entry.transform.GetPosition();
            const Vector3 rot = entry.transform.GetRotation();
            const Vector3 scl = entry.transform.GetScale();

            glColor4f(1.f, 1.f, 1.f, 1.f);

            glPushMatrix();
            glTranslatef(pos.x, pos.y, pos.z);
            if (rot.x != 0.f)
                glRotatef(rot.x, 1.f, 0.f, 0.f);
            if (rot.y != 0.f)
                glRotatef(rot.y, 0.f, 1.f, 0.f);
            if (rot.z != 0.f)
                glRotatef(rot.z, 0.f, 0.f, 1.f);
            glScalef(scl.x, scl.y, scl.z);
            glCallList(lists.GetListForType(entry.type));
            glPopMatrix();
        }
    }

    DrawStats stats{};
    stats.primitiveCount = uRender + tRender;
    lists.SetLastStats(stats);
}

void RaylibRenderer::RenderModels(const DrawLists& lists) { UNUSED_VAR(lists); }
void RaylibRenderer::RenderUI(const DrawLists& lists) { UNUSED_VAR(lists); }

CameraID RaylibRenderer::AddCamera() { return -1; }

void RaylibRenderer::SetActiveCamera3D(CameraID id, const Camera3D& camera)
{
    UNUSED_VAR(id);
    m_drawLists.SetActiveCamera3D(camera);
}

void RaylibRenderer::SetActiveCamera2D(CameraID id, const Camera2D& camera)
{
    UNUSED_VAR(id);
    m_drawLists.SetActiveCamera2D(camera);
}

void RaylibRenderer::SetCameraState(CameraID id, bool enabled, const Vector2& pos, const Vector2& target)
{
    UNUSED_VAR(id);
    UNUSED_VAR(enabled);
    UNUSED_VAR(pos);
    UNUSED_VAR(target);
}

void RaylibRenderer::ResetCameraState(CameraID id) { UNUSED_VAR(id); }

bool RaylibRenderer::IsInitialized() const { return m_initialized; }

DrawStats RaylibRenderer::GetLastStats() const { return m_drawLists.GetLastStats(); }
Camera3D RaylibRenderer::GetActiveCamera3D() const { return m_drawLists.GetCamera3D(); }
