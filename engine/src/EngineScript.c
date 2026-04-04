#include "Engine.h"
#include <malloc.h>
#include <string.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>


#define MAX_SCRIPT_UNITS SCRIPTING_LUA_MAX_UNITS // (16 slots = 8 pairs)
static ScriptUnit s_ScriptUnits[MAX_SCRIPT_UNITS];

// Forward declare internal binding registers
static void RegisterCoreBindings(lua_State *L);
static void RegisterGraphicsBindings(lua_State *L);
static void RegisterInputBindings(lua_State *L);

// Custom allocator that restricts Lua to her assigned EVEN slot in ARENA_SCRIPT
static void *Engine_Lua_Alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  ScriptUnit *unit = (ScriptUnit *)ud;

  if (nsize == 0) {
    return NULL; 
  }

  void *slotBase = Engine_GetSlot(ARENA_SCRIPT, unit->slotIndex);
  size_t slotCapacity = Engine_GetSlotCapacity(ARENA_SCRIPT, unit->slotIndex);

  if (ptr == NULL) {
    // Linear allocation within the slot
    size_t alignedOffset = (unit->heapOffset + 7) & ~7;
    if (alignedOffset + nsize > slotCapacity) {
      return NULL; 
    }

    void *newPtr = (uint8_t *)slotBase + alignedOffset;
    unit->heapOffset = alignedOffset + nsize;
    return newPtr;
  } else {
    // Realloc: Move to new offset within the same slot
    size_t alignedOffset = (unit->heapOffset + 7) & ~7;
    if (alignedOffset + nsize > slotCapacity) {
      return NULL;
    }

    void *newPtr = (uint8_t *)slotBase + alignedOffset;
    memcpy(newPtr, ptr, (osize < nsize) ? osize : nsize);
    unit->heapOffset = alignedOffset + nsize;
    return newPtr;
  }
}

static int Internal_Lua_Panic(lua_State *L) {
  const char *msg = lua_tostring(L, -1);
  Engine_LogError("LUA PANIC: %s", msg);
  return 0;
}

bool Engine_Script_Init(void) {
  for (int i = 0; i < MAX_SCRIPT_UNITS; i++) {
    s_ScriptUnits[i].slotIndex = i * 2;
    s_ScriptUnits[i].heapOffset = 0;
    s_ScriptUnits[i].active = false;

    // Create the Lua State for this unit
    s_ScriptUnits[i].L =
        lua_newstate(Engine_Lua_Alloc, &s_ScriptUnits[i]);

    if (s_ScriptUnits[i].L) {
      lua_atpanic(s_ScriptUnits[i].L, Internal_Lua_Panic);
      luaL_openlibs(s_ScriptUnits[i].L);

      // Register our categorized bindings
      RegisterCoreBindings(s_ScriptUnits[i].L);
      RegisterGraphicsBindings(s_ScriptUnits[i].L);
      RegisterInputBindings(s_ScriptUnits[i].L);
    } else {
      Engine_LogError("Failed to create Lua state for Script Unit %d", i);
      return false;
    }
  }

  Engine_LogInfo("Lua Scripting Manager Initialized (%d units ready)",
                 MAX_SCRIPT_UNITS);
  return true;
}

void Engine_Script_Close(void) {
  for (int i = 0; i < MAX_SCRIPT_UNITS; i++) {
    if (s_ScriptUnits[i].L) {
      lua_close(s_ScriptUnits[i].L);
      s_ScriptUnits[i].L = NULL;
    }
  }
}

int Engine_Script_Load(const void *data, size_t size) {
  for (int i = 0; i < MAX_SCRIPT_UNITS; i++) {
    if (!s_ScriptUnits[i].active) {
      // Bytecode goes to ODD slot: slotIndex + 1
      bool success = Engine_LoadToSlot(ARENA_SCRIPT, s_ScriptUnits[i].slotIndex + 1,
                                       data, size);
      if (success) {
        s_ScriptUnits[i].active = true;
        return i;
      }
    }
  }
  return -1;
}

bool Engine_Script_Run(int unitIndex) {
  if (unitIndex < 0 || unitIndex >= MAX_SCRIPT_UNITS)
    return false;
  ScriptUnit *unit = &s_ScriptUnits[unitIndex];
  if (!unit->L || !unit->active)
    return false;

  // Retrieve the bytecode from the ODD slot
  uint32_t codeSlot = unit->slotIndex + 1;
  void *codePtr = Engine_GetSlot(ARENA_SCRIPT, codeSlot);
  size_t codeSize = Engine_GetSlotCapacity(ARENA_SCRIPT, codeSlot);

  if (luaL_loadbuffer(unit->L, (const char *)codePtr, codeSize, "PS2_Script") !=
      LUA_OK) {
    Engine_LogError("Failed to load Lua buffer from slot %d", codeSlot);
    return false;
  }

  if (lua_pcall(unit->L, 0, 0, 0) != LUA_OK) {
    const char *err = lua_tostring(unit->L, -1);
    Engine_LogError("Lua pcall error: %s", err);
    return false;
  }

  return true;
}

