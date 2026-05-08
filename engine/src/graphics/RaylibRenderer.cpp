#include "../include/graphics/RaylibRenderer.h"
#include <cmath>
#include <cstring>
#include <raylib.h>
#include <rlgl.h>

#include "../include/graphics/DrawList.h"
#include "../include/graphics/PrimitiveGeometry.h"
#include "EngineApp.h"
#include "EngineDebug.h"
#include "EngineMemory.h"
#include "EngineResource.h"
#include "Macros.h"

#define MAX_MEGA_BATCH_VERTS 40000

// Helper for CPU transformation
static void TransformToBuffer(const float* model, uint32_t vertexCount, const Transform3D& transform, float* dest, uint32_t currentTotalVerts)
{
    // Strict bounds safety to prevent arena overflow
    if (currentTotalVerts + vertexCount > MAX_MEGA_BATCH_VERTS)
        return;

    const Vector3 pos = transform.GetPosition();
    const Vector3 rot = transform.GetRotation();
    const Vector3 scl = transform.GetScale();

    constexpr float toRad = PI / 180.f;
    const float cx = cosf(rot.x * toRad), sx = sinf(rot.x * toRad);
    const float cy = cosf(rot.y * toRad), sy = sinf(rot.y * toRad);
    const float cz = cosf(rot.z * toRad), sz = sinf(rot.z * toRad);

    const float r00 = cy * cz, r01 = -cy * sz, r02 = sy;
    const float r10 = cx * sz + sx * sy * cz, r11 = cx * cz - sx * sy * sz, r12 = -sx * cy;
    const float r20 = sx * sz - cx * sy * cz, r21 = sx * cz + cx * sy * sz, r22 = cx * cy;

    const float m0 = r00 * scl.x, m1 = r01 * scl.y, m2 = r02 * scl.z, m3 = pos.x;
    const float m4 = r10 * scl.x, m5 = r11 * scl.y, m6 = r12 * scl.z, m7 = pos.y;
    const float m8 = r20 * scl.x, m9 = r21 * scl.y, m10 = r22 * scl.z, m11 = pos.z;

    for (uint32_t i = 0; i < vertexCount; ++i)
    {
        const float lx = model[i * 8 + 0];
        const float ly = model[i * 8 + 1];
        const float lz = model[i * 8 + 2];
        const float nx = model[i * 8 + 3];
        const float ny = model[i * 8 + 4];
        const float nz = model[i * 8 + 5];

        dest[i * 8 + 0] = m0 * lx + m1 * ly + m2 * lz + m3;
        dest[i * 8 + 1] = m4 * lx + m5 * ly + m6 * lz + m7;
        dest[i * 8 + 2] = m8 * lx + m9 * ly + m10 * lz + m11;

        dest[i * 8 + 3] = r00 * nx + r01 * ny + r02 * nz;
        dest[i * 8 + 4] = r10 * nx + r11 * ny + r12 * nz;
        dest[i * 8 + 5] = r20 * nx + r21 * ny + r22 * nz;

        dest[i * 8 + 6] = model[i * 8 + 6];
        dest[i * 8 + 7] = model[i * 8 + 7];
    }
}

static void RenderPrimitiveCore(const float* model, uint32_t vertexCount, uint32_t stride, float cr = 1.f, float cg = 1.f, float cb = 1.f, bool useColor = false)
{
    if (useColor)
    {
        rlColor4ub(static_cast<unsigned char>(cr * 255.f), static_cast<unsigned char>(cg * 255.f), static_cast<unsigned char>(cb * 255.f), 255);
    }
    else
    {
        rlColor4ub(255, 255, 255, 255);
    }

    for (uint32_t i = 0; i < vertexCount; ++i)
    {
        rlTexCoord2f(model[i * stride + 6], model[i * stride + 7]);
        rlNormal3f(model[i * stride + 3], model[i * stride + 4], model[i * stride + 5]);
        rlVertex3f(model[i * stride + 0], model[i * stride + 1], model[i * stride + 2]);
    }
}

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

    // Retrieve Mega-Batch from dedicated ARENA_RENDERER slot.
    m_megaBatch = (float*)Engine_GetSlot(ARENA_RENDERER, 0);
    if (!m_megaBatch)
    {
        Engine_Panic("RaylibRenderer: Failed to retrieve Mega-Batch from ARENA_RENDERER!");
    }

    // Register the buffer usage so it shows up in memory stats
    size_t batchSizeBytes = MAX_MEGA_BATCH_VERTS * PRIMITIVE_VERTEX_STRIDE * sizeof(float);
    Engine_LoadToSlot(ARENA_RENDERER, 0, nullptr, batchSizeBytes);
    Engine_LogInfo("RaylibRenderer: Mega-Batch initialized at %p (%zu KB)", m_megaBatch, batchSizeBytes / 1024);
}

