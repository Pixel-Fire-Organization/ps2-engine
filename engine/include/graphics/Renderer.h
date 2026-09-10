#pragma once

#include "DrawList.h"
#include "UI.h"
#include "EngineGraphics.h"
#include "EngineLevel.h"

class Renderer
{
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
    /// Translate one frame of interface into this backend's screen-space path.
    ///
    /// Shared rather than per-backend so every renderer draws an identical
    /// interface, which is what makes comparing two backends on the same frame
    /// a usable way to locate a rendering bug.
    /// @param ui The quads built this frame, in draw order.
    /// @param offset Screen-space translation applied to every quad.
    /// @param scale Screen-space scale applied to every quad.
    void AddUIToDrawList(const UI& ui, const Vector2& offset, const Vector2& scale)
    {
        const UiQuad* quads = ui.Quads();
        const uint32_t count = ui.Count();
        for (uint32_t i = 0; i < count; ++i)
        {
            const UiQuad& src = quads[i];
            Quad2D quad;
            quad.x = static_cast<int32_t>(offset.x + static_cast<float>(src.x) * scale.x);
            quad.y = static_cast<int32_t>(offset.y + static_cast<float>(src.y) * scale.y);
            quad.w = static_cast<int32_t>(static_cast<float>(src.w) * scale.x);
            quad.h = static_cast<int32_t>(static_cast<float>(src.h) * scale.y);
            quad.texture = src.texture;
            quad.u0 = src.u0;
            quad.v0 = src.v0;
            quad.u1 = src.u1;
            quad.v1 = src.v1;
            quad.r = src.r;
            quad.g = src.g;
            quad.b = src.b;
            quad.a = src.a;
            DrawQuad2D(quad);
        }
    }
    virtual void AddLevelToDrawList(const Level& level) = 0;
    virtual void AddModelToDrawList(int32_t modelId, const Vector3& position, const Vector3& rotation, const Vector3& scale) = 0;
    virtual void AddSkyToDrawList(int32_t resourceId) = 0;
    virtual void ClearDrawLists() = 0;

    virtual void Render() = 0;
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void ClearFrame(const Color3& color) = 0;

    /// Draw one screen-space quad.
    ///
    /// The single screen-space primitive: everything two-dimensional reaches a
    /// backend through it, so a backend implements screen space exactly once.
    /// @param quad The quad to draw, in framebuffer pixels.
    virtual void DrawQuad2D(const Quad2D& quad) = 0;

    /// An opaque, untextured rectangle.
    ///
    /// Convenience over DrawQuad2D for callers that have no interface to build,
    /// such as the panic display and the game's own screen-space primitive.
    /// @param x Left edge in framebuffer pixels.
    /// @param y Top edge in framebuffer pixels.
    /// @param width Width in pixels.
    /// @param height Height in pixels.
    /// @param color The colour to fill with.
    void DrawRect2D(int32_t x, int32_t y, int32_t width, int32_t height, const Color3& color)
    {
        Quad2D quad;
        quad.x = x;
        quad.y = y;
        quad.w = width;
        quad.h = height;
        quad.texture = 0;
        quad.u0 = 0;
        quad.v0 = 0;
        quad.u1 = 0;
        quad.v1 = 0;
        quad.r = ToByte(color.r);
        quad.g = ToByte(color.g);
        quad.b = ToByte(color.b);
        quad.a = 255;
        DrawQuad2D(quad);
    }

    virtual void DrawGrid(int32_t slices, float spacing) = 0;

    // --- Camera (fixed-slot model) ---
    // Write the pose of one of the GFX_MAX_CAMERAS_3D slots.
    virtual void SetCamera3D(CameraID id, const Camera3D& camera) = 0;
    // Choose which slot is the active (rendered) camera this frame.
    virtual void SetActiveCamera3D(CameraID id) = 0;
    // Set the single 2D / UI camera.
    virtual void SetActiveCamera2D(const Camera2D& camera) = 0;

    // --- Texture upload / release ---
    // Upload a decoded texture and return a backend handle; 0 means failure.
    // Pixel pointers must be 16-byte aligned.
    virtual uint32_t UploadTexture(const TextureUpload& upload) = 0;
    virtual void ReleaseTexture(uint32_t handle) = 0;

    virtual bool IsInitialized() const = 0;
    virtual void Shutdown() = 0;

    virtual DrawStats GetLastStats() const = 0;
    virtual Camera3D GetActiveCamera3D() const = 0;

protected:
    /// @param value A colour channel in [0,1].
    /// @return The channel as an 8-bit value, clamped.
    static uint8_t ToByte(float value)
    {
        const float scaled = value * 255.0f;
        if (scaled <= 0.0f)
            return 0;
        if (scaled >= 255.0f)
            return 255;
        return static_cast<uint8_t>(scaled);
    }

    DrawLists m_drawLists;

    virtual void RenderSkybox(const DrawLists& lists) = 0;
    virtual void RenderPrimitives(DrawLists& lists) = 0;
    virtual void RenderModels(const DrawLists& lists) = 0;
};
