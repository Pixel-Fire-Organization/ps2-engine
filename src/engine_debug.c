#include "engine.h"
#include <raylib.h>
#include <stdarg.h>
#include <stdio.h>

static void CustomLog(int logLevel, const char *text, va_list args) {
  // Basic formatting
  const char *prefix = "INFO: ";
  if (logLevel == LOG_WARNING)
    prefix = "WARN: ";
  else if (logLevel == LOG_ERROR)
    prefix = "ERR : ";
  else if (logLevel == LOG_DEBUG)
    prefix = "DBG : ";

  printf("%s", prefix);
  vprintf(text, args);
  printf("\n");
}

void Engine_InitDebug(void) {
  SetTraceLogCallback(CustomLog);
  Engine_LogInfo("Engine Debug initialized.");
}

void Engine_LogInfo(const char *text, ...) {
  va_list args;
  va_start(args, text);
  CustomLog(LOG_INFO, text, args);
  va_end(args);
}

void Engine_LogError(const char *text, ...) {
  va_list args;
  va_start(args, text);
  CustomLog(LOG_ERROR, text, args);
  va_end(args);
}

void Engine_DrawDebugOverlay(void) {
  DrawFPS(10, 10);
  DrawText("System: PS2 Raylib Engine", 10, 30, 20, GREEN);
}

void Engine_Panic(const char *message) {
#ifdef DEBUG
  Engine_LogError("!!! PS2 PANIC !!! %s", message);

  if (!Engine_Is_GFX_Initialized()) {
    // We should init a minimal environment here.
    // This can happen if the memory map is invalid, or CORE functionality
    // failed for some reason.

    InitWindow(GFX_SCREEN_WIDTH, GFX_SCREEN_HEIGHT, "PS2 Engine");
  }

  while (!IsWindowReady())
    ;

  // Because Raylib requires a window to draw, and we will never close it, nor
  // will the user close it, the loop will be indefinite.
  while (!WindowShouldClose()) {

    BeginDrawing();
    ClearBackground(RED);
    DrawText("PS2 ENGINE PANIC", PANIC_UI_PADDING, PANIC_UI_PADDING,
             PANIC_UI_FONT_SIZE_TITLE, WHITE);
    DrawText("UNRECOVERABLE ERROR:", PANIC_UI_PADDING, 100,
             PANIC_UI_FONT_SIZE_SUBTITLE, YELLOW);
    DrawText(message, PANIC_UI_PADDING, 140, PANIC_UI_FONT_SIZE_BODY, WHITE);
    DrawText("HALTING EMOTION ENGINE...", PANIC_UI_PADDING, 200,
             PANIC_UI_FONT_SIZE_FOOTER, LIGHTGRAY);
    EndDrawing();

    if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
      // Reset PS2
      
    }
  }
#else

  while (1)
    ;

#endif
}
