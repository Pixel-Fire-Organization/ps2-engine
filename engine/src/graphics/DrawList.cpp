#include "../include/graphics/DrawList.h"

#include <cmath>
#include <cstdint>
#include <raylib.h>

#include "EngineApp.h"
#include "EngineResource.h"
#include "rlgl.h"

// clang-format off
const float MODEL_CUBE[] = {
    // --- Front (+Z) ---
    -0.5f, -0.5f,  0.5f,   0.f, 0.f, 1.f,   0.f, 0.f,
     0.5f, -0.5f,  0.5f,   0.f, 0.f, 1.f,   1.f, 0.f,
     0.5f,  0.5f,  0.5f,   0.f, 0.f, 1.f,   1.f, 1.f,
    -0.5f, -0.5f,  0.5f,   0.f, 0.f, 1.f,   0.f, 0.f,
     0.5f,  0.5f,  0.5f,   0.f, 0.f, 1.f,   1.f, 1.f,
    -0.5f,  0.5f,  0.5f,   0.f, 0.f, 1.f,   0.f, 1.f,

    // --- Back (-Z) ---
     0.5f, -0.5f, -0.5f,   0.f, 0.f,-1.f,   0.f, 0.f,
    -0.5f, -0.5f, -0.5f,   0.f, 0.f,-1.f,   1.f, 0.f,
    -0.5f,  0.5f, -0.5f,   0.f, 0.f,-1.f,   1.f, 1.f,
     0.5f, -0.5f, -0.5f,   0.f, 0.f,-1.f,   0.f, 0.f,
    -0.5f,  0.5f, -0.5f,   0.f, 0.f,-1.f,   1.f, 1.f,
     0.5f,  0.5f, -0.5f,   0.f, 0.f,-1.f,   0.f, 1.f,

    // --- Left (-X) ---
    -0.5f, -0.5f, -0.5f,  -1.f, 0.f, 0.f,   0.f, 0.f,
    -0.5f, -0.5f,  0.5f,  -1.f, 0.f, 0.f,   1.f, 0.f,
    -0.5f,  0.5f,  0.5f,  -1.f, 0.f, 0.f,   1.f, 1.f,
    -0.5f, -0.5f, -0.5f,  -1.f, 0.f, 0.f,   0.f, 0.f,
    -0.5f,  0.5f,  0.5f,  -1.f, 0.f, 0.f,   1.f, 1.f,
    -0.5f,  0.5f, -0.5f,  -1.f, 0.f, 0.f,   0.f, 1.f,

    // --- Right (+X) ---
     0.5f, -0.5f,  0.5f,   1.f, 0.f, 0.f,   0.f, 0.f,
     0.5f, -0.5f, -0.5f,   1.f, 0.f, 0.f,   1.f, 0.f,
     0.5f,  0.5f, -0.5f,   1.f, 0.f, 0.f,   1.f, 1.f,
     0.5f, -0.5f,  0.5f,   1.f, 0.f, 0.f,   0.f, 0.f,
     0.5f,  0.5f, -0.5f,   1.f, 0.f, 0.f,   1.f, 1.f,
     0.5f,  0.5f,  0.5f,   1.f, 0.f, 0.f,   0.f, 1.f,

    // --- Top (+Y) ---
    -0.5f,  0.5f,  0.5f,   0.f, 1.f, 0.f,   0.f, 0.f,
     0.5f,  0.5f,  0.5f,   0.f, 1.f, 0.f,   1.f, 0.f,
     0.5f,  0.5f, -0.5f,   0.f, 1.f, 0.f,   1.f, 1.f,
    -0.5f,  0.5f,  0.5f,   0.f, 1.f, 0.f,   0.f, 0.f,
     0.5f,  0.5f, -0.5f,   0.f, 1.f, 0.f,   1.f, 1.f,
    -0.5f,  0.5f, -0.5f,   0.f, 1.f, 0.f,   0.f, 1.f,

    // --- Bottom (-Y) ---
    -0.5f, -0.5f, -0.5f,   0.f,-1.f, 0.f,   0.f, 0.f,
     0.5f, -0.5f, -0.5f,   0.f,-1.f, 0.f,   1.f, 0.f,
     0.5f, -0.5f,  0.5f,   0.f,-1.f, 0.f,   1.f, 1.f,
    -0.5f, -0.5f, -0.5f,   0.f,-1.f, 0.f,   0.f, 0.f,
     0.5f, -0.5f,  0.5f,   0.f,-1.f, 0.f,   1.f, 1.f,
    -0.5f, -0.5f,  0.5f,   0.f,-1.f, 0.f,   0.f, 1.f,
};