void Engine_Script_UpdateAll(float dt) {
  for (int i = 0; i < MAX_SCRIPT_UNITS; i++) {
    if (s_ScriptUnits[i].active && s_ScriptUnits[i].L) {
      lua_getglobal(s_ScriptUnits[i].L, "OnUpdate");
      if (lua_isfunction(s_ScriptUnits[i].L, -1)) {
        lua_pushnumber(s_ScriptUnits[i].L, dt);
        if (lua_pcall(s_ScriptUnits[i].L, 1, 0, 0) != LUA_OK) {
          const char *err = lua_tostring(s_ScriptUnits[i].L, -1);
          Engine_LogError("Lua Update error in unit %d: %s", i, err);
        }
      } else {
        lua_pop(s_ScriptUnits[i].L, 1);
      }
    }
  }
}

// --- Internal Binding Implementations ---

// Core
static int Lua_Engine_Log(lua_State *L) {
  const char *msg = luaL_checkstring(L, 1);
  Engine_LogInfo("[Lua] %s", msg);
  return 0;
}

static int Lua_Engine_GetTime(lua_State *L) {
  lua_pushnumber(L, GetTime());
  return 1;
}

// Graphics
static int Lua_Graphics_Clear(lua_State *L) {
  if (lua_istable(L, 1)) {
    lua_geti(L, 1, 1);
    unsigned char r = (unsigned char)lua_tointeger(L, -1);
    lua_geti(L, 1, 2);
    unsigned char g = (unsigned char)lua_tointeger(L, -1);
    lua_geti(L, 1, 3);
    unsigned char b = (unsigned char)lua_tointeger(L, -1);
    lua_geti(L, 1, 4);
    unsigned char a = (unsigned char)lua_tointeger(L, -1);
    lua_pop(L, 4);
    ClearBackground((Color){r, g, b, a});
  }
  return 0;
}

static int Lua_Graphics_DrawRect(lua_State *L) {
  float x = (float)luaL_checknumber(L, 1);
  float y = (float)luaL_checknumber(L, 2);
  float w = (float)luaL_checknumber(L, 3);
  float h = (float)luaL_checknumber(L, 4);

  if (lua_istable(L, 5)) {
    lua_geti(L, 5, 1);
    unsigned char r = (unsigned char)lua_tointeger(L, -1);
    lua_geti(L, 5, 2);
    unsigned char g = (unsigned char)lua_tointeger(L, -1);
    lua_geti(L, 5, 3);
    unsigned char b = (unsigned char)lua_tointeger(L, -1);
    lua_geti(L, 5, 4);
    unsigned char a = (unsigned char)lua_tointeger(L, -1);
    lua_pop(L, 4);
    DrawRectangle((int)x, (int)y, (int)w, (int)h, (Color){r, g, b, a});
  }
  return 0;
}

static int Lua_Graphics_DrawCube(lua_State *L) {
  float px = (float)luaL_checknumber(L, 1);
  float py = (float)luaL_checknumber(L, 2);
  float pz = (float)luaL_checknumber(L, 3);
  float sz = (float)luaL_checknumber(L, 4);

  if (lua_istable(L, 5)) {
    lua_geti(L, 5, 1);
    unsigned char r = (unsigned char)lua_tointeger(L, -1);
    lua_geti(L, 5, 2);
    unsigned char g = (unsigned char)lua_tointeger(L, -1);
    lua_geti(L, 5, 3);
    unsigned char b = (unsigned char)lua_tointeger(L, -1);
    lua_geti(L, 5, 4);
    unsigned char a = (unsigned char)lua_tointeger(L, -1);
    lua_pop(L, 4);
    DrawCube((Vector3){px, py, pz}, sz, sz, sz, (Color){r, g, b, a});
  }
  return 0;
}

// Input
static int Lua_Input_IsPadPressed(lua_State *L) {
  const char *btn = luaL_checkstring(L, 1);
  // Map to Raylib gamepad buttons for the PS2
  bool pressed = false;
  if (strcmp(btn, "x") == 0)
    pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
  else if (strcmp(btn, "tri") == 0)
    pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_UP);
  else if (strcmp(btn, "squ") == 0)
    pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
  else if (strcmp(btn, "cir") == 0)
    pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);

  lua_pushboolean(L, pressed);
  return 1;
}

static void RegisterCoreBindings(lua_State *L) {
  lua_newtable(L);
  lua_pushcfunction(L, Lua_Engine_Log);
  lua_setfield(L, -2, "log");
  lua_pushcfunction(L, Lua_Engine_GetTime);
  lua_setfield(L, -2, "get_time");
  lua_setglobal(L, "engine");
}

static void RegisterGraphicsBindings(lua_State *L) {
  lua_newtable(L);
  lua_pushcfunction(L, Lua_Graphics_Clear);
  lua_setfield(L, -2, "clear");
  lua_pushcfunction(L, Lua_Graphics_DrawRect);
  lua_setfield(L, -2, "draw_rect");
  lua_pushcfunction(L, Lua_Graphics_DrawCube);
  lua_setfield(L, -2, "draw_cube");
  lua_setglobal(L, "graphics");
}

static void RegisterInputBindings(lua_State *L) {
  lua_newtable(L);
  lua_pushcfunction(L, Lua_Input_IsPadPressed);
  lua_setfield(L, -2, "is_pad_pressed");
  lua_setglobal(L, "input");
}
