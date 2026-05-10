#pragma once
#include <cstdint>

#include "EngineGraphics.h"

struct DrawStats
{
    uint16_t primitiveCount; // primitives rendered last frame
    uint16_t modelCount; // model mesh draw calls last frame (one per unindexed mesh)
    uint16_t uniqueTextures; // unique texture IDs bound last frame (0 = untextured batch counts as 1 if any)
};

struct PrimitiveDrawEntry
{
    Transform3D transform;
    Color3 color;
    Primitive3D type;
    int32_t textureId = -1;
};

struct ModelDrawEntry
{
    int32_t resourceId;
    Transform3D transform;
};

struct UIDrawEntry
{
    UI ui;
    Vector2 offset;
    float scale;
};

class DrawLists
{
    PrimitiveDrawEntry untexturedPrims[GFX_MAX_DRAW_LIST_LENGTH];
    PrimitiveDrawEntry texturedPrims[GFX_MAX_DRAW_LIST_LENGTH];
    ModelDrawEntry models[GFX_MAX_DRAW_LIST_LENGTH];
    UIDrawEntry uiItems[GFX_MAX_DRAW_LIST_LENGTH]{};
    uint16_t untexturedCount = 0;
    uint16_t texturedCount = 0;
    uint16_t modelCount = 0;
    uint16_t uiCount = 0;
    int32_t skyboxResourceId = -1;

    Camera3D camera3D{};
    Camera2D camera2D{};

    DrawStats m_lastStats{};

    // Separated vertex / normal / UV arrays extracted from MODEL_* at init.
    // Pointers into the renderer arena buffer; persistent for display list lifetime.
    float* m_cubeVerts = nullptr;
    float* m_cubeNorms = nullptr;
    float* m_cubeUVs = nullptr;
    float* m_sphereVerts = nullptr;
    float* m_sphereNorms = nullptr;
    float* m_sphereUVs = nullptr;
    float* m_cylVerts = nullptr;
    float* m_cylNorms = nullptr;
    float* m_cylUVs = nullptr;

    // ps2gl display list handles — one per primitive type.
    // Compiled once at startup; the DMA packet is cached on first glCallList.
    unsigned int m_dlCube = 0;
    unsigned int m_dlSphere = 0;
    unsigned int m_dlCylinder = 0;

    // Compile display lists from separated geometry arrays.
    void CompilePrimitiveDLists(float* megaBatch);

public:
    DrawLists();

    DrawLists(const DrawLists&) = delete;
    DrawLists(DrawLists&&) = delete;
    DrawLists& operator=(const DrawLists&) = delete;
    DrawLists& operator=(DrawLists&&) = delete;

    // Initialise OpenGL display lists using the given pre-allocated geometry buffer.
    // Must be called once after the GL context is ready.
    void Init(float* megaBatch);

    // Free OpenGL display list resources.
    void Shutdown();

    bool AddPrimitive(const PrimitiveDrawEntry& entry);
    bool AddModel(const ModelDrawEntry& entry);
    bool AddUIDraw(const UIDrawEntry& entry);
    bool SetSkyboxTexture(int32_t skyboxTextureId);

    void SetActiveCamera3D(const Camera3D& camera);
    void SetActiveCamera2D(const Camera2D& camera);

    void Reset(bool resetSkybox = false);

    // Return the display list handle for a given primitive type.
    unsigned int GetListForType(Primitive3D type) const;

    // Getters for Renderer
    const PrimitiveDrawEntry* GetUntexturedPrims() const { return untexturedPrims; }
    uint16_t GetUntexturedCount() const { return untexturedCount; }

    const PrimitiveDrawEntry* GetTexturedPrims() const { return texturedPrims; }
    uint16_t GetTexturedCount() const { return texturedCount; }

    const ModelDrawEntry* GetModels() const { return models; }
    uint16_t GetModelCount() const { return modelCount; }

    const UIDrawEntry* GetUIItems() const { return uiItems; }
    uint16_t GetUICount() const { return uiCount; }

    int32_t GetSkyboxResourceId() const { return skyboxResourceId; }
    const Camera3D& GetCamera3D() const { return camera3D; }
    const Camera2D& GetCamera2D() const { return camera2D; }

    DrawStats GetLastStats() const { return m_lastStats; }
    void SetLastStats(const DrawStats& stats) { m_lastStats = stats; }
};
