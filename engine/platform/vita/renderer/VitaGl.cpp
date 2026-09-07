#include "VitaGl.h"

#include "../CommonDialog.h"

#include <cstdlib>
#include <cstring>

#include "EngineDebug.h"
#include "EngineMemory.h"
#include "Macros.h"
#include "PlatformConstants.h"
#include "graphics/TextureExpand.h"
#include "platform/Platform.h"

extern "C" {
#include <vitaGL.h>
}

VitaGlRenderer::VitaGlRenderer(const EngineConfig& config)
    : m_whiteTexture(0), m_geometry(), m_clearColor(Color3{0.0f, 0.0f, 0.0f}), m_width(GFX_SCREEN_WIDTH), m_height(GFX_SCREEN_HEIGHT), m_frameStats(), m_initialized(false)
{
    UNUSED_VAR(config);
    memset(m_textures, 0, sizeof(m_textures));

    float* arena = static_cast<float*>(Engine_GetSlot(ARENA_RENDERER, 0));
    if (!arena)
    {
        Engine_LogError("VitaGlRenderer: failed to retrieve ARENA_RENDERER slot 0");
        return;
    }
    m_drawLists.Init(arena);

    if (!vglInit(VITAGL_LEGACY_POOL_BYTES))
    {
        Engine_LogError("VitaGlRenderer: vglInit failed");
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (!CreateWhiteTexture())
    {
        Engine_LogError("VitaGlRenderer: could not create the untextured fallback; display remains held by vitaGL");
        return;
    }

    m_initialized = true;
    Engine_LogInfo("VitaGlRenderer: ready (%ux%u)", m_width, m_height);
}

bool VitaGlRenderer::CreateWhiteTexture()
{
    const uint8_t white[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (!tex)
        return false;

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    m_whiteTexture = static_cast<uint32_t>(tex);
    return true;
}

uint32_t VitaGlRenderer::UploadTexture(const TextureUpload& upload)
{
    const uint32_t width = static_cast<uint32_t>(upload.width);
    const uint32_t height = static_cast<uint32_t>(upload.height);
    if (!m_initialized || width == 0 || height == 0)
        return 0;

    int slot = -1;
    for (int i = 0; i < VITAGL_MAX_RESIDENT_TEXTURES; ++i)
    {
        if (m_textures[i] == 0)
        {
            slot = i;
            break;
        }
    }
    if (slot < 0)
    {
        Engine_LogError("VitaGlRenderer: texture registry full (%d)", VITAGL_MAX_RESIDENT_TEXTURES);
        return 0;
    }

    const size_t bytes = static_cast<size_t>(width) * height * 4u;
    uint8_t* rgba = static_cast<uint8_t*>(malloc(bytes));
    if (!rgba)
    {
        Engine_LogError("VitaGlRenderer: out of memory expanding a %ux%u texture", width, height);
        return 0;
    }

    if (!Gfx_ExpandToRgba8(upload, rgba, bytes))
    {
        free(rgba);
        Engine_LogError("VitaGlRenderer: could not expand a %ux%u texture", width, height);
        return 0;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (!tex)
    {
        free(rgba);
        Engine_LogError("VitaGlRenderer: glGenTextures failed");
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    free(rgba);

    m_textures[slot] = static_cast<uint32_t>(tex);
    return static_cast<uint32_t>(tex);
}

void VitaGlRenderer::ReleaseTexture(uint32_t handle)
{
    if (handle == 0)
        return;

    for (int i = 0; i < VITAGL_MAX_RESIDENT_TEXTURES; ++i)
    {
        if (m_textures[i] != handle)
            continue;
        GLuint tex = static_cast<GLuint>(handle);
        glDeleteTextures(1, &tex);
        m_textures[i] = 0;
        return;
    }
}

void VitaGlRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale)
{
    AddPrimitiveToDrawList(primitive, position, rotation, scale, Color3{1.0f, 1.0f, 1.0f}, -1);
}

void VitaGlRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color)
{
    AddPrimitiveToDrawList(primitive, position, rotation, scale, color, -1);
}

void VitaGlRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, int32_t textureId)
{
    AddPrimitiveToDrawList(primitive, position, rotation, scale, Color3{1.0f, 1.0f, 1.0f}, textureId);
}

void VitaGlRenderer::AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color, int32_t textureId)
{
    PrimitiveDrawEntry entry;
    entry.transform = Transform3D(position, rotation, scale);
    entry.color = color;
    entry.type = primitive;
    entry.textureId = textureId;
    m_drawLists.AddPrimitive(entry);
}

void VitaGlRenderer::AddLevelToDrawList(const Level& level) { UNUSED_VAR(level); }

void VitaGlRenderer::AddModelToDrawList(int32_t modelId, const Vector3& position, const Vector3& rotation, const Vector3& scale)
{
    ModelDrawEntry entry;
    entry.resourceId = modelId;
    entry.transform = Transform3D(position, rotation, scale);
    m_drawLists.AddModel(entry);
}

void VitaGlRenderer::AddSkyToDrawList(int32_t resourceId) { m_drawLists.SetSkyboxTexture(resourceId); }

void VitaGlRenderer::ClearDrawLists() { m_drawLists.Reset(false); }

