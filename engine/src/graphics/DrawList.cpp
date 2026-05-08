#include "../include/graphics/DrawList.h"
#include "../include/graphics/PrimitiveGeometry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <raylib.h>

#include "EngineResource.h"
#include "rlgl.h"

#define PRIMITIVE_VERTEX_STRIDE 8
#define PRIMITIVE_CUBE_VERTEX_COUNT 36
#define PRIMITIVE_SPHERE_VERTEX_COUNT 336
#define PRIMITIVE_CYLINDER_VERTEX_COUNT 144

static void RenderPrimitiveCore(const float* model, uint32_t vertexCount, uint32_t stride, float cr = 1.f, float cg = 1.f, float cb = 1.f, bool useColor = false);
static void TransformToBuffer(const float* model, uint32_t vertexCount, const Transform3D& transform, float* dest);

// clang-format off
DrawLists::DrawLists()
    : untexturedPrims{}, texturedPrims{}, models{}, uiItems{}
{
    camera3D.position   = Vector3{0.0f, 10.0f, 20.0f};
    camera3D.target     = Vector3{0.0f,  0.0f,  0.0f};
    camera3D.up         = Vector3{0.0f,  1.0f,  0.0f};
    camera3D.fovy       = 45.0f;
    camera3D.projection = CAMERA_PERSPECTIVE;

    camera2D.offset   = Vector2{0.0f, 0.0f};
    camera2D.target   = Vector2{0.0f, 0.0f};
    camera2D.rotation = 0.0f;
    camera2D.zoom     = 1.0f;
}
// clang-format on

bool DrawLists::AddPrimitive(const PrimitiveDrawEntry& entry)
{
    if (entry.textureId == -1)
    {
        if (untexturedCount >= GFX_MAX_DRAW_LIST_LENGTH)
            return false;
        untexturedPrims[untexturedCount++] = entry;
    }
    else
    {
        if (texturedCount >= GFX_MAX_DRAW_LIST_LENGTH)
            return false;
        texturedPrims[texturedCount++] = entry;
    }
    return true;
}

bool DrawLists::AddModel(const ModelDrawEntry& entry)
{
    if (modelCount >= GFX_MAX_DRAW_LIST_LENGTH)
        return false;

    models[modelCount++] = entry;

    return true;
}

bool DrawLists::AddUIDraw(const UIDrawEntry& entry)
{
    if (uiCount >= GFX_MAX_DRAW_LIST_LENGTH)
        return false;

    uiItems[uiCount++] = entry;

    return true;
}

bool DrawLists::SetSkyboxTexture(int32_t skyboxTextureId)
{
    if (skyboxTextureId > 0)
    {
        this->skyboxResourceId = skyboxTextureId;
        return true;
    }

    return false;
}

void DrawLists::SetActiveCamera3D(const Camera3D& camera) { camera3D = camera; }
void DrawLists::SetActiveCamera2D(const Camera2D& camera) { camera2D = camera; }

void DrawLists::Render()
{
    // RenderSkybox(); // Disabled to isolate hang

    BeginMode3D(camera3D);
    RenderPrimitives();
    RenderModels();
    EndMode3D();

    BeginMode2D(camera2D);
    RenderUI();
    EndMode2D();

    Reset(false);
}

void DrawLists::Reset(const bool resetSkybox)
{
    untexturedCount = 0;
    texturedCount = 0;
    modelCount = 0;
    uiCount = 0;

    if (resetSkybox)
        skyboxResourceId = -1;
}

