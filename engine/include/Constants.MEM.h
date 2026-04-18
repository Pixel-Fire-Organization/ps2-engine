#pragma once

// Hard limit for engine-managed memory (PS2 has 32MB total)
#define MEM_LIMIT_MAX_EE_RAM (30 * 1024 * 1024) // 30 MB

/*******************************/
/** ARENA                     **/
/*******************************/

#define MEM_BLOCK_SCRIPT_SIZE (4 * 1024 * 1024) // 4 MB (256 KB per slot)
#define MEM_BLOCK_SCRIPT_SLOTS 16 // 256 KB

#define MEM_BLOCK_CONFIG_SIZE (1 * 1024 * 1024) // 1 MB
#define MEM_BLOCK_CONFIG_SLOTS 4 // 256 KB

#define MEM_BLOCK_LEVEL_DATA_SIZE (4 * 1024 * 1024) // 4 MB
#define MEM_BLOCK_LEVEL_DATA_SLOTS 8 // 512 KB

#define MEM_ARENA_MAX_SLOTS 32
#define MEM_ARENA_SLOT_ALIGNMENT (16 * 1024) // 16 KB

/*******************************/
/** POOL                     **/
/*******************************/

#define MEM_POOL_MAIN_SIZE (1 * 1024 * 1024) // 1 MB
#define MEM_POOL_CHUNK_SIZE 256 // bytes per chunk
