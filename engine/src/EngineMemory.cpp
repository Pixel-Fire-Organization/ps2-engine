#include <string.h>
#include "Engine.h"

// Round ptr up to the next multiple of alignment.
// PRECONDITION: alignment must be a power of two (or 0 to skip alignment).
// Callers are responsible for validating this before calling.
static inline uintptr_t AlignForward(uintptr_t ptr, size_t alignment)
{
    if (alignment == 0)
        return ptr;
    uintptr_t a = (uintptr_t)alignment;
    // Safe only for power-of-two alignment: (ptr & (a-1)) gives the remainder.
    uintptr_t modulo = ptr & (a - 1u);
    if (modulo != 0)
    {
        ptr += a - modulo;
    }
    return ptr;
}

// Internal specialized arenas (hidden from header)
// GFX resources are managed by Raylib — only engine-internal arenas remain.
static MemoryArena g_ScriptArena;
static MemoryArena g_ConfigArena;
static MemoryArena g_LevelDataArena;

// Global pool (Encapsulated)
static MemoryPool g_MainPool;

void Engine_PoolInitMain(void* buffer, size_t capacity, size_t chunk_size)
{
    Engine_PoolInit(&g_MainPool, buffer, capacity, chunk_size);
}

void* Engine_PoolAllocMain(void) { return Engine_PoolAlloc(&g_MainPool); }

void Engine_PoolFreeMain(void* ptr) { Engine_PoolFree(&g_MainPool, ptr); }

void* Engine_PoolGetBufferMain(void) { return g_MainPool.buffer; }

// Max slots supported per arena type for metadata arrays
#define MAX_ARENA_SLOTS MEM_ARENA_MAX_SLOTS
static ResourceSlot s_Slots[ARENA_COUNT][MAX_ARENA_SLOTS];
static uint32_t s_SlotCounts[ARENA_COUNT];

static void Internal_InitSlots(ArenaType type, MemoryArena* arena, uint32_t count)
{
    if (count == 0)
        return;
    if (count > MAX_ARENA_SLOTS)
        count = MAX_ARENA_SLOTS;

    s_SlotCounts[type] = count;

    // Calculate size per slot, ensuring each slot start is 16KB aligned
    size_t alignment = MEM_ARENA_SLOT_ALIGNMENT;
    size_t total_capacity = arena->capacity;

    // We need to account for potential padding at the start of each slot
    // To be safe, we calculate a "safe" slot size
    size_t slot_capacity = (total_capacity / count);
    // Align slot capacity down to 16KB to ensure every start is aligned if we
    // start aligned
    slot_capacity = (slot_capacity / alignment) * alignment;

    uint8_t* ptr = arena->buffer;
    for (uint32_t i = 0; i < count; i++)
    {
        // Align the start of this specific slot
        uintptr_t aligned_start = AlignForward((uintptr_t)ptr, alignment);

        s_Slots[type][i].ptr = (void*)aligned_start;
        s_Slots[type][i].capacity = slot_capacity;
        s_Slots[type][i].usedSize = 0;
        s_Slots[type][i].locked = false;

        ptr = (uint8_t*)(aligned_start + slot_capacity);
    }
}

void Engine_ArenasInitSegmented(void* base_ptr)
{
    uint8_t* ptr = (uint8_t*)base_ptr;

    Engine_ArenaInit(&g_ScriptArena, ptr, MEM_BLOCK_SCRIPT_SIZE);
    Internal_InitSlots(ARENA_SCRIPT, &g_ScriptArena, MEM_BLOCK_SCRIPT_SLOTS);
    ptr += MEM_BLOCK_SCRIPT_SIZE;

    Engine_ArenaInit(&g_ConfigArena, ptr, MEM_BLOCK_CONFIG_SIZE);
    Internal_InitSlots(ARENA_CONFIG, &g_ConfigArena, MEM_BLOCK_CONFIG_SLOTS);
    ptr += MEM_BLOCK_CONFIG_SIZE;

    Engine_ArenaInit(&g_LevelDataArena, ptr, MEM_BLOCK_LEVEL_DATA_SIZE);
    Internal_InitSlots(ARENA_LEVEL_DATA, &g_LevelDataArena, MEM_BLOCK_LEVEL_DATA_SLOTS);
}

void* Engine_GetSlot(ArenaType type, uint32_t slotIndex)
{
    if (type >= ARENA_COUNT || slotIndex >= s_SlotCounts[type])
        return nullptr;
    return s_Slots[type][slotIndex].ptr;
}

size_t Engine_GetSlotCapacity(ArenaType type, uint32_t slotIndex)
{
    if (type >= ARENA_COUNT || slotIndex >= s_SlotCounts[type])
        return 0;
    return s_Slots[type][slotIndex].capacity;
}

