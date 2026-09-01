#pragma once

#include <cstdint>

#include "graphics/DrawList.h"

// Draw runs per frame - a run is a span of vertices sharing one texture.
#define DESKTOP_MAX_DRAW_RUNS 1024

class DesktopGeometry final
{
public:
    // Interleaved, because everything is rebuilt per frame anyway - the
    // separated stride-0 layout the PS2 needs buys nothing here.
    struct Vertex
    {
        float x, y, z;
        float nx, ny, nz;
        float u, v;
        float r, g, b, a;
    };

    // A contiguous span of vertices sharing one texture: the unit of a draw call.
    struct DrawRun
    {
        uint32_t first;
        uint32_t count;
        uint32_t texture; // backend texture handle; 0 = untextured
    };

    DesktopGeometry();
    ~DesktopGeometry();

    DesktopGeometry(const DesktopGeometry&) = delete;
    DesktopGeometry(DesktopGeometry&&) = delete;
    DesktopGeometry& operator=(const DesktopGeometry&) = delete;
    DesktopGeometry& operator=(DesktopGeometry&&) = delete;

    // Clear the 3D staging for a new frame. 2D is NOT cleared here: DrawRect2D
    // runs during GameUpdate, before the renderer's BeginFrame, so wiping it at
    // frame start would discard what the game just submitted.
    void BeginFrame();

    // Clear the 2D staging. Call after the frame has been submitted.
    void EndFrame();

    // Build this frame's 3D geometry from the draw lists plus the resident level
    // sectors. `lists` is sorted by texture first, which is what lets runs
    // coalesce.
    void BuildFrame(DrawLists& lists, DrawStats* stats);

    void AddRect2D(int32_t x, int32_t y, int32_t w, int32_t h, const Color3& color);

    const Vertex* Vertices3D() const { return m_verts3D; }
    uint32_t Count3D() const { return m_count3D; }
    const Vertex* Vertices2D() const { return m_verts2D; }
    uint32_t Count2D() const { return m_count2D; }
    const DrawRun* Runs() const { return m_runs; }
    uint32_t RunCount() const { return m_runCount; }

    // Column-major, matching the PS2 path's convention.
    static void BuildModelMatrix(const Vector3& pos, const Vector3& rot, const Vector3& scale, float out[16]);
    static void BuildViewProjection(const Camera3D& camera, uint32_t width, uint32_t height, bool zeroToOneDepth, float out[16]);
    static void BuildOrtho2D(uint32_t width, uint32_t height, bool zeroToOneDepth, float out[16]);

private:
    static bool Reserve(Vertex*& array, uint32_t& capacity, uint32_t used, uint32_t extra);

    void AppendMesh(const float model[16], const float* verts, uint8_t components, const float* norms, const float* uvs, uint32_t vertexCount, uint8_t topology, Color3 color, uint32_t texture);
    void AppendPrimitive(const DrawLists& lists, const PrimitiveDrawEntry& entry);
    void AppendModel(const ModelDrawEntry& entry);
    void AppendLevelSectors();
    void PushRun(uint32_t firstVertex, uint32_t count, uint32_t texture);

    // Resolve a resource handle to the backend texture handle the resource
    // manager recorded at upload time (0 when not resident).
    static uint32_t ResolveTexture(int32_t resourceId);

    Vertex* m_verts3D;
    uint32_t m_count3D;
    uint32_t m_capacity3D;

    Vertex* m_verts2D;
    uint32_t m_count2D;
    uint32_t m_capacity2D;

    DrawRun m_runs[DESKTOP_MAX_DRAW_RUNS];
    uint32_t m_runCount;

    DrawStats* m_stats; // borrowed for the duration of BuildFrame
};
