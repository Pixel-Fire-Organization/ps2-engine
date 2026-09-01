#pragma once

#include "DesktopGeometry.h"
#include "EngineCore.h"
#include "graphics/Renderer.h"

#include <webgpu/webgpu.h>

// Resident GPU textures. Handles are index+1 so 0 stays "invalid" to every
// caller, matching what the PS2 backends promise.
#define WGPU_MAX_TEXTURES 256

class WebGpuRenderer final : public Renderer
{
public:
    WebGpuRenderer() = delete;
    explicit WebGpuRenderer(const EngineConfig& config);
    ~WebGpuRenderer() override = default;

    WebGpuRenderer(const WebGpuRenderer&) = delete;
    WebGpuRenderer(WebGpuRenderer&&) = delete;
    WebGpuRenderer& operator=(const WebGpuRenderer&) = delete;
    WebGpuRenderer& operator=(WebGpuRenderer&&) = delete;

    RendererType GetRendererType() const override;

    void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale) override;
    void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color) override;
    void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, int32_t textureId) override;
    void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color, int32_t textureId) override;
    void AddUIToDrawList(const UI& ui, const Vector2& offset, const Vector2& scale) override;
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
    void RenderUI(const DrawLists& lists) override;

private:
    // Matches the WGSL uniform block: mat4x4 is 64 bytes and the struct must
    // be a multiple of 16.
    struct Uniforms
    {
        float viewProj[16];
    };

    struct TextureEntry
    {
        WGPUTexture texture;
        WGPUTextureView view;
        WGPUBindGroup bindGroup;
    };

    bool InitDevice();
    bool CreatePipelines();
    bool CreateWhiteTexture();
    bool ConfigureSurface(uint32_t width, uint32_t height);
    bool EnsureDepthTexture(uint32_t width, uint32_t height);
    void ReleaseDepthTexture();

    WGPUBindGroup BindGroupFor(uint32_t handle) const;

    // --- wgpu objects -------------------------------------------------------
    WGPUInstance m_instance;
    WGPUAdapter m_adapter;
    WGPUDevice m_device;
    WGPUQueue m_queue;
    WGPUSurface m_surface;
    WGPUTextureFormat m_surfaceFormat;

    WGPURenderPipeline m_pipeline3D;
    WGPURenderPipeline m_pipeline2D;
    WGPUBindGroupLayout m_uniformLayout;
    WGPUBindGroupLayout m_textureLayout;
    WGPUBindGroup m_bindGroup3D;
    WGPUBindGroup m_bindGroup2D;
    WGPUBuffer m_uniformBuffer3D;
    WGPUBuffer m_uniformBuffer2D;
    WGPUBuffer m_vertexBuffer;
    uint64_t m_vertexBufferCapacity;
    WGPUSampler m_sampler;

    WGPUTexture m_depthTexture;
    WGPUTextureView m_depthView;

    // Sampled by untextured geometry, so one pipeline serves both cases rather
    // than two that differ only in whether a texture is bound.
    TextureEntry m_whiteTexture;
    TextureEntry m_textures[WGPU_MAX_TEXTURES];

    // Geometry staging is shared with the OpenGL backend: the transform and
    // texture batching are identical, only the upload differs.
    DesktopGeometry m_geometry;

    Color3 m_clearColor;
    uint32_t m_width;
    uint32_t m_height;

    DrawStats m_frameStats;
    bool m_initialized;
};
