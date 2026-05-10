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
    ClearModelDListCache();
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
    // Reset shared per-frame draw-call budget counter and stat accumulators.
    m_frameDrawCallsUsed = 0;
    m_framePrimCount = 0;
    m_frameModelMeshCount = 0;

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

    // Commit combined render stats now that all 3D sub-functions are done.
    DrawStats stats{};
    stats.primitiveCount = m_framePrimCount;
    stats.modelCount = m_frameModelMeshCount;
    m_drawLists.SetLastStats(stats);

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
    // VIF1 DMA shared budget (PS2-specific):
    //
    // m_frameDrawCallsUsed is shared with RenderModels — both functions check
    // it against GFX_DRAW_CALL_BUDGET before every glCallList call.
    // See Constants.GFX.h for the full budget analysis.
    // -----------------------------------------------------------------------
    const uint16_t totalPrims = uCount + tCount;
    const uint16_t budgetLeft = (GFX_DRAW_CALL_BUDGET > m_frameDrawCallsUsed) ? static_cast<uint16_t>(GFX_DRAW_CALL_BUDGET - m_frameDrawCallsUsed) : 0u;

    if (totalPrims > budgetLeft)
    {
        Engine_LogError("RenderPrimitives: CurPacket budget exceeded (%u used + %u prims > %u). "
                        "Excess primitives dropped to prevent VIF1 DMA overflow crash.",
                        m_frameDrawCallsUsed, totalPrims, (uint16_t)GFX_DRAW_CALL_BUDGET);
    }

    // Distribute available budget: fill untextured first, then textured with remainder.
    const uint16_t uRender = (uCount <= budgetLeft) ? uCount : budgetLeft;
    const uint16_t tBudget = (uRender < budgetLeft) ? static_cast<uint16_t>(budgetLeft - uRender) : 0u;
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
            ++m_frameDrawCallsUsed;
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
            ++m_frameDrawCallsUsed;
        }
    }

    m_framePrimCount = uRender + tRender;
    // Note: DrawStats are set by Render() after all sub-functions finish.
}

void RaylibRenderer::RenderModels(const DrawLists& lists)
{
    const uint16_t mCount = lists.GetModelCount();
    if (mCount == 0)
        return;

    // -----------------------------------------------------------------------
    // Model rendering strategy (PS2-specific):
    //
    // Models share the same ps2gl display list caching strategy as primitives.
    // On the first draw of a model, a DList is compiled for each unindexed
    // mesh (mesh.indices == nullptr).  Raylib's Mesh struct already stores
    // vertices / normals / texcoords as stride-0 float arrays, so they can be
    // passed directly to glVertexPointer / glNormalPointer / glTexCoordPointer
    // without any data copy.
    //
    // Per-model transform is pushed once (one glPushMatrix + glTranslatef +
    // glScalef block), then all meshes of that model are drawn with glCallList.
    // glColor4f is called once per model (not per mesh), so from the second
    // mesh onward CurMaterial is NOT re-dirtied → ps2gl skips the material
    // section of the VU1 context re-upload, saving ~10 qwords per extra mesh.
    //
    // Indexed mesh (mesh.indices != nullptr) support is NOT available:
    //   • glDrawElements() is a hard mError() in ps2gl.
    //   • pglDrawIndexedArrays() only handles unsigned-byte indices (< 256).
    // Such meshes are skipped with an error log on first encounter.
    // -----------------------------------------------------------------------

    const ModelDrawEntry* entries = lists.GetModels();
    uint16_t modelMeshDraws = 0;

    rlSetTexture(rlGetTextureIdDefault());

    for (uint16_t i = 0; i < mCount; ++i)
    {
        if (m_frameDrawCallsUsed >= GFX_DRAW_CALL_BUDGET)
        {
            Engine_LogError("RenderModels: CurPacket budget exhausted at model %u of %u. "
                            "Remaining models dropped to prevent VIF1 DMA overflow crash.",
                            (unsigned)i, (unsigned)mCount);
            break;
        }

        const ModelDrawEntry& entry = entries[i];
        const auto* model = static_cast<const Model*>(Engine_Resource_Get(entry.resourceId));
        if (!model)
        {
            Engine_LogError("RenderModels: resource %d not ready, skipping.", entry.resourceId);
            continue;
        }

        ModelDListEntry* dl = FindOrCompileModelDLists(model, entry.resourceId);
        if (!dl)
        {
            Engine_LogError("RenderModels: DList cache miss for resource %d "
                            "(cache full or all meshes unsupported).",
                            entry.resourceId);
            continue;
        }

        const Vector3 pos = entry.transform.GetPosition();
        const Vector3 rot = entry.transform.GetRotation();
        const Vector3 scl = entry.transform.GetScale();

        glPushMatrix();
        glTranslatef(pos.x, pos.y, pos.z);
        if (rot.x != 0.f)
            glRotatef(rot.x, 1.f, 0.f, 0.f);
        if (rot.y != 0.f)
            glRotatef(rot.y, 0.f, 1.f, 0.f);
        if (rot.z != 0.f)
            glRotatef(rot.z, 0.f, 0.f, 1.f);
        glScalef(scl.x, scl.y, scl.z);

        // Set color once per model — NOT between meshes.  Calling glColor4f
        // sets the CurMaterial dirty flag which triggers a full VU1 context
        // re-upload (~82 qwords).  If we keep it constant across meshes,
        // only the first mesh of this model pays the full context cost.
        glColor4f(1.f, 1.f, 1.f, 1.f);

        for (uint8_t m = 0; m < dl->meshCount; ++m)
        {
            if (dl->handles[m] == 0)
                continue; // unsupported / indexed mesh, already logged at compile

            if (m_frameDrawCallsUsed >= GFX_DRAW_CALL_BUDGET)
                break;

            // Bind diffuse texture for this mesh.
            const int matIdx = (model->meshMaterial) ? model->meshMaterial[m] : 0;
            if (model->materials)
            {
                const Texture2D& tex = model->materials[matIdx].maps[MATERIAL_MAP_DIFFUSE].texture;
                if (tex.id != 0)
                    rlEnableTexture(tex.id);
                else
                    rlSetTexture(rlGetTextureIdDefault());
            }

            glCallList(dl->handles[m]);
            ++m_frameDrawCallsUsed;
            ++modelMeshDraws;
        }

        glPopMatrix();
    }

    m_frameModelMeshCount = modelMeshDraws;
    rlDisableTexture();
}

