#include "../include/graphics/DrawList.h"
#include "../include/graphics/PrimitiveGeometry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <raylib.h>

#include "EngineApp.h"
#include "EngineResource.h"
#include "rlgl.h"

static void RenderPrimitiveCore(const float* model, uint32_t vertexCount, uint32_t stride, const Transform3D& transform, float cr = 1.f, float cg = 1.f, float cb = 1.f, bool useColor = false);

// clang-format off
DrawLists::DrawLists()
    : primitives{}, models{}
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
    if (primitiveCount >= GFX_MAX_DRAW_LIST_LENGTH)
        return false;

    primitives[primitiveCount++] = entry;

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
    RenderSkybox();

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
    m_lastStats.modelCount = modelCount;

    primitiveCount = 0;
    modelCount = 0;
    uiCount = 0;

    if (resetSkybox)
        skyboxResourceId = -1;
}

void DrawLists::RenderSkybox() const
{
    if (skyboxResourceId == -1)
        return;

    const auto tex = static_cast<const Texture2D*>(Engine_Resource_Get(skyboxResourceId));
    if (!tex)
        return;

    // Skybox must render before everything else, behind all geometry
    rlDisableDepthMask();
    rlDisableBackfaceCulling();

    rlSetTexture(tex->id);

    const Transform3D skyTransform({camera3D.position.x, camera3D.position.y, camera3D.position.z}, {0.f, 0.f, 0.f}, {500.f, 500.f, 500.f});

    rlBegin(RL_TRIANGLES);
    RenderPrimitiveCore(MODEL_CUBE, PRIMITIVE_CUBE_VERTEX_COUNT, PRIMITIVE_VERTEX_STRIDE, skyTransform, 1.f, 1.f, 1.f, false);
    rlEnd();

    rlDisableTexture();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

void DrawLists::RenderPrimitives()
{
    m_lastStats.primitiveCount = primitiveCount;

    if (primitiveCount == 0)
        return;

    // Sort by textureId so same-texture primitives are contiguous.
    // This lets us open one rlBegin/rlEnd per unique texture group,
    // minimising GS DMA flushes on PS2 (the main perf cost).
    // 1. Sort by texture first, then by color for untextured primitives.
    // This allows us to batch untextured primitives that share the same color.
    std::sort(primitives, primitives + primitiveCount,
              [](const PrimitiveDrawEntry& a, const PrimitiveDrawEntry& b)
              {
                  if (a.textureId != b.textureId)
                      return a.textureId < b.textureId;
                  if (a.textureId == -1)
                  {
                      if (a.color.r != b.color.r)
                          return a.color.r < b.color.r;
                      if (a.color.g != b.color.g)
                          return a.color.g < b.color.g;
                      return a.color.b < b.color.b;
                  }
                  return false;
              });

    // Sentinel: no batch open yet.
    constexpr int32_t kNoBatch = INT32_MIN;
    int32_t currentTextureId = kNoBatch;
    Color3 currentColor = {-1.f, -1.f, -1.f};
    uint16_t uniqueTextures = 0;

    for (uint16_t i = 0; i < primitiveCount; ++i)
    {
        const PrimitiveDrawEntry& entry = primitives[i];

        // On a texture-group boundary: flush the current batch and start a new one.
        if (entry.textureId != currentTextureId)
        {
            // Only flush if we had a TEXTURED batch open.
            // Untextured batches (-1) now close themselves inside the loop.
            if (currentTextureId != kNoBatch && currentTextureId != -1)
            {
                rlEnd();
                rlDisableTexture();
            }

            currentTextureId = entry.textureId;
            ++uniqueTextures;

            if (currentTextureId != -1)
            {
                const auto* tex = static_cast<const Texture2D*>(Engine_Resource_Get(currentTextureId));
                if (tex && tex->id != 0)
                {
                    rlEnableTexture(tex->id);
                }
                else
                {
                    rlDisableTexture(); // resource not ready — render untextured
                }
                rlBegin(RL_TRIANGLES);
            }
        }

        // For untextured primitives each entry may carry a different solid colour.
        // Pass the colour explicitly into RenderPrimitiveCore so it is emitted
        // per vertex — ps2gl/GS requires colour to be set before each glVertex3f.
        const bool isUntextured = (currentTextureId == -1);

        if (entry.textureId == -1)
        {
            // Untextured path: One batch per primitive (safe)
            rlSetTexture(rlGetTextureIdDefault());
            rlBegin(RL_TRIANGLES);
            RenderPrimitiveCore(entry.type == Primitive3D::Cube ? MODEL_CUBE : (entry.type == Primitive3D::Cylinder ? MODEL_CYLINDER : MODEL_SPHERE),
                                entry.type == Primitive3D::Cube ? PRIMITIVE_CUBE_VERTEX_COUNT : (entry.type == Primitive3D::Cylinder ? PRIMITIVE_CYLINDER_VERTEX_COUNT : PRIMITIVE_SPHERE_VERTEX_COUNT),
                                PRIMITIVE_VERTEX_STRIDE, entry.transform, entry.color.r, entry.color.g, entry.color.b, true);
            rlEnd();
        }
        else
        {
            // Textured path: One batch per primitive (safe)
            const auto* tex = static_cast<const Texture2D*>(Engine_Resource_Get(entry.textureId));
            if (tex && tex->id != 0)
            {
                rlEnableTexture(tex->id);
            }
            else
            {
                rlSetTexture(rlGetTextureIdDefault());
            }

            rlBegin(RL_TRIANGLES);
            RenderPrimitiveCore(entry.type == Primitive3D::Cube ? MODEL_CUBE : (entry.type == Primitive3D::Cylinder ? MODEL_CYLINDER : MODEL_SPHERE),
                                entry.type == Primitive3D::Cube ? PRIMITIVE_CUBE_VERTEX_COUNT : (entry.type == Primitive3D::Cylinder ? PRIMITIVE_CYLINDER_VERTEX_COUNT : PRIMITIVE_SPHERE_VERTEX_COUNT),
                                PRIMITIVE_VERTEX_STRIDE, entry.transform, 1.f, 1.f, 1.f, false);
            rlEnd();
        }
    }

    // Close the last open batch.
    if (currentColor.r >= 0.f || (currentTextureId != kNoBatch && currentTextureId != -1))
    {
        rlEnd();
        rlDisableTexture();
    }

    m_lastStats.uniqueTextures = uniqueTextures;
}

void DrawLists::RenderModels() const {}

void DrawLists::RenderUI() const {}

static void RenderPrimitiveCore(const float* model, uint32_t vertexCount, uint32_t stride, const Transform3D& transform, float cr, float cg, float cb, bool useColor)
{
    const Vector3 position = transform.GetPosition();
    const Vector3 rotation = transform.GetRotation();
    const Vector3 scale = transform.GetScale();

    constexpr float toRad = PI / 180.f;
    const float cx = cosf(rotation.x * toRad), sx = sinf(rotation.x * toRad);
    const float cy = cosf(rotation.y * toRad), sy = sinf(rotation.y * toRad);
    const float cz = cosf(rotation.z * toRad), sz = sinf(rotation.z * toRad);
    const float r00 = cy * cz;
    const float r01 = -cy * sz;
    const float r02 = sy;
    const float r10 = cx * sz + sx * sy * cz;
    const float r11 = cx * cz - sx * sy * sz;
    const float r12 = -sx * cy;
    const float r20 = sx * sz - cx * sy * cz;
    const float r21 = sx * cz + cx * sy * sz;
    const float r22 = cx * cy;

    auto lx = 0.0f;
    auto ly = 0.0f;
    auto lz = 0.0f;
    auto nx = 0.0f;
    auto ny = 0.0f;
    auto nz = 0.0f;

    auto vertX = 0.0f;
    auto vertY = 0.0f;
    auto vertZ = 0.0f;

    auto normX = 0.0f;
    auto normY = 0.0f;
    auto normZ = 0.0f;

    auto texU = 0.0f;
    auto texV = 0.0f;

    if (useColor)
        rlColor4ub(static_cast<unsigned char>(cr * 255.f), static_cast<unsigned char>(cg * 255.f), static_cast<unsigned char>(cb * 255.f), 255);
    else
        rlColor4ub(255, 255, 255, 255);

    for (uint32_t i = 0; i < vertexCount; ++i)
    {
        lx = model[i * stride + 0] * scale.x;
        ly = model[i * stride + 1] * scale.y;
        lz = model[i * stride + 2] * scale.z;
        nx = model[i * stride + 3];
        ny = model[i * stride + 4];
        nz = model[i * stride + 5];

        vertX = r00 * lx + r01 * ly + r02 * lz + position.x;
        vertY = r10 * lx + r11 * ly + r12 * lz + position.y;
        vertZ = r20 * lx + r21 * ly + r22 * lz + position.z;

        normX = r00 * nx + r01 * ny + r02 * nz;
        normY = r10 * nx + r11 * ny + r12 * nz;
        normZ = r20 * nx + r21 * ny + r22 * nz;

        texU = model[i * stride + 6];
        texV = model[i * stride + 7];

        rlNormal3f(normX, normY, normZ);
        rlTexCoord2f(texU, texV);
        rlVertex3f(vertX, vertY, vertZ);
    }
}
