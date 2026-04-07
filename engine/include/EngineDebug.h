#ifndef ENGINE_DEBUG_H
#define ENGINE_DEBUG_H

#include <stdbool.h>

// Initialize debug logging and on-screen debug overlay
void Engine_InitDebug(void);

// Log a message
void Engine_LogInfo(const char *text, ...);
void Engine_LogError(const char *text, ...);

// Draw the debug overlay (FPS, memory usage, etc.)
// Assumes raylib's BeginDrawing() has been called.
void Engine_DrawDebugOverlay(void);

// Draw a printable ASCII table (codepoints 33–127) for font/glyph diagnostics.
// Assumes raylib's BeginDrawing() has been called.
void Engine_DrawAsciiTable(void);

// PS2-specific panic BSOD trigger
void Engine_Panic(const char *message);

#endif // ENGINE_DEBUG_H