void VitaGlRenderer::ClearFrame(const Color3& color) { m_clearColor = color; }

void VitaGlRenderer::DrawRect2D(int32_t x, int32_t y, int32_t width, int32_t height, const Color3& color) { m_geometry.AddRect2D(x, y, width, height, color); }

void VitaGlRenderer::DrawGrid(int32_t slices, float spacing)
{
    UNUSED_VAR(slices);
    UNUSED_VAR(spacing);
}

void VitaGlRenderer::BeginFrame()
{
    Platform* platform = Engine_GetPlatform();
    platform->GetFramebufferSize(&m_width, &m_height);

    m_geometry.SetFrameBudget(0, m_width, m_height);
    m_geometry.BeginFrame();
    m_frameStats = DrawStats{};
}

void VitaGlRenderer::Render() { m_geometry.BuildFrame(m_drawLists, &m_frameStats); }

void VitaGlRenderer::BindVertexArrays(const StagedGeometry::Vertex* base)
{
    const GLsizei stride = sizeof(StagedGeometry::Vertex);
    glVertexPointer(3, GL_FLOAT, stride, &base->x);
    glNormalPointer(GL_FLOAT, stride, &base->nx);
    glTexCoordPointer(2, GL_FLOAT, stride, &base->u);
    glColorPointer(4, GL_FLOAT, stride, &base->r);
}

void VitaGlRenderer::DrawStagedGeometry()
{
    const uint32_t count3D = m_geometry.Count3D();
    const uint32_t count2D = m_geometry.Count2D();
    if (count3D == 0 && count2D == 0)
        return;

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    float matrix[16];

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);

    if (count3D > 0)
    {
        glEnable(GL_DEPTH_TEST);
        StagedGeometry::BuildViewProjection(m_drawLists.GetCamera3D(), m_width, m_height, true, matrix);
        glLoadMatrixf(matrix);

        BindVertexArrays(m_geometry.Vertices3D());

        const StagedGeometry::DrawRun* runs = m_geometry.Runs();
        for (uint32_t i = 0; i < m_geometry.RunCount(); ++i)
        {
            glBindTexture(GL_TEXTURE_2D, runs[i].texture ? runs[i].texture : m_whiteTexture);
            glDrawArrays(GL_TRIANGLES, static_cast<GLint>(runs[i].first), static_cast<GLsizei>(runs[i].count));
        }
    }

    if (count2D > 0)
    {
        glDisable(GL_DEPTH_TEST);
        StagedGeometry::BuildOrtho2D(m_width, m_height, true, matrix);
        glLoadMatrixf(matrix);

        BindVertexArrays(m_geometry.Vertices2D());
        glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(count2D));
    }

    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void VitaGlRenderer::EndFrame()
{
    if (!m_initialized)
        return;

    glClearColor(m_clearColor.r, m_clearColor.g, m_clearColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    DrawStagedGeometry();

    Platform* platform = Engine_GetPlatform();
    const double waitStart = platform->GetTimeSeconds();
    vglSwapBuffers(VitaCommonDialog_IsActive() ? GL_TRUE : GL_FALSE);
    m_frameStats.presentWaitMs = static_cast<float>((platform->GetTimeSeconds() - waitStart) * 1000.0);

    m_geometry.EndFrame();

    m_drawLists.SetLastStats(m_frameStats);
    m_drawLists.Reset(false);
}

void VitaGlRenderer::DrawDebugOverlay() {}

void VitaGlRenderer::SetCamera3D(CameraID id, const Camera3D& camera) { m_drawLists.SetCamera3D(id, camera); }
void VitaGlRenderer::SetActiveCamera3D(CameraID id) { m_drawLists.SetActiveCamera3D(id); }
void VitaGlRenderer::SetActiveCamera2D(const Camera2D& camera) { m_drawLists.SetActiveCamera2D(camera); }

bool VitaGlRenderer::IsInitialized() const { return m_initialized; }

void VitaGlRenderer::Shutdown()
{
    if (!m_initialized)
        return;

    for (int i = 0; i < VITAGL_MAX_RESIDENT_TEXTURES; ++i)
    {
        if (!m_textures[i])
            continue;
        GLuint tex = static_cast<GLuint>(m_textures[i]);
        glDeleteTextures(1, &tex);
        m_textures[i] = 0;
    }
    if (m_whiteTexture)
    {
        GLuint tex = static_cast<GLuint>(m_whiteTexture);
        glDeleteTextures(1, &tex);
        m_whiteTexture = 0;
    }

    m_initialized = false;
}

RendererType VitaGlRenderer::GetRendererType() const { return RendererType::VitaGl; }
DrawStats VitaGlRenderer::GetLastStats() const { return m_drawLists.GetLastStats(); }
Camera3D VitaGlRenderer::GetActiveCamera3D() const { return m_drawLists.GetCamera3D(); }

void VitaGlRenderer::RenderSkybox(const DrawLists& lists) { UNUSED_VAR(lists); }
void VitaGlRenderer::RenderPrimitives(DrawLists& lists) { UNUSED_VAR(lists); }
void VitaGlRenderer::RenderModels(const DrawLists& lists) { UNUSED_VAR(lists); }