// Octahedron — 6 axis-aligned vertices, 8 faces
// Vertices: A=(0,+0.5,0)  B=(0,-0.5,0)  C=(+0.5,0,0)
//           D=(-0.5,0,0)  E=(0,0,+0.5)  F=(0,0,-0.5)
// Normals = normalized position (vertex normal = face direction for an octahedron)
const float MODEL_SPHERE[] = {
    // --- Face 1: A, E, C (+Y +Z +X) ---
     0.0f, 0.5f, 0.0f,   0.f, 1.f, 0.f,   0.50f, 0.00f,
     0.0f, 0.0f, 0.5f,   0.f, 0.f, 1.f,   0.75f, 0.50f,
     0.5f, 0.0f, 0.0f,   1.f, 0.f, 0.f,   1.00f, 0.50f,

    // --- Face 2: A, C, F (+Y +X -Z) ---
     0.0f, 0.5f,  0.0f,   0.f, 1.f, 0.f,   0.50f, 0.00f,
     0.5f, 0.0f,  0.0f,   1.f, 0.f, 0.f,   1.00f, 0.50f,
     0.0f, 0.0f, -0.5f,   0.f, 0.f,-1.f,   0.25f, 0.50f,

    // --- Face 3: A, F, D (+Y -Z -X) ---
     0.0f, 0.5f,  0.0f,   0.f, 1.f,  0.f,   0.50f, 0.00f,
     0.0f, 0.0f, -0.5f,   0.f, 0.f, -1.f,   0.25f, 0.50f,
    -0.5f, 0.0f,  0.0f,  -1.f, 0.f,  0.f,   0.00f, 0.50f,

    // --- Face 4: A, D, E (+Y -X +Z) ---
     0.0f, 0.5f, 0.0f,   0.f, 1.f, 0.f,   0.50f, 0.00f,
    -0.5f, 0.0f, 0.0f,  -1.f, 0.f, 0.f,   0.00f, 0.50f,
     0.0f, 0.0f, 0.5f,   0.f, 0.f, 1.f,   0.75f, 0.50f,

    // --- Face 5: B, C, E (-Y +X +Z) ---
     0.0f, -0.5f, 0.0f,   0.f, -1.f, 0.f,   0.50f, 1.00f,
     0.5f,  0.0f, 0.0f,   1.f,  0.f, 0.f,   1.00f, 0.50f,
     0.0f,  0.0f, 0.5f,   0.f,  0.f, 1.f,   0.75f, 0.50f,

    // --- Face 6: B, F, C (-Y -Z +X) ---
     0.0f, -0.5f,  0.0f,   0.f, -1.f,  0.f,   0.50f, 1.00f,
     0.0f,  0.0f, -0.5f,   0.f,  0.f, -1.f,   0.25f, 0.50f,
     0.5f,  0.0f,  0.0f,   1.f,  0.f,  0.f,   1.00f, 0.50f,

    // --- Face 7: B, D, F (-Y -X -Z) ---
     0.0f, -0.5f,  0.0f,   0.f, -1.f,  0.f,   0.50f, 1.00f,
    -0.5f,  0.0f,  0.0f,  -1.f,  0.f,  0.f,   0.00f, 0.50f,
     0.0f,  0.0f, -0.5f,   0.f,  0.f, -1.f,   0.25f, 0.50f,

    // --- Face 8: B, E, D (-Y +Z -X) ---
     0.0f, -0.5f, 0.0f,   0.f, -1.f, 0.f,   0.50f, 1.00f,
     0.0f,  0.0f, 0.5f,   0.f,  0.f, 1.f,   0.75f, 0.50f,
    -0.5f,  0.0f, 0.0f,  -1.f,  0.f, 0.f,   0.00f, 0.50f,
};

