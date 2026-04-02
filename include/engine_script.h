#ifndef ENGINE_SCRIPT_H
#define ENGINE_SCRIPT_H

#include <stdbool.h>
#include <stddef.h>

// Forward declaration of lua_State to keep header clean
typedef struct lua_State lua_State;

typedef struct {
    lua_State* L;
    int heapSlot;  // Even index
    int codeSlot;  // Odd index
    bool active;
} ScriptUnit;

// Initializer for the Lua subsystem
bool Engine_Script_Init(void);
void Engine_Script_Close(void);

// Script Loading and Execution
// loads a script into a free slot pair (even/odd)
int Engine_Script_Load(const void* data, size_t size);
bool Engine_Script_Run(int unitIndex);

// Called every frame to trigger OnUpdate in all active scripts
void Engine_Script_UpdateAll(float dt);

// PS2-specific panic BSOD trigger
void Engine_Panic(const char* message);

#endif // ENGINE_SCRIPT_H
