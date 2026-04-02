#include "engine_debug.h"
#include <raylib.h>
#include <stdio.h>
#include <stdarg.h>

static void CustomLog(int logLevel, const char *text, va_list args) {
    // Basic formatting
    const char *prefix = "INFO: ";
    if (logLevel == LOG_WARNING) prefix = "WARN: ";
    else if (logLevel == LOG_ERROR) prefix = "ERR : ";
    else if (logLevel == LOG_DEBUG) prefix = "DBG : ";

    printf("%s", prefix);
    vprintf(text, args);
    printf("\n");
}

void Engine_InitDebug(void) {
    SetTraceLogCallback(CustomLog);
    Engine_LogInfo("Engine Debug initialized.");
}

void Engine_LogInfo(const char* text, ...) {
    va_list args;
    va_start(args, text);
    CustomLog(LOG_INFO, text, args);
    va_end(args);
}

void Engine_LogError(const char* text, ...) {
    va_list args;
    va_start(args, text);
    CustomLog(LOG_ERROR, text, args);
    va_end(args);
}

void Engine_DrawDebugOverlay(void) {
    DrawFPS(10, 10);
    DrawText("System: PS2 Raylib Engine", 10, 30, 20, GREEN);
}
