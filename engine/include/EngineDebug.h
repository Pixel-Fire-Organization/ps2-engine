#pragma once

// Initialize debug logging and on-screen debug overlay
void Engine_InitDebug();

// Log a message
void Engine_LogInfo(const char* text, ...);

void Engine_LogError(const char* text, ...);

// Draw the debug overlay (FPS, memory usage, etc.)
// Assumes the active renderer's BeginFrame() has been called.
void Engine_DrawDebugOverlay();

// Draw a printable ASCII table (codepoints 33–127) for font/glyph diagnostics.
// Assumes the active renderer's BeginFrame() has been called.
void Engine_DrawAsciiTable();

// Stop the engine. Never returns: the active platform decides what a panic
// looks like, and every platform terminates. See docs/subsystems/DEBUG.md.
[[noreturn]] void Engine_Panic(const char* message);

// ---------------------------------------------------------------------------
// Performance Logger
// Triggered by holding L1+L2+R1+R2 on port 0.
// Dumps a per-frame snapshot (draw counts, VRAM, RAM, pads, BIOS) to console.
// Only active when enablePerfLogger was set in EngineConfig.
// ---------------------------------------------------------------------------
void Engine_PerfLogger_Init(bool enabled);
void Engine_PerfLogger_Tick();