void DrawLists::RenderSkybox() const
{
    if (skyboxResourceId == -1)
        return;

    const auto* tex = static_cast<const Texture2D*>(Engine_Resource_Get(skyboxResourceId));
    if (!tex || tex->id == 0)
    {
        rlEnableBackfaceCulling();
        rlEnableDepthMask();
        return;
    }

    // Skybox must render before everything else, behind all geometry
    rlDisableDepthMask();
    rlDisableBackfaceCulling();

    rlEnableTexture(tex->id);

    const Transform3D skyTransform({camera3D.position.x, camera3D.position.y, camera3D.position.z}, {0.f, 0.f, 0.f}, {500.f, 500.f, 500.f});

    rlPushMatrix();
    rlTranslatef(camera3D.position.x, camera3D.position.y, camera3D.position.z);
    rlScalef(500.f, 500.f, 500.f);

    rlBegin(RL_TRIANGLES);
    RenderPrimitiveCore(MODEL_CUBE, PRIMITIVE_CUBE_VERTEX_COUNT, PRIMITIVE_VERTEX_STRIDE);
    rlEnd();

    rlPopMatrix();

    rlSetTexture(0);
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

// Static buffer to store pre-transformed vertices (approx 2.5MB)
// 80,000 vertices is enough for ~220 spheres or ~2200 cubes.
#define MAX_MEGA_BATCH_VERTS 80000
static float s_megaBatch[MAX_MEGA_BATCH_VERTS * PRIMITIVE_VERTEX_STRIDE];

void DrawLists::RenderPrimitives()
{
    if (untexturedCount == 0 && texturedCount == 0)
        return;

    // Ensure we start with a clean, synchronized state
    rlDrawRenderBatchActive();
    
    rlEnableBackfaceCulling();
    rlSetCullFace(RL_CULL_FACE_FRONT);
    rlEnableDepthTest();
    rlEnableDepthMask();

    constexpr uint32_t CHUNK_SIZE = 256;

    // 1. Process Untextured Primitives
    if (untexturedCount > 0)
    {
        uint32_t totalVerts = 0;
        rlSetTexture(rlGetTextureIdDefault());

        for (uint16_t i = 0; i < untexturedCount; ++i)
        {
            const auto& entry = untexturedPrims[i];
            uint32_t vCount = (entry.type == Primitive3D::Sphere) ? PRIMITIVE_SPHERE_VERTEX_COUNT
                : (entry.type == Primitive3D::Cylinder)           ? PRIMITIVE_CYLINDER_VERTEX_COUNT
                                                                  : PRIMITIVE_CUBE_VERTEX_COUNT;

            if (totalVerts + vCount > MAX_MEGA_BATCH_VERTS)
                break;

            const float* modelData = (entry.type == Primitive3D::Sphere) ? MODEL_SPHERE : (entry.type == Primitive3D::Cylinder) ? MODEL_CYLINDER : MODEL_CUBE;

            // Transform into mega-batch on CPU (Optimized 4x3)
            TransformToBuffer(modelData, vCount, entry.transform, s_megaBatch + (totalVerts * PRIMITIVE_VERTEX_STRIDE));

            // Note: In untextured mode, we use the primitive color.
            // Since we can't batch different colors easily, we flush if color changes OR use vertex colors.
            // For now, let's just flush per-primitive to keep colors working, but using the CHUNK logic.

            rlColor4ub(static_cast<unsigned char>(entry.color.r * 255.f), static_cast<unsigned char>(entry.color.g * 255.f), static_cast<unsigned char>(entry.color.b * 255.f), 255);

            for (uint32_t vOffset = 0; vOffset < vCount; vOffset += CHUNK_SIZE)
            {
                uint32_t chunkCount = vCount - vOffset;
                if (chunkCount > CHUNK_SIZE)
                    chunkCount = CHUNK_SIZE;

                rlBegin(RL_TRIANGLES);
                float* chunkData = s_megaBatch + ((totalVerts + vOffset) * PRIMITIVE_VERTEX_STRIDE);
                for (uint32_t v = 0; v < chunkCount; ++v)
                {
                    rlTexCoord2f(chunkData[v * 8 + 6], chunkData[v * 8 + 7]);
                    rlNormal3f(chunkData[v * 8 + 3], chunkData[v * 8 + 4], chunkData[v * 8 + 5]);
                    rlVertex3f(chunkData[v * 8 + 0], chunkData[v * 8 + 1], chunkData[v * 8 + 2]);
                }
                rlEnd();
            }
            totalVerts += vCount;
        }
    }

    // 2. Process Textured Primitives
    if (texturedCount > 0)
    {
        // Simple texture grouping (no sort to avoid stack issues)
        int32_t lastTexId = -2;

        for (uint16_t i = 0; i < texturedCount; ++i)
        {
            const auto& entry = texturedPrims[i];
            uint32_t vCount = (entry.type == Primitive3D::Sphere) ? PRIMITIVE_SPHERE_VERTEX_COUNT
                : (entry.type == Primitive3D::Cylinder)           ? PRIMITIVE_CYLINDER_VERTEX_COUNT
                                                                  : PRIMITIVE_CUBE_VERTEX_COUNT;

            const float* modelData = (entry.type == Primitive3D::Sphere) ? MODEL_SPHERE : (entry.type == Primitive3D::Cylinder) ? MODEL_CYLINDER : MODEL_CUBE;

            if (entry.textureId != lastTexId)
            {
                const Texture2D* tex = static_cast<const Texture2D*>(Engine_Resource_Get(entry.textureId));
                if (tex && tex->id != 0)
                    rlEnableTexture(tex->id);
                else
                    rlSetTexture(rlGetTextureIdDefault());
                lastTexId = entry.textureId;
            }

            rlColor4ub(255, 255, 255, 255);

            // Transform directly to temporary space (reuse start of buffer)
            TransformToBuffer(modelData, vCount, entry.transform, s_megaBatch);

            for (uint32_t vOffset = 0; vOffset < vCount; vOffset += CHUNK_SIZE)
            {
                uint32_t chunkCount = vCount - vOffset;
                if (chunkCount > CHUNK_SIZE)
                    chunkCount = CHUNK_SIZE;

                rlBegin(RL_TRIANGLES);
                for (uint32_t v = 0; v < chunkCount; ++v)
                {
                    float* vd = s_megaBatch + ((vOffset + v) * 8);
                    rlTexCoord2f(vd[6], vd[7]);
                    rlNormal3f(vd[3], vd[4], vd[5]);
                    rlVertex3f(vd[0], vd[1], vd[2]);
                }
                rlEnd();
            }
        }
    }

    rlDisableTexture();
    rlDrawRenderBatchActive(); // Force flush of 3D geometry before state reset
    rlDisableBackfaceCulling();
    rlDisableDepthTest();
    rlDisableDepthMask();
    rlSetCullFace(RL_CULL_FACE_BACK); 

    m_lastStats.primitiveCount = untexturedCount + texturedCount;
}

void DrawLists::RenderModels() const {}
void DrawLists::RenderUI() const {}

static void RenderPrimitiveCore(const float* model, uint32_t vertexCount, uint32_t stride, float cr, float cg, float cb, bool useColor)
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

static void TransformToBuffer(const float* model, uint32_t vertexCount, const Transform3D& transform, float* dest)
{
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
