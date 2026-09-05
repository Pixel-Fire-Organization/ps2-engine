#include "TestbedScene.h"

#include "EngineUi.h"
#include "platform/Platform.h"

namespace
{
    const TestbedScene s_Scenes[] = {
        {"GAMEPAD", TestbedCategory::InputDevices, Scene_Gamepad_Init, Scene_Gamepad_Update, nullptr},
        {"KEYBOARD MOUSE", TestbedCategory::InputDevices, Scene_KeyboardMouse_Init, Scene_KeyboardMouse_Update, nullptr},
        {"TOUCH", TestbedCategory::InputDevices, Scene_Touch_Init, Scene_Touch_Update, nullptr},
        {"POINTER", TestbedCategory::InputDevices, Scene_Pointer_Init, Scene_Pointer_Update, nullptr},
        {"DEBUG CHORDS", TestbedCategory::InputDevices, Scene_Chords_Init, Scene_Chords_Update, nullptr},
        {"PLATFORM INFO", TestbedCategory::SystemsBudgets, Scene_PlatformInfo_Init, Scene_PlatformInfo_Update, nullptr},
    };

    const int s_SceneCount = static_cast<int>(sizeof(s_Scenes) / sizeof(s_Scenes[0]));
} // namespace

const TestbedScene* Testbed_Catalogue(int* outCount)
{
    if (outCount)
        *outCount = s_SceneCount;
    return s_Scenes;
}

const char* Testbed_CategoryName(TestbedCategory category)
{
    switch (category)
    {
    case TestbedCategory::InputDevices:
        return "INPUT AND DEVICES";
    case TestbedCategory::RenderingDisplay:
        return "RENDERING AND DISPLAY";
    case TestbedCategory::SystemsBudgets:
        return "SYSTEMS AND BUDGETS";
    case TestbedCategory::TimingPacing:
        return "TIMING AND PACING";
    case TestbedCategory::Count:
        break;
    }
    return "?";
}

void Testbed_DrawUnavailable(const char* what)
{
    Ui_LabelColored("NOT AVAILABLE ON THIS PLATFORM", UiColor::TextWarn);
    Ui_Label(what);
}