// 4-slice cylinder — square cross-section, radius 0.5, height 1.0, centered at origin
// Corners (xz): p0=(+0.5,0)  p1=(0,+0.5)  p2=(-0.5,0)  p3=(0,-0.5)
// Side normals are per-vertex radial; cap normals are flat ±Y
const float MODEL_CYLINDER[] = {
    // --- Face 0: p0 → p1 ---
     0.5f, -0.5f, 0.0f,     1.f, 0.f, 0.f,    0.00f, 0.f,
     0.5f,  0.5f, 0.0f,     1.f, 0.f, 0.f,    0.00f, 1.f,
     0.0f,  0.5f, 0.5f,     0.f, 0.f, 1.f,    0.25f, 1.f,
     0.5f, -0.5f, 0.0f,     1.f, 0.f, 0.f,    0.00f, 0.f,
     0.0f,  0.5f, 0.5f,     0.f, 0.f, 1.f,    0.25f, 1.f,
     0.0f, -0.5f, 0.5f,     0.f, 0.f, 1.f,    0.25f, 0.f,

    // --- Face 1: p1 → p2 ---
     0.0f, -0.5f, 0.5f,     0.f, 0.f, 1.f,    0.25f, 0.f,
     0.0f,  0.5f, 0.5f,     0.f, 0.f, 1.f,    0.25f, 1.f,
    -0.5f,  0.5f, 0.0f,    -1.f, 0.f, 0.f,    0.50f, 1.f,
     0.0f, -0.5f, 0.5f,     0.f, 0.f, 1.f,    0.25f, 0.f,
    -0.5f,  0.5f, 0.0f,    -1.f, 0.f, 0.f,    0.50f, 1.f,
    -0.5f, -0.5f, 0.0f,    -1.f, 0.f, 0.f,    0.50f, 0.f,

    // --- Face 2: p2 → p3 ---
    -0.5f, -0.5f,  0.0f,   -1.f, 0.f,  0.f,   0.50f, 0.f,
    -0.5f,  0.5f,  0.0f,   -1.f, 0.f,  0.f,   0.50f, 1.f,
     0.0f,  0.5f, -0.5f,    0.f, 0.f, -1.f,   0.75f, 1.f,
    -0.5f, -0.5f,  0.0f,   -1.f, 0.f,  0.f,   0.50f, 0.f,
     0.0f,  0.5f, -0.5f,    0.f, 0.f, -1.f,   0.75f, 1.f,
     0.0f, -0.5f, -0.5f,    0.f, 0.f, -1.f,   0.75f, 0.f,

    // --- Face 3: p3 → p0 ---
     0.0f, -0.5f, -0.5f,    0.f, 0.f, -1.f,   0.75f, 0.f,
     0.0f,  0.5f, -0.5f,    0.f, 0.f, -1.f,   0.75f, 1.f,
     0.5f,  0.5f,  0.0f,    1.f, 0.f,  0.f,   1.00f, 1.f,
     0.0f, -0.5f, -0.5f,    0.f, 0.f, -1.f,   0.75f, 0.f,
     0.5f,  0.5f,  0.0f,    1.f, 0.f,  0.f,   1.00f, 1.f,
     0.5f, -0.5f,  0.0f,    1.f, 0.f,  0.f,   1.00f, 0.f,

    // === Top cap (y=+0.5, norm=+Y) — fan from center, CCW from above ===
     0.0f, 0.5f,  0.0f,     0.f, 1.f, 0.f,    0.50f, 0.50f,
     0.0f, 0.5f,  0.5f,     0.f, 1.f, 0.f,    0.50f, 1.00f,
     0.5f, 0.5f,  0.0f,     0.f, 1.f, 0.f,    1.00f, 0.50f,
     0.0f, 0.5f,  0.0f,     0.f, 1.f, 0.f,    0.50f, 0.50f,
    -0.5f, 0.5f,  0.0f,     0.f, 1.f, 0.f,    0.00f, 0.50f,
     0.0f, 0.5f,  0.5f,     0.f, 1.f, 0.f,    0.50f, 1.00f,
     0.0f, 0.5f,  0.0f,     0.f, 1.f, 0.f,    0.50f, 0.50f,
     0.0f, 0.5f, -0.5f,     0.f, 1.f, 0.f,    0.50f, 0.00f,
    -0.5f, 0.5f,  0.0f,     0.f, 1.f, 0.f,    0.00f, 0.50f,
     0.0f, 0.5f,  0.0f,     0.f, 1.f, 0.f,    0.50f, 0.50f,
     0.5f, 0.5f,  0.0f,     0.f, 1.f, 0.f,    1.00f, 0.50f,
     0.0f, 0.5f, -0.5f,     0.f, 1.f, 0.f,    0.50f, 0.00f,

    // === Bottom cap (y=-0.5, norm=-Y) — fan from center, CCW from below ===
     0.0f, -0.5f,  0.0f,    0.f, -1.f, 0.f,   0.50f, 0.50f,
     0.5f, -0.5f,  0.0f,    0.f, -1.f, 0.f,   1.00f, 0.50f,
     0.0f, -0.5f,  0.5f,    0.f, -1.f, 0.f,   0.50f, 1.00f,
     0.0f, -0.5f,  0.0f,    0.f, -1.f, 0.f,   0.50f, 0.50f,
     0.0f, -0.5f,  0.5f,    0.f, -1.f, 0.f,   0.50f, 1.00f,
    -0.5f, -0.5f,  0.0f,    0.f, -1.f, 0.f,   0.00f, 0.50f,
     0.0f, -0.5f,  0.0f,    0.f, -1.f, 0.f,   0.50f, 0.50f,
    -0.5f, -0.5f,  0.0f,    0.f, -1.f, 0.f,   0.00f, 0.50f,
     0.0f, -0.5f, -0.5f,    0.f, -1.f, 0.f,   0.50f, 0.00f,
     0.0f, -0.5f,  0.0f,    0.f, -1.f, 0.f,   0.50f, 0.50f,
     0.0f, -0.5f, -0.5f,    0.f, -1.f, 0.f,   0.50f, 0.00f,
     0.5f, -0.5f,  0.0f,    0.f, -1.f, 0.f,   1.00f, 0.50f,
};
// clang-format on

