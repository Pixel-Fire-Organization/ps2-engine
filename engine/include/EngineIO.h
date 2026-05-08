#pragma once

#include <stddef.h>

typedef void (*IO_Callback)(const void* data, size_t size, void* userData);

bool Engine_IO_Init();

// Enqueue a file for asynchronous reading.
bool Engine_IO_ReadAsync(const char* filepath, IO_Callback callback, void* userData);

void Engine_IO_Update();

void Engine_IO_Shutdown();

void Engine_IO_AcquireFileAccess();
void Engine_IO_ReleaseFileAccess();
