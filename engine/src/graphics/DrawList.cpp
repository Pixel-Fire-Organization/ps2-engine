#include "../include/graphics/DrawList.h"
#include "EngineDebug.h"

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