void RaylibRenderer::RenderUI(const DrawLists& lists) { UNUSED_VAR(lists); }

// ---------------------------------------------------------------------------
// Model DList cache helpers
// ---------------------------------------------------------------------------

RaylibRenderer::ModelDListEntry* RaylibRenderer::FindOrCompileModelDLists(const Model* model, int32_t resourceId)
{
    // Fast path: find existing cache entry.
    for (uint8_t i = 0; i < m_modelDListCacheCount; ++i)
    {
        if (m_modelDListCache[i].resourceId == resourceId)
            return &m_modelDListCache[i];
    }

    // Cache miss — compile a new entry.
    if (m_modelDListCacheCount >= GFX_MAX_CACHED_MODELS)
    {
        Engine_LogError("FindOrCompileModelDLists: model DList cache full (%d entries). "
                        "Increase GFX_MAX_CACHED_MODELS or call ClearModelDListCache() "
                        "before loading new models.",
                        (int)GFX_MAX_CACHED_MODELS);
        return nullptr;
    }

    ModelDListEntry& entry = m_modelDListCache[m_modelDListCacheCount];
    entry.resourceId = resourceId;
    entry.meshCount = 0;
    memset(entry.handles, 0, sizeof(entry.handles));

    const int meshesToProcess = (model->meshCount < GFX_MAX_MODEL_MESH_COUNT) ? model->meshCount : GFX_MAX_MODEL_MESH_COUNT;

    for (int m = 0; m < meshesToProcess; ++m)
    {
        const Mesh& mesh = model->meshes[m];

        if (mesh.indices != nullptr)
        {
            // glDrawElements is a hard mError() in ps2gl.
            // pglDrawIndexedArrays only supports unsigned-byte indices (< 256 verts).
            Engine_LogError("FindOrCompileModelDLists: resource %d mesh %d has indices — "
                            "glDrawElements is not supported on PS2 (ps2gl limitation). "
                            "Export the model without vertex sharing or use GenMesh* functions.",
                            resourceId, m);
            entry.handles[m] = 0;
            ++entry.meshCount;
            continue;
        }

        if (!mesh.vertices || mesh.vertexCount == 0)
        {
            Engine_LogError("FindOrCompileModelDLists: resource %d mesh %d has no vertices, skipping.", resourceId, m);
            entry.handles[m] = 0;
            ++entry.meshCount;
            continue;
        }

        // Raylib's Mesh already stores vertices / normals / texcoords as
        // stride-0 float arrays — pass them directly (no data copy needed).
        // glVertexPointer / glNormalPointer calls are immediate state; only
        // the glDrawArrays call is recorded into the DList.
        glVertexPointer(3, GL_FLOAT, 0, mesh.vertices);

        if (mesh.normals)
            glNormalPointer(GL_FLOAT, 0, mesh.normals);

        if (mesh.texcoords)
            glTexCoordPointer(2, GL_FLOAT, 0, mesh.texcoords);

        entry.handles[m] = glGenLists(1);
        glNewList(entry.handles[m], GL_COMPILE);
        glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
        glEndList();

        ++entry.meshCount;
        Engine_LogInfo("FindOrCompileModelDLists: resource %d mesh %d → DList %u (%d verts)", resourceId, m, entry.handles[m], mesh.vertexCount);
    }

    ++m_modelDListCacheCount;
    return &entry;
}

void RaylibRenderer::ClearModelDListCache()
{
    for (uint8_t i = 0; i < m_modelDListCacheCount; ++i)
    {
        for (uint8_t m = 0; m < m_modelDListCache[i].meshCount; ++m)
        {
            if (m_modelDListCache[i].handles[m] != 0)
            {
                glDeleteLists(m_modelDListCache[i].handles[m], 1);
                m_modelDListCache[i].handles[m] = 0;
            }
        }
        m_modelDListCache[i].resourceId = -1;
        m_modelDListCache[i].meshCount = 0;
    }
    m_modelDListCacheCount = 0;
    Engine_LogInfo("RaylibRenderer: model DList cache cleared.");
}

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
