#include "../include/graphics/DrawList.h"
#include "EngineDebug.h"

#include <GL/gl.h>
#include <rlgl.h>
#include "../include/graphics/PrimitiveGeometry.h"
#include "EngineMemory.h"

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

// ---------------------------------------------------------------------------
// Init / Shutdown
// ---------------------------------------------------------------------------

void DrawLists::Init(float* megaBatch) { CompilePrimitiveDLists(megaBatch); }

void DrawLists::Shutdown()
{
    if (m_dlCube)
    {
        glDeleteLists(m_dlCube, 1);
        m_dlCube = 0;
    }
    if (m_dlSphere)
    {
        glDeleteLists(m_dlSphere, 1);
        m_dlSphere = 0;
    }
    if (m_dlCylinder)
    {
        glDeleteLists(m_dlCylinder, 1);
        m_dlCylinder = 0;
    }
}

// ---------------------------------------------------------------------------
// CompilePrimitiveDLists
// ---------------------------------------------------------------------------
// Extracts separated (stride=0) vertex / normal / UV arrays from the
// interleaved MODEL_* tables and compiles one ps2gl display list per
// primitive shape.
//
// ps2gl constraints (see thirdparty/ps2gl/README.md):
//   • glVertexPointer / glNormalPointer / glTexCoordPointer: stride MUST be 0.
//   • glDrawElements: NOT implemented — triggers mError().
//   • Display lists with glDrawArrays: the DMA packet is cached after the
//     first glCallList, making subsequent calls very fast (no EE work at all).
// ---------------------------------------------------------------------------
void DrawLists::CompilePrimitiveDLists(float* megaBatch)
{
    // Helper: de-interleave MODEL_* (stride-8: xyz|nxyz|uv) into separate arrays.
    auto extract = [](const float* src, uint32_t count, float* vOut, float* nOut, float* uvOut)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            const float* p = src + i * 8;
            vOut[i * 3 + 0] = p[0];
            vOut[i * 3 + 1] = p[1];
            vOut[i * 3 + 2] = p[2];
            nOut[i * 3 + 0] = p[3];
            nOut[i * 3 + 1] = p[4];
            nOut[i * 3 + 2] = p[5];
            uvOut[i * 2 + 0] = p[6];
            uvOut[i * 2 + 1] = p[7];
        }
    };

    // Sub-allocate separated arrays from the renderer arena (megaBatch).
    float* cur = megaBatch;

    m_cubeVerts = cur;
    cur += PRIMITIVE_CUBE_VERTEX_COUNT * 3;
    m_cubeNorms = cur;
    cur += PRIMITIVE_CUBE_VERTEX_COUNT * 3;
    m_cubeUVs = cur;
    cur += PRIMITIVE_CUBE_VERTEX_COUNT * 2;

    m_sphereVerts = cur;
    cur += PRIMITIVE_SPHERE_VERTEX_COUNT * 3;
    m_sphereNorms = cur;
    cur += PRIMITIVE_SPHERE_VERTEX_COUNT * 3;
    m_sphereUVs = cur;
    cur += PRIMITIVE_SPHERE_VERTEX_COUNT * 2;

    m_cylVerts = cur;
    cur += PRIMITIVE_CYLINDER_VERTEX_COUNT * 3;
    m_cylNorms = cur;
    cur += PRIMITIVE_CYLINDER_VERTEX_COUNT * 3;
    m_cylUVs = cur;
    cur += PRIMITIVE_CYLINDER_VERTEX_COUNT * 2;

    extract(MODEL_CUBE, PRIMITIVE_CUBE_VERTEX_COUNT, m_cubeVerts, m_cubeNorms, m_cubeUVs);
    extract(MODEL_SPHERE, PRIMITIVE_SPHERE_VERTEX_COUNT, m_sphereVerts, m_sphereNorms, m_sphereUVs);
    extract(MODEL_CYLINDER, PRIMITIVE_CYLINDER_VERTEX_COUNT, m_cylVerts, m_cylNorms, m_cylUVs);

    // Flush any pending rlgl geometry before touching ps2gl client state directly.
    rlDrawRenderBatchActive();

    // Enable vertex array client state — these are immediate (not recorded in DLists).
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    // --- CUBE ---
    m_dlCube = glGenLists(1);
    glVertexPointer(3, GL_FLOAT, 0, m_cubeVerts);
    glNormalPointer(GL_FLOAT, 0, m_cubeNorms);
    glTexCoordPointer(2, GL_FLOAT, 0, m_cubeUVs);
    glNewList(m_dlCube, GL_COMPILE);
    glDrawArrays(GL_TRIANGLES, 0, PRIMITIVE_CUBE_VERTEX_COUNT);
    glEndList();

    // --- SPHERE ---
    m_dlSphere = glGenLists(1);
    glVertexPointer(3, GL_FLOAT, 0, m_sphereVerts);
    glNormalPointer(GL_FLOAT, 0, m_sphereNorms);
    glTexCoordPointer(2, GL_FLOAT, 0, m_sphereUVs);
    glNewList(m_dlSphere, GL_COMPILE);
    glDrawArrays(GL_TRIANGLES, 0, PRIMITIVE_SPHERE_VERTEX_COUNT);
    glEndList();

    // --- CYLINDER ---
    m_dlCylinder = glGenLists(1);
    glVertexPointer(3, GL_FLOAT, 0, m_cylVerts);
    glNormalPointer(GL_FLOAT, 0, m_cylNorms);
    glTexCoordPointer(2, GL_FLOAT, 0, m_cylUVs);
    glNewList(m_dlCylinder, GL_COMPILE);
    glDrawArrays(GL_TRIANGLES, 0, PRIMITIVE_CYLINDER_VERTEX_COUNT);
    glEndList();

    Engine_LogInfo("DrawLists: Primitive DLists compiled (cube=%u, sphere=%u, cyl=%u)", m_dlCube, m_dlSphere, m_dlCylinder);
}