bool Engine_LoadToSlot(ArenaType type, uint32_t slotIndex, const void* data, size_t size)
{
    if (type >= ARENA_COUNT || slotIndex >= s_SlotCounts[type])
        return false;

    ResourceSlot* slot = &s_Slots[type][slotIndex];
    if (slot->locked)
        return false;
    if (size > slot->capacity)
        return false;

    if (data && size > 0)
    {
        memcpy(slot->ptr, data, size);
    }
    slot->usedSize = size;
    return true;
}

void Engine_LockSlot(ArenaType type, uint32_t slotIndex)
{
    if (type >= ARENA_COUNT || slotIndex >= s_SlotCounts[type])
        return;
    s_Slots[type][slotIndex].locked = true;
}

void Engine_UnlockSlot(ArenaType type, uint32_t slotIndex)
{
    if (type >= ARENA_COUNT || slotIndex >= s_SlotCounts[type])
        return;
    s_Slots[type][slotIndex].locked = false;
}

void Engine_ClearSlot(ArenaType type, uint32_t slotIndex)
{
    if (type >= ARENA_COUNT || slotIndex >= s_SlotCounts[type])
        return;
    ResourceSlot* slot = &s_Slots[type][slotIndex];
    if (slot->locked)
        return;

    memset(slot->ptr, 0, slot->capacity);
    slot->usedSize = 0;
}

void* Engine_AddToArena(ArenaType type, size_t size, size_t alignment)
{
    switch (type)
    {
    case ARENA_SCRIPT:
        return Engine_ArenaAlloc(&g_ScriptArena, size, alignment);
    case ARENA_CONFIG:
        return Engine_ArenaAlloc(&g_ConfigArena, size, alignment);
    case ARENA_LEVEL_DATA:
        return Engine_ArenaAlloc(&g_LevelDataArena, size, alignment);
    default:
        return nullptr;
    }
}

void Engine_ArenaInit(MemoryArena* arena, void* backing_buffer, size_t capacity)
{
    arena->buffer = (uint8_t*)backing_buffer;
    arena->capacity = capacity;
    arena->offset = 0;
}

void* Engine_ArenaAlloc(MemoryArena* arena, size_t size, size_t alignment)
{
    // AlignForward uses a bitmask trick that is only correct for power-of-two
    // alignments. Catch bad values here so a misaligned alloc never silently
    // corrupts memory. alignment == 0 means "no alignment", which is also
    // accepted.
    if (alignment != 0 && !IS_POWER_OF_TWO(alignment))
    {
        Engine_LogError("Engine_ArenaAlloc: alignment %zu is not a power of two", alignment);
        return nullptr;
    }

    uintptr_t current_ptr = (uintptr_t)arena->buffer + arena->offset;
    uintptr_t aligned_ptr = AlignForward(current_ptr, alignment);
    size_t shift = (size_t)(aligned_ptr - (uintptr_t)arena->buffer);

    if (shift + size > arena->capacity)
    {
        return nullptr; // Out of memory
    }

    arena->offset = shift + size;
    return (void*)aligned_ptr;
}

void Engine_ArenaReset(MemoryArena* arena) { arena->offset = 0; }

void Engine_ArenaClear(MemoryArena* arena)
{
    if (arena->buffer)
    {
        memset(arena->buffer, 0, arena->capacity);
    }
    arena->offset = 0;
}

void Engine_PoolInit(MemoryPool* pool, void* backing_buffer, size_t capacity, size_t chunk_size)
{
    pool->buffer = (uint8_t*)backing_buffer;
    pool->capacity = capacity;
    if (chunk_size < sizeof(PoolFreeNode))
    {
        chunk_size = sizeof(PoolFreeNode);
    }
    pool->chunk_size = chunk_size;
    Engine_PoolReset(pool);
}

void Engine_PoolReset(MemoryPool* pool)
{
    if (pool->capacity < pool->chunk_size || !pool->buffer)
    {
        pool->head = nullptr;
        return;
    }

    size_t num_chunks = pool->capacity / pool->chunk_size;
    pool->head = (PoolFreeNode*)pool->buffer;
    PoolFreeNode* curr = pool->head;

    for (size_t i = 1; i < num_chunks; ++i)
    {
        PoolFreeNode* next_node = (PoolFreeNode*)(pool->buffer + i * pool->chunk_size);
        curr->next = next_node;
        curr = next_node;
    }
    curr->next = nullptr;
}

void* Engine_PoolAlloc(MemoryPool* pool)
{
    if (pool->head == nullptr)
    {
        return nullptr;
    }
    PoolFreeNode* node = pool->head;
    pool->head = pool->head->next;

    // Clear chunk memory for determinism (excluding what was used by the node
    // itself, actually we clear all of it)
    memset((void*)node, 0, pool->chunk_size);
    return (void*)node;
}

void Engine_PoolFree(MemoryPool* pool, void* ptr)
{
    if (ptr == nullptr)
        return;

    // In a robust pool allocator, you'd verify ptr is within bounds and aligned.
    // For speed we assume it is.
    PoolFreeNode* node = (PoolFreeNode*)ptr;
    node->next = pool->head;
    pool->head = node;
}
