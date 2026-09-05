#pragma once

#include "graphics/StagedGeometry.h"
#include "EngineCore.h"
#include "GlApi.h"
#include "graphics/Renderer.h"

// Resident GPU textures. Handles are the GL texture names themselves, and GL
// guarantees 0 is never a valid name - which is exactly the engine's "invalid
// handle" convention, so no translation is needed.
#define GL_MAX_RESIDENT_TEXTURES 256

class OpenGlRenderer final : public Renderer
{
public:
    OpenGlRenderer() = delete;
    explicit OpenGlRenderer(const EngineConfig& config);
    ~OpenGlRenderer() override = default;

    OpenGlRenderer(const OpenGlRenderer&) = delete;
    OpenGlRenderer(OpenGlRenderer&&) = delete;
    OpenGlRenderer& operator=(const OpenGlRenderer&) = delete;
    OpenGlRenderer& operator=(OpenGlRenderer&&) = delete;

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

private:
    bool CreateContext();
    bool CreateProgram();
    bool CreateWhiteTexture();
    void SetupVertexAttributes();
    void UploadAndDraw();

    HWND m_hwnd;
    HDC m_dc;
    HGLRC m_context;

    // True when a 3.3+ core profile was obtained. Decides VAO use and which
    // GLSL dialect the shaders were compiled from.
    bool m_coreProfile;
    int m_versionMajor;
    int m_versionMinor;

    GLuint m_program;
    GLint m_uniformViewProj;
    GLint m_uniformTexture;
    GLuint m_vao;
    GLuint m_vertexBuffer;
    GLsizei m_vertexBufferCapacity; // in bytes

    GLuint m_whiteTexture;
    GLuint m_textures[GL_MAX_RESIDENT_TEXTURES];

    StagedGeometry m_geometry;

    Color3 m_clearColor;
    uint32_t m_width;
    uint32_t m_height;

    DrawStats m_frameStats;
    bool m_initialized;
};
