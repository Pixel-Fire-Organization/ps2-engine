#pragma once

#include <cstdint>

/// Which group of the catalogue a scene belongs to. The menu draws one heading
/// per category with that category's scenes beneath it.
enum class TestbedCategory : uint8_t
{
    InputDevices = 0,
    RenderingDisplay,
    SystemsBudgets,
    TimingPacing,

    Count
};

/// One entry of the catalogue. `init` runs on entry, `update` every frame while
/// active, `shutdown` on the way out; the last two may be null.
struct TestbedScene
{
    const char* name;
    TestbedCategory category;
    void (*init)();
    void (*update)(float dt);
    void (*shutdown)();
};

/// @param outCount Receives how many entries the catalogue holds.
/// @return The catalogue, in declaration order.
const TestbedScene* Testbed_Catalogue(int* outCount);

/// @param category The group to name.
/// @return Its heading text.
const char* Testbed_CategoryName(TestbedCategory category);

/// Split a scene that will not fit the interface budget on the smallest screen.
/// L1 and R1 move between pages, and the page resets whenever a scene is
/// entered. Draw only the page returned.
/// @param pageCount How many pages the scene has.
/// @return The page to draw, counting from zero.
int Testbed_Page(int pageCount);

/// Draw the panel a scene shows when the thing it tests is not present here.
/// @param what The capability or subsystem that is unavailable.
void Testbed_DrawUnavailable(const char* what);

// --- Scene entry points ----------------------------------------------------

void Scene_PlatformInfo_Init();
void Scene_PlatformInfo_Update(float dt);

void Scene_Gamepad_Init();
void Scene_Gamepad_Update(float dt);

void Scene_KeyboardMouse_Init();
void Scene_KeyboardMouse_Update(float dt);

void Scene_Touch_Init();
void Scene_Touch_Update(float dt);

void Scene_Pointer_Init();
void Scene_Pointer_Update(float dt);

void Scene_Chords_Init();
void Scene_Chords_Update(float dt);

void Scene_Primitives_Init();
void Scene_Primitives_Update(float dt);

void Scene_ScreenAspect_Init();
void Scene_ScreenAspect_Update(float dt);

void Scene_DepthRange_Init();
void Scene_DepthRange_Update(float dt);

void Scene_UiGallery_Init();
void Scene_UiGallery_Update(float dt);

void Scene_Style_Init();
void Scene_Style_Update(float dt);

void Scene_DrawLoad_Init();
void Scene_DrawLoad_Update(float dt);

void Scene_Memory_Init();
void Scene_Memory_Update(float dt);

void Scene_Resources_Init();
void Scene_Resources_Update(float dt);

void Scene_LevelStream_Init();
void Scene_LevelStream_Update(float dt);
void Scene_LevelStream_Shutdown();

void Scene_Achievements_Init();
void Scene_Achievements_Update(float dt);