void RaylibRenderer::Shutdown()
{
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
    UIDrawEntry entry;
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
    RenderPrimitives(m_drawLists);
    RenderModels(m_drawLists);
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
    RenderPrimitiveCore(MODEL_CUBE, PRIMITIVE_CUBE_VERTEX_COUNT, PRIMITIVE_VERTEX_STRIDE);
    rlEnd();

    rlPopMatrix();

    rlDisableTexture();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

void RaylibRenderer::RenderPrimitives(DrawLists& lists)
{
    uint16_t uCount = lists.GetUntexturedCount();
    uint16_t tCount = lists.GetTexturedCount();

    if (uCount == 0 && tCount == 0)
        return;

    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlSetCullFace(RL_CULL_FACE_FRONT);
    rlEnableDepthTest();
    rlEnableDepthMask();

    // 1. Process Untextured - ONE GIANT BATCH
    if (uCount > 0)
    {
        uint32_t totalVerts = 0;
        rlSetTexture(rlGetTextureIdDefault());
        const PrimitiveDrawEntry* prims = lists.GetUntexturedPrims();

        // Pass 1: Transform everything to Mega-Batch (CPU bound)
        for (uint16_t i = 0; i < uCount; ++i)
        {
            const auto& entry = prims[i];
            uint32_t vCount = (entry.type == Primitive3D::Sphere) ? PRIMITIVE_SPHERE_VERTEX_COUNT
                : (entry.type == Primitive3D::Cylinder)           ? PRIMITIVE_CYLINDER_VERTEX_COUNT
                                                                  : PRIMITIVE_CUBE_VERTEX_COUNT;

            if (totalVerts + vCount > MAX_MEGA_BATCH_VERTS)
                break;

            const float* modelData = (entry.type == Primitive3D::Sphere) ? MODEL_SPHERE : (entry.type == Primitive3D::Cylinder) ? MODEL_CYLINDER : MODEL_CUBE;
            TransformToBuffer(modelData, vCount, entry.transform, m_megaBatch + (totalVerts * PRIMITIVE_VERTEX_STRIDE), totalVerts);
            totalVerts += vCount;
        }

        // Pass 2: Single rlBegin/rlEnd for ALL untextured primitives (GPU bound)
        uint32_t processedVerts = 0;
        rlBegin(RL_TRIANGLES);
        for (uint16_t i = 0; i < uCount; ++i)
        {
            const auto& entry = prims[i];
            uint32_t vCount = (entry.type == Primitive3D::Sphere) ? PRIMITIVE_SPHERE_VERTEX_COUNT
                : (entry.type == Primitive3D::Cylinder)           ? PRIMITIVE_CYLINDER_VERTEX_COUNT
                                                                  : PRIMITIVE_CUBE_VERTEX_COUNT;

            if (processedVerts + vCount > MAX_MEGA_BATCH_VERTS)
                break;

            // Set color for this primitive's vertices
            rlColor4ub(static_cast<unsigned char>(entry.color.r * 255.f), static_cast<unsigned char>(entry.color.g * 255.f), static_cast<unsigned char>(entry.color.b * 255.f), 255);

            float* primData = m_megaBatch + (processedVerts * 8);
            for (uint32_t v = 0; v < vCount; ++v)
            {
                rlTexCoord2f(primData[6], primData[7]);
                rlNormal3f(primData[3], primData[4], primData[5]);
                rlVertex3f(primData[0], primData[1], primData[2]);
                primData += 8;
            }
            processedVerts += vCount;
        }
        rlEnd();
    }

    // 2. Process Textured - Batch by unique texture
    if (tCount > 0)
    {
        int32_t lastTexId = -2;
        const PrimitiveDrawEntry* prims = lists.GetTexturedPrims();
        uint32_t texturedTotalVerts = 0;

        for (uint16_t i = 0; i < tCount; ++i)
        {
            const auto& entry = prims[i];
            uint32_t vCount = (entry.type == Primitive3D::Sphere) ? PRIMITIVE_SPHERE_VERTEX_COUNT
                : (entry.type == Primitive3D::Cylinder)           ? PRIMITIVE_CYLINDER_VERTEX_COUNT
                                                                  : PRIMITIVE_CUBE_VERTEX_COUNT;

            if (texturedTotalVerts + vCount > MAX_MEGA_BATCH_VERTS)
                break;

            const float* modelData = (entry.type == Primitive3D::Sphere) ? MODEL_SPHERE : (entry.type == Primitive3D::Cylinder) ? MODEL_CYLINDER : MODEL_CUBE;

            if (entry.textureId != lastTexId)
            {
                if (lastTexId != -2)
                    rlEnd(); // End previous texture batch

                const Texture2D* tex = static_cast<const Texture2D*>(Engine_Resource_Get(entry.textureId));
                if (tex && tex->id != 0)
                    rlEnableTexture(tex->id);
                else
                    rlSetTexture(rlGetTextureIdDefault());

                rlBegin(RL_TRIANGLES);
                lastTexId = entry.textureId;
            }

            rlColor4ub(255, 255, 255, 255);
            TransformToBuffer(modelData, vCount, entry.transform, m_megaBatch + (texturedTotalVerts * 8), texturedTotalVerts);

            float* primData = m_megaBatch + (texturedTotalVerts * 8);
            for (uint32_t v = 0; v < vCount; ++v)
            {
                rlTexCoord2f(primData[6], primData[7]);
                rlNormal3f(primData[3], primData[4], primData[5]);
                rlVertex3f(primData[0], primData[1], primData[2]);
                primData += 8;
            }
            texturedTotalVerts += vCount;
        }
        rlEnd(); // End final texture batch
    }

    rlDisableTexture();
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlDisableDepthTest();
    rlDisableDepthMask();
    rlSetCullFace(RL_CULL_FACE_BACK);

    DrawStats stats;
    memset(&stats, 0, sizeof(stats));
    stats.primitiveCount = uCount + tCount;
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