unsigned int DrawLists::GetListForType(Primitive3D type) const
{
    switch (type)
    {
    case Primitive3D::Sphere:
        return m_dlSphere;
    case Primitive3D::Cylinder:
        return m_dlCylinder;
    default:
        return m_dlCube;
    }
}

bool DrawLists::AddPrimitive(const PrimitiveDrawEntry& entry)
{
    if (entry.textureId == -1)
    {
        if (untexturedCount >= GFX_MAX_DRAW_LIST_LENGTH)
        {
            Engine_LogError("DrawLists: Max untextured primitives reached (%d). Rejecting.", GFX_MAX_DRAW_LIST_LENGTH);
            return false;
        }
        untexturedPrims[untexturedCount++] = entry;
    }
    else
    {
        if (texturedCount >= GFX_MAX_DRAW_LIST_LENGTH)
        {
            Engine_LogError("DrawLists: Max textured primitives reached (%d). Rejecting.", GFX_MAX_DRAW_LIST_LENGTH);
            return false;
        }
        texturedPrims[texturedCount++] = entry;
    }
    return true;
}

bool DrawLists::AddModel(const ModelDrawEntry& entry)
{
    if (modelCount >= GFX_MAX_DRAW_LIST_LENGTH)
    {
        Engine_LogError("DrawLists: Max models reached (%d). Rejecting.", GFX_MAX_DRAW_LIST_LENGTH);
        return false;
    }

    models[modelCount++] = entry;
    return true;
}

bool DrawLists::AddUIDraw(const UIDrawEntry& entry)
{
    if (uiCount >= GFX_MAX_DRAW_LIST_LENGTH)
    {
        Engine_LogError("DrawLists: Max UI items reached (%d). Rejecting.", GFX_MAX_DRAW_LIST_LENGTH);
        return false;
    }

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

void DrawLists::Reset(const bool resetSkybox)
{
    untexturedCount = 0;
    texturedCount = 0;
    modelCount = 0;
    uiCount = 0;

    if (resetSkybox)
        skyboxResourceId = -1;
}
