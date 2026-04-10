#ifndef ENGINE_IO_H
#define ENGINE_IO_H

#include <stdbool.h>
#include <stddef.h>

typedef void (*IO_Callback)(const void* data, size_t size, void* userData);

bool Engine_IO_Init(void);

// Enqueue a file for asynchronous reading.
bool Engine_IO_ReadAsync(const char* filepath, IO_Callback callback, void* userData);

void Engine_IO_Update(void);

void Engine_IO_Shutdown(void);

#endif // ENGINE_IO_H