static void PrimitiveSetTexture(int32_t textureId, const Color3& color);
static void RenderCube(const PrimitiveDrawEntry& entry);
static void RenderCylinder(const PrimitiveDrawEntry& entry);
static void RenderSphere(const PrimitiveDrawEntry& entry);
static void RenderPrimitiveCore(const float* model, uint32_t vertexCount, uint32_t stride, const Transform3D& transform);

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
    RenderPrimitiveCore(MODEL_CUBE, PRIMITIVE_CUBE_VERTEX_COUNT, PRIMITIVE_VERTEX_STRIDE, skyTransform);
    rlEnd();

    rlSetTexture(0);
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

void DrawLists::RenderPrimitives() const
{
    rlBegin(RL_TRIANGLES);
    for (uint16_t i = 0; i < primitiveCount; ++i)
    {
        switch (primitives[i].type)
        {
        case Primitive3D::Cube:
            RenderCube(primitives[i]);
            break;
        case Primitive3D::Cylinder:
            RenderCylinder(primitives[i]);
            break;
        case Primitive3D::Sphere:
            RenderSphere(primitives[i]);
            break;
        default:
            Engine_LogError("Undefined primitive type was sent as a draw list item. Type: %u", primitives[i].type);
            break;
        }
    }
    rlEnd();
}

void DrawLists::RenderModels() const {}

void DrawLists::RenderUI() const {}

static void RenderCube(const PrimitiveDrawEntry& entry)
{
    PrimitiveSetTexture(entry.textureId, entry.color);
    RenderPrimitiveCore(MODEL_CUBE, PRIMITIVE_CUBE_VERTEX_COUNT, PRIMITIVE_VERTEX_STRIDE, entry.transform);

    if (entry.textureId != -1)
        rlSetTexture(0);
}

static void RenderCylinder(const PrimitiveDrawEntry& entry)
{
    PrimitiveSetTexture(entry.textureId, entry.color);
    RenderPrimitiveCore(MODEL_CYLINDER, PRIMITIVE_CYLINDER_VERTEX_COUNT, PRIMITIVE_VERTEX_STRIDE, entry.transform);

    if (entry.textureId != -1)
        rlSetTexture(0);
}

static void RenderSphere(const PrimitiveDrawEntry& entry)
{
    PrimitiveSetTexture(entry.textureId, entry.color);
    RenderPrimitiveCore(MODEL_SPHERE, PRIMITIVE_SPHERE_VERTEX_COUNT, PRIMITIVE_VERTEX_STRIDE, entry.transform);

    if (entry.textureId != -1)
        rlSetTexture(0);
}

static void RenderPrimitiveCore(const float* model, uint32_t vertexCount, uint32_t stride, const Transform3D& transform)
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

        rlVertex3f(vertX, vertY, vertZ);
        rlNormal3f(normX, normY, normZ);
        rlTexCoord2f(texU, texV);
    }
}

static void PrimitiveSetTexture(int32_t textureId, const Color3& color)
{
    if (textureId == -1)
    {
        rlColor3f(color.r, color.g, color.b);
    }
    else
    {
        const auto tex = static_cast<const Texture2D*>(Engine_Resource_Get(textureId));
        rlSetTexture(tex->id);
    }
}
