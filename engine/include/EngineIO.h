#pragma once

#include <stddef.h>

typedef void (*IO_Callback)(const void* data, size_t size, void* userData);

bool Engine_IO_Init();

// Enqueue a file for asynchronous reading.
//
// ARCHIVE SEAM: this (plus the header peek in EngineResource) is the single
// point where assets are resolved to bytes. Today `filepath` names a loose
// .ps2a file on disc. A future flat archive (a header of per-file offset/size
// entries followed by the raw bytes — no folder hierarchy) drops in HERE: map
// `filepath` → {offset, size} via the header, then seek+read that span from the
// single archive file. No caller outside this seam needs to change.
bool Engine_IO_ReadAsync(const char* filepath, IO_Callback callback, void* userData);

void Engine_IO_Update();

void Engine_IO_Shutdown();

void Engine_IO_AcquireFileAccess();
void Engine_IO_ReleaseFileAccess();
