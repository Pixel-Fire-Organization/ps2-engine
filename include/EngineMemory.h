#ifndef ENGINE_MEMORY_H
#define ENGINE_MEMORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint8_t *buffer;
  size_t capacity;
  size_t offset;
} MemoryArena;

typedef struct {
  void *ptr;
  size_t capacity;
  size_t usedSize;
  bool locked;
} ResourceSlot;

// Arena Types for segmented memory allocation
typedef enum {
  ARENA_TEXTURE,
  ARENA_MESH,
  ARENA_AUDIO,
  ARENA_SCRIPT,
  ARENA_UI,
  ARENA_SYSTEM,
  ARENA_COUNT
} ArenaType;

void Engine_ArenaInit(MemoryArena *arena, void *backing_buffer,
                      size_t capacity);
void *Engine_ArenaAlloc(MemoryArena *arena, size_t size, size_t alignment);
void *Engine_AddToArena(ArenaType type, size_t size, size_t alignment);
void Engine_ArenasInitSegmented(void *base_ptr);

// Slot API for O(1) asset replacement
void *Engine_GetSlot(ArenaType type, uint32_t slotIndex);
size_t Engine_GetSlotCapacity(ArenaType type, uint32_t slotIndex);
bool Engine_LoadToSlot(ArenaType type, uint32_t slotIndex, const void *data,
                       size_t size);
void Engine_LockSlot(ArenaType type, uint32_t slotIndex);
void Engine_UnlockSlot(ArenaType type, uint32_t slotIndex);
void Engine_ClearSlot(ArenaType type, uint32_t slotIndex);

void Engine_ArenaReset(MemoryArena *arena);
void Engine_ArenaClear(MemoryArena *arena);

// Pool Allocator (Fixed block size)
typedef struct PoolFreeNode {
  struct PoolFreeNode *next;
} PoolFreeNode;

typedef struct {
  uint8_t *buffer;
  size_t capacity;
  size_t chunk_size;
  PoolFreeNode *head;
} MemoryPool;

void Engine_PoolInitMain(void *buffer, size_t capacity, size_t chunk_size);
void *Engine_PoolAllocMain(void);
void Engine_PoolFreeMain(void *ptr);
void *Engine_PoolGetBufferMain(void);
void Engine_PoolInit(MemoryPool *pool, void *backing_buffer, size_t capacity,
                     size_t chunk_size);
void *Engine_PoolAlloc(MemoryPool *pool);
void Engine_PoolFree(MemoryPool *pool, void *ptr);
void Engine_PoolReset(MemoryPool *pool);

#endif // ENGINE_MEMORY_H
