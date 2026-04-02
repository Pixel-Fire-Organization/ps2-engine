#include "../include/engine.h"
#include <stdio.h>
#include <string.h>

// Simulated callback for when our file read finishes
static void OnFileLoaded(void* data, size_t size, void* userData) {
    bool* loadedFlag = (bool*)userData;
    Engine_LogInfo("Async File Load Complete: %zu bytes read", size);
    if (data) {
        // Allocate space into System Slot 0
        if (Engine_LoadToSlot(ARENA_SYSTEM, 0, data, size)) {
            void* persistentData = Engine_GetSlot(ARENA_SYSTEM, 0);
            ((char*)persistentData)[size] = '\0'; // null terminate if string
            Engine_LogInfo("Slot 0 allocated string: %s", (char*)persistentData);
        }
    }
    *loadedFlag = true;
}

int main(void) {
    EngineConfig config = {
        .screenWidth = 640,
        .screenHeight = 448, // Standard PS2 NTSC resolution roughly
        .windowTitle = "PS2 Engine Test",
        .memoryPoolSize = 1024 * 1024,   // 1MB Pool
        .memoryMap = {
            .textureSize = 15 * 1024 * 1024, // 15MB
            .textureSlots = 10,              // 1.5MB per slot
            .meshSize    = 4  * 1024 * 1024, // 4MB
            .meshSlots   = 8,                // 512KB per slot
            .audioSize   = 4  * 1024 * 1024, // 4MB
            .audioSlots  = 8,                // 512KB per slot
            .scriptSize  = 4  * 1024 * 1024, // 4MB
            .scriptSlots = 16,               // 256KB per slot
            .uiSize      = 1  * 1024 * 1024, // 1MB
            .uiSlots     = 4,                // 256KB per slot
            .systemSize  = 2  * 1024 * 1024, // 2MB
            .systemSlots = 4                 // 512KB per slot
        }
    };

    if (!Engine_Init(config)) {
        return -1;
    }

    Engine_IO_Init();

    bool fileLoaded = false;
    // Attempting to stream a dummy file from cdrom root or host
#ifdef _PS2
    Engine_IO_ReadAsync("cdrom0:\\DUMMY.TXT;1", OnFileLoaded, &fileLoaded);
#else
    // For local PC testing (create a dummy.txt locally if you test on PC target)
    FILE* f = fopen("dummy.txt", "w");
    if(f) { fputs("Hello from Game CD!", f); fclose(f); }
    Engine_IO_ReadAsync("dummy.txt", OnFileLoaded, &fileLoaded);
#endif

    Camera3D camera = {0};
    camera.position = (Vector3){ 0.0f, 10.0f, 10.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    Vector3 cubePosition = { 0.0f, 0.0f, 0.0f };

    while (!WindowShouldClose()) {
        Engine_Update();
        Engine_IO_Update();

        // Multi-controller Input Example
        if (IsGamepadAvailable(0)) {
            if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
                cubePosition.y += 0.1f;
            }
        }
        if (IsGamepadAvailable(1)) {
            if (IsGamepadButtonDown(1, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
                cubePosition.y -= 0.1f;
            }
        }

        BeginDrawing();
            ClearBackground(RAYWHITE);

            BeginMode3D(camera);
                DrawCube(cubePosition, 2.0f, 2.0f, 2.0f, RED);
                DrawCubeWires(cubePosition, 2.0f, 2.0f, 2.0f, MAROON);
                DrawGrid(10, 1.0f);
            EndMode3D();

            DrawText("PS2 Engine Demo", 10, 50, 20, DARKGRAY);
            if (fileLoaded) {
                DrawText("Async Load Status: SUCCESS", 10, 80, 20, DARKGREEN);
            } else {
                DrawText("Async Load Status: LOADING...", 10, 80, 20, ORANGE);
            }
            
            Engine_DrawDebugOverlay();

        EndDrawing();
    }

    Engine_IO_Shutdown();
    Engine_Close();

    return 0;
}
