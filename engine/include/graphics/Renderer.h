#pragma once

#include "EngineGraphics.h"
#include "EngineLevel.h"
#include "DrawList.h"

class Renderer {
public:
    Renderer() = default;
    Renderer(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;

    void operator=(const Renderer&) = delete;
    void operator=(Renderer&&) = delete;

    virtual ~Renderer() = default;

    virtual RendererType GetRendererType() const = 0;

    virtual void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale) = 0;
    virtual void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color) = 0;
    virtual void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, int32_t textureId) = 0;
    virtual void AddPrimitiveToDrawList(Primitive3D primitive, const Vector3& position, const Vector3& rotation, const Vector3& scale, Color3 color, int32_t textureId) = 0;
    virtual void AddUIToDrawList(const UI& ui, const Vector2& offset, const Vector2& scale) = 0;
    virtual void AddLevelToDrawList(const Level& level) = 0;
    virtual void AddModelToDrawList(int32_t modelId, const Vector3& position, const Vector3& rotation, const Vector3& scale) = 0;
    virtual void AddSkyToDrawList(int32_t resourceId) = 0;
    virtual void ClearDrawLists() = 0;

    virtual void Render() = 0;
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void DrawDebugOverlay() = 0;
    virtual void ClearFrame(const Color3& color) = 0;
    virtual void DrawRect2D(int32_t x, int32_t y, int32_t width, int32_t height, const Color3& color) = 0;
    virtual void DrawGrid(int32_t slices, float spacing) = 0;

    // --- Camera (fixed-slot model) ---
    // Write the pose of one of the GFX_MAX_CAMERAS_3D slots.
    virtual void SetCamera3D(CameraID id, const Camera3D& camera) = 0;
    // Choose which slot is the active (rendered) camera this frame.
    virtual void SetActiveCamera3D(CameraID id) = 0;
    // Set the single 2D / UI camera.
    virtual void SetActiveCamera2D(const Camera2D& camera) = 0;

    // --- Texture upload / release ---
    // Upload a decoded texture (level-0..N pixels + optional CLUT) to GS VRAM
    // and return a backend handle (0 = failure). Pixel pointers must be 16-byte
    // aligned; `format` selects the GS pixel storage mode.
    virtual uint32_t UploadTexture(const TextureUpload& upload) = 0;
    // Free the GS VRAM (and any backend bookkeeping) for a previously uploaded texture.
    virtual void ReleaseTexture(uint32_t handle) = 0;

    virtual bool IsInitialized() const = 0;
    virtual void Shutdown() = 0;

    virtual DrawStats GetLastStats() const = 0;
    virtual Camera3D GetActiveCamera3D() const = 0;

protected:
    DrawLists m_drawLists;

    virtual void RenderSkybox(const DrawLists& lists) = 0;
    virtual void RenderPrimitives(DrawLists& lists) = 0;
    virtual void RenderModels(const DrawLists& lists) = 0;
    virtual void RenderUI(const DrawLists& lists) = 0;
};
