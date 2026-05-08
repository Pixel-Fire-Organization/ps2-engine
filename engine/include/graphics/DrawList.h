#pragma once
#include <cstdint>

#include "EngineGraphics.h"

struct DrawStats
{
    uint16_t primitiveCount;   // primitives submitted last frame
    uint16_t modelCount;       // models submitted last frame
    uint16_t uniqueTextures;   // unique texture IDs bound last frame (0 = untextured batch counts as 1 if any)
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

    void RenderSkybox() const;
    void RenderPrimitives();
    void RenderModels() const;
    void RenderUI() const;

public:
    DrawLists();

    DrawLists(const DrawLists&) = delete;
    DrawLists(DrawLists&&) = delete;
    DrawLists& operator=(const DrawLists&) = delete;
    DrawLists& operator=(DrawLists&&) = delete;

    bool AddPrimitive(const PrimitiveDrawEntry& entry);
    bool AddModel(const ModelDrawEntry& entry);
    bool AddUIDraw(const UIDrawEntry& entry);
    bool SetSkyboxTexture(int32_t skyboxTextureId);

    void SetActiveCamera3D(const Camera3D& camera);
    void SetActiveCamera2D(const Camera2D& camera);

    void Render();
    void Reset(bool resetSkybox = false);

    DrawStats GetLastStats() const { return m_lastStats; }
};
