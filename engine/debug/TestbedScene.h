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

/// Draw the panel a scene shows when the thing it tests is not present here.
/// @param what The capability or subsystem that is unavailable.
void Testbed_DrawUnavailable(const char* what);

// --- Scene entry points ----------------------------------------------------

void Scene_PlatformInfo_Init();
void Scene_PlatformInfo_Update(float dt);
