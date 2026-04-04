#include "Engine.h"
#include <stdio.h>
#include <string.h>

typedef enum
{
  LOAD_STATE_LOADING,
  LOAD_STATE_SUCCESS,
  LOAD_STATE_FAILED
} LoadState;

static void OnFileLoaded(void* data, size_t size, void* userData)
{
  LoadState* state = (LoadState*) userData;
  Engine_LogInfo("Async File Load Complete: %zu bytes read", size);
  if (data)
  {
// Allocate space into System Slot 0
    if (Engine_LoadToSlot(ARENA_SYSTEM, 0, data, size))
    {
      void* persistentData = Engine_GetSlot(ARENA_SYSTEM, 0);
      size_t capacity = Engine_GetSlotCapacity(ARENA_SYSTEM, 0);

      // Safety: null terminate only if there's room, otherwise it might overflow
      if (size < capacity)
      {
        ((char*) persistentData)[size] = '\0';
        Engine_LogInfo("Slot 0 allocated string: %s", (char*) persistentData);
      }
      else
      {
        Engine_LogInfo("Slot 0 allocated data (not null terminated, size matches capacity)");
      }
    }
    *state = LOAD_STATE_SUCCESS;
  }
  else
  {
    *state = LOAD_STATE_FAILED;
  }
}

int main(void)
{
  EngineConfig config = {
      .windowTitle = "PS2 Engine Test"
  };

  if (!Engine_Init(config))
  {
    return -1;
  }

  LoadState loadState = LOAD_STATE_LOADING;
  // Attempting to stream a dummy file from cdrom root
  Engine_IO_ReadAsync("cdrom0:\\DUMMY.TXT;1", OnFileLoaded, &loadState);

  Camera3D camera = {0};
  camera.position = (Vector3){0.0f, 10.0f, 10.0f};
  camera.target = (Vector3){0.0f, 0.0f, 0.0f};
  camera.up = (Vector3){0.0f, 1.0f, 0.0f};
  camera.fovy = 45.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  Vector3 cubePosition = {0.0f, 0.0f, 0.0f};

  while (!WindowShouldClose())
  {
    Engine_Update();

    // Multi-controller Input Example
    if (IsGamepadAvailable(0))
    {
      if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
      {
        cubePosition.y += 0.1f;
      }
    }
    if (IsGamepadAvailable(1))
    {
      if (IsGamepadButtonDown(1, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
      {
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
    if (loadState == LOAD_STATE_SUCCESS)
    {
      DrawText("Async Load Status: SUCCESS", 10, 80, 20, DARKGREEN);
    }
    else if (loadState == LOAD_STATE_FAILED)
    {
      DrawText("Async Load Status: FAILED", 10, 80, 20, RED);
    }
    else
    {
      DrawText("Async Load Status: LOADING...", 10, 80, 20, ORANGE);
    }

    Engine_DrawDebugOverlay();

    EndDrawing();
  }

  Engine_Close();

  return 0;
}
