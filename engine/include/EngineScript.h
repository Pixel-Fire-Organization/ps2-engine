#ifndef ENGINE_SCRIPT_H
#define ENGINE_SCRIPT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declaration of lua_State to keep header clean
typedef struct lua_State lua_State;

typedef struct {
  lua_State *L;
  uint32_t slotIndex; // Base index (EVEN: Heap, ODD: Bytecode)
  size_t heapOffset;
  bool active;
} ScriptUnit;

// Initializer for the Lua subsystem
bool Engine_Script_Init(void);
void Engine_Script_Close(void);

// Script Loading and Execution
// loads a script into a free slot pair (even/odd)
int Engine_Script_Load(const void *data, size_t size);
bool Engine_Script_Run(int unitIndex);

// Called every frame to trigger OnUpdate in all active scripts
void Engine_Script_UpdateAll(float dt);

#endif // ENGINE_SCRIPT_H
