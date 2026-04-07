#include <malloc.h>
#include <rlgl.h>
#include <string.h>
#include "Engine.h"

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>


#define MAX_SCRIPT_UNITS SCRIPTING_LUA_MAX_UNITS // (16 slots = 8 pairs)
static ScriptUnit s_ScriptUnits[MAX_SCRIPT_UNITS];

// Exit callback registered by EngineApp — called when Lua invokes engine.exit().
static void (*s_OnExit)(void) = NULL;


// ---------------------------------------------------------------------------
// Render-mode state machine
// ---------------------------------------------------------------------------
typedef enum
{
    RENDER_MODE_NONE = 0,
    RENDER_MODE_2D,
    RENDER_MODE_3D
} RenderMode;

static RenderMode s_CurrentRenderMode = RENDER_MODE_NONE;
static double s_LastMode3DEnterTime = -1.0;
static double s_LastMode2DEnterTime = -1.0;

// ---------------------------------------------------------------------------
// Camera registry — fixed-size slot arrays with per-slot LFU tracking
// ---------------------------------------------------------------------------
static Camera3D s_Cameras3D[SCRIPTING_MAX_CAMERAS_3D];
static bool s_Camera3DActive[SCRIPTING_MAX_CAMERAS_3D];
static uint32_t s_Camera3DLastUsed[SCRIPTING_MAX_CAMERAS_3D];

static Camera2D s_Cameras2D[SCRIPTING_MAX_CAMERAS_2D];
static bool s_Camera2DActive[SCRIPTING_MAX_CAMERAS_2D];
static uint32_t s_Camera2DLastUsed[SCRIPTING_MAX_CAMERAS_2D];

static uint32_t s_FrameCount = 0;

// Forward declare internal binding registers
static void RegisterCoreBindings(lua_State * L);

static void RegisterGraphicsBindings(lua_State * L);

static void RegisterInputBindings(lua_State * L);

static void RegisterIOBindings(lua_State * L);

static void RegisterResourceBindings(lua_State * L);

static void RegisterLevelBindings(lua_State * L);

// Custom allocator that restricts Lua to her assigned EVEN slot in ARENA_SCRIPT
static void* Engine_Lua_Alloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    ScriptUnit* unit = (ScriptUnit*)ud;

    if (nsize == 0)
    {
        return NULL;
    }

    void* slotBase = Engine_GetSlot(ARENA_SCRIPT, unit->slotIndex);
    size_t slotCapacity = Engine_GetSlotCapacity(ARENA_SCRIPT, unit->slotIndex);

    if (ptr == NULL)
    {
        // Linear allocation within the slot
        size_t alignedOffset = (unit->heapOffset + 7) & ~7;
        if (alignedOffset + nsize > slotCapacity)
        {
            return NULL;
        }

        void* newPtr = (uint8_t*)slotBase + alignedOffset;
        unit->heapOffset = alignedOffset + nsize;
        return newPtr;
    }
    else
    {
        // Realloc: Move to new offset within the same slot
        size_t alignedOffset = (unit->heapOffset + 7) & ~7;
        if (alignedOffset + nsize > slotCapacity)
        {
            return NULL;
        }

        void* newPtr = (uint8_t*)slotBase + alignedOffset;
        memcpy(newPtr, ptr, (osize < nsize) ? osize : nsize);
        unit->heapOffset = alignedOffset + nsize;
        return newPtr;
    }
}

static int Internal_Lua_Panic(lua_State* L)
{
    const char* msg = lua_tostring(L, -1);
    Engine_LogError("LUA PANIC: %s", msg);
    return 0;
}

void Engine_Script_SetExitCallback(void (*onExit)(void)) { s_OnExit = onExit; }

bool Engine_Script_Init(void)
{
    for (int i = 0; i < MAX_SCRIPT_UNITS; i++)
    {
        s_ScriptUnits[i].slotIndex = i * 2;
        s_ScriptUnits[i].heapOffset = 0;
        s_ScriptUnits[i].active = false;

        // Create the Lua State for this unit
        s_ScriptUnits[i].L = lua_newstate(Engine_Lua_Alloc, &s_ScriptUnits[i]);

        if (s_ScriptUnits[i].L)
        {
            lua_atpanic(s_ScriptUnits[i].L, Internal_Lua_Panic);

            // Load only the libraries needed in a sandboxed PS2 game.
            // Skipping io/os/package/debug/coroutine saves ~60KB of heap per unit
            // and avoids "nulldev call" spam from Lua's io lib init (stdin/stdout
            // are null devices on PS2).
            static const luaL_Reg s_Libs[] = {
                {"_G", luaopen_base}, // print, type, pairs, tostring, etc.
                {"math", luaopen_math}, // math.sin, math.sqrt, etc.
                {"string", luaopen_string}, // string.format, string.find, etc.
                {"table", luaopen_table}, // table.insert, table.remove, etc.
                {NULL, NULL}
            };
            for (const luaL_Reg* lib = s_Libs; lib->func; lib++)
            {
                luaL_requiref(s_ScriptUnits[i].L, lib->name, lib->func, 1);
                lua_pop(s_ScriptUnits[i].L, 1);
            }

            // Register our categorized bindings
            RegisterCoreBindings(s_ScriptUnits[i].L);
            RegisterGraphicsBindings(s_ScriptUnits[i].L);
            RegisterInputBindings(s_ScriptUnits[i].L);
            RegisterIOBindings(s_ScriptUnits[i].L);
            RegisterResourceBindings(s_ScriptUnits[i].L);
            RegisterLevelBindings(s_ScriptUnits[i].L);
        }
        else
        {
            Engine_LogError("Failed to create Lua state for Script Unit %d", i);
            Engine_Panic("Lua VM initialization failed — out of memory");
            return false;
        }
    } // end for each script unit

    Engine_LogInfo("Lua Scripting Manager Initialized (%d units ready)", MAX_SCRIPT_UNITS);
    return true;
}

void Engine_Script_Close(void)
{
    for (int i = 0; i < MAX_SCRIPT_UNITS; i++)
    {
        if (s_ScriptUnits[i].L)
        {
            lua_close(s_ScriptUnits[i].L);
            s_ScriptUnits[i].L = NULL;
        }
    }

    // Reset render-mode and camera state
    s_CurrentRenderMode = RENDER_MODE_NONE;
    s_FrameCount = 0;
    for (int i = 0; i < SCRIPTING_MAX_CAMERAS_3D; i++)
        s_Camera3DActive[i] = false;
    for (int i = 0; i < SCRIPTING_MAX_CAMERAS_2D; i++)
        s_Camera2DActive[i] = false;
}

int Engine_Script_Load(const void* data, size_t size)
{
    for (int i = 0; i < MAX_SCRIPT_UNITS; i++)
    {
        if (!s_ScriptUnits[i].active)
        {
            // Bytecode goes to ODD slot: slotIndex + 1
            bool success = Engine_LoadToSlot(ARENA_SCRIPT, s_ScriptUnits[i].slotIndex + 1, data, size);
            if (success)
            {
                s_ScriptUnits[i].codeSize = size;
                s_ScriptUnits[i].active = true;
                return i;
            }
        }
    }
    return -1;
}

bool Engine_Script_Run(int unitIndex)
{
    if (unitIndex < 0 || unitIndex >= MAX_SCRIPT_UNITS)
        return false;
    ScriptUnit* unit = &s_ScriptUnits[unitIndex];
    if (!unit->L || !unit->active)
        return false;

    // Retrieve the bytecode from the ODD slot using the actual stored size
    uint32_t codeSlot = unit->slotIndex + 1;
    const char* code = (const char*)Engine_GetSlot(ARENA_SCRIPT, codeSlot);
    size_t codeSize = unit->codeSize;

    // Strip UTF-8 BOM (0xEF 0xBB 0xBF) — Windows editors prepend this to text
    // files. Lua does not understand the BOM and returns LUA_ERRSYNTAX without it.
    if (codeSize >= 3 && (unsigned char)code[0] == 0xEF && (unsigned char)code[1] == 0xBB &&
        (unsigned char)code[2] == 0xBF)
    {
        code += 3;
        codeSize -= 3;
    }

    if (luaL_loadbuffer(unit->L, code, codeSize, "PS2_Script") != LUA_OK)
    {
        const char* err = lua_tostring(unit->L, -1);
        Engine_LogError("Lua compile error in slot %d: %s", codeSlot, err ? err : "(unknown)");
        Engine_Panic("Lua script failed to compile");
        return false;
    }

    if (lua_pcall(unit->L, 0, 0, 0) != LUA_OK)
    {
        const char* err = lua_tostring(unit->L, -1);
        Engine_LogError("Lua runtime error in unit %d: %s", unitIndex, err ? err : "(unknown)");
        Engine_Panic("Lua script failed to execute");
        return false;
    }

    return true;
}

void Engine_Script_UpdateAll(float dt)
{
    for (int i = 0; i < MAX_SCRIPT_UNITS; i++)
    {
        if (s_ScriptUnits[i].active && s_ScriptUnits[i].L)
        {
            lua_getglobal(s_ScriptUnits[i].L, "OnUpdate");
            if (lua_isfunction(s_ScriptUnits[i].L, -1))
            {
                lua_pushnumber(s_ScriptUnits[i].L, dt);
                if (lua_pcall(s_ScriptUnits[i].L, 1, 0, 0) != LUA_OK)
                {
                    const char* err = lua_tostring(s_ScriptUnits[i].L, -1);
                    Engine_LogError("Lua Update error in unit %d: %s", i, err);
                }
            }
            else
            {
                lua_pop(s_ScriptUnits[i].L, 1);
            }
        }
    }
}

void Engine_Script_EndCurrentMode(void)
{
    if (s_CurrentRenderMode == RENDER_MODE_3D)
    {
        EndMode3D();
    }
    else if (s_CurrentRenderMode == RENDER_MODE_2D)
    {
        EndMode2D();
    }
    s_CurrentRenderMode = RENDER_MODE_NONE;
}

void Engine_Script_FrameTick(void)
{
    s_FrameCount++;

    // Evict any 3D camera slot that has been idle for too long.
    for (int i = 0; i < SCRIPTING_MAX_CAMERAS_3D; i++)
    {
        if (s_Camera3DActive[i] && (s_FrameCount - s_Camera3DLastUsed[i]) >= SCRIPTING_CAM_IDLE_FRAMES_EVICT)
        {
            Engine_LogInfo("[Script] Camera3D slot %d evicted after %u idle frames", i,
                           s_FrameCount - s_Camera3DLastUsed[i]);
            s_Camera3DActive[i] = false;
        }
    }

    // Evict any 2D camera slot that has been idle for too long.
    for (int i = 0; i < SCRIPTING_MAX_CAMERAS_2D; i++)
    {
        if (s_Camera2DActive[i] && (s_FrameCount - s_Camera2DLastUsed[i]) >= SCRIPTING_CAM_IDLE_FRAMES_EVICT)
        {
            Engine_LogInfo("[Script] Camera2D slot %d evicted after %u idle frames", i,
                           s_FrameCount - s_Camera2DLastUsed[i]);
            s_Camera2DActive[i] = false;
        }
    }
}

// --- Internal Binding Implementations ---

// ---------------------------------------------------------------------------
// Camera creation bindings
// ---------------------------------------------------------------------------

// graphics.make_camera_3d(pos_x, pos_y, pos_z,
//                         target_x, target_y, target_z,
//                         up_x, up_y, up_z,
//                         fovy, projection)   -> handle (int) or -1 on error
// projection: 0 = CAMERA_PERSPECTIVE, 1 = CAMERA_ORTHOGRAPHIC
static int Lua_Graphics_MakeCamera3D(lua_State* L)
{
    // Find a free slot
    int slot = -1;
    for (int i = 0; i < SCRIPTING_MAX_CAMERAS_3D; i++)
    {
        if (!s_Camera3DActive[i])
        {
            slot = i;
            break;
        }
    }
    if (slot < 0)
    {
        Engine_LogError("[Script] make_camera_3d: no free slots (max %d)", SCRIPTING_MAX_CAMERAS_3D);
        lua_pushinteger(L, -1);
        return 1;
    }

    Camera3D cam;
    cam.position =
        (Vector3)
    {
        (float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3)
    };
    cam.target =
        (Vector3)
    {
        (float)luaL_checknumber(L, 4), (float)luaL_checknumber(L, 5), (float)luaL_checknumber(L, 6)
    };
    cam.up = (Vector3)
    {
        (float)luaL_checknumber(L, 7), (float)luaL_checknumber(L, 8), (float)luaL_checknumber(L, 9)
    };
    cam.fovy = (float)luaL_checknumber(L, 10);
    cam.projection = (int)luaL_checkinteger(L, 11);

    s_Cameras3D[slot] = cam;
    s_Camera3DActive[slot] = true;
    s_Camera3DLastUsed[slot] = s_FrameCount;

    lua_pushinteger(L, slot);
    return 1;
}

// graphics.make_camera_2d(offset_x, offset_y,
//                         target_x, target_y,
//                         rotation, zoom)      -> handle (int) or -1 on error
static int Lua_Graphics_MakeCamera2D(lua_State* L)
{
    int slot = -1;
    for (int i = 0; i < SCRIPTING_MAX_CAMERAS_2D; i++)
    {
        if (!s_Camera2DActive[i])
        {
            slot = i;
            break;
        }
    }
    if (slot < 0)
    {
        Engine_LogError("[Script] make_camera_2d: no free slots (max %d)", SCRIPTING_MAX_CAMERAS_2D);
        lua_pushinteger(L, -1);
        return 1;
    }

    Camera2D cam;
    cam.offset = (Vector2)
    {
        (float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2)
    };
    cam.target = (Vector2)
    {
        (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4)
    };
    cam.rotation = (float)luaL_checknumber(L, 5);
    cam.zoom = (float)luaL_checknumber(L, 6);

    s_Cameras2D[slot] = cam;
    s_Camera2DActive[slot] = true;
    s_Camera2DLastUsed[slot] = s_FrameCount;

    lua_pushinteger(L, slot);
    return 1;
}

// ---------------------------------------------------------------------------
// Mode-switching bindings
// ---------------------------------------------------------------------------

// graphics.begin_mode_3d(handle)
// Re-entry guard: if already in 3D and < SCRIPTING_MODE_REENTRY_COOLDOWN_SEC
// has elapsed since the last entry, the call is a no-op to avoid redundant
// EndMode3D/BeginMode3D pairs. Otherwise re-enters with the supplied camera.
static int Lua_Graphics_BeginMode3D(lua_State* L)
{
    int32_t handle = (int32_t)luaL_checkinteger(L, 1);

    if (handle < 0 || handle >= SCRIPTING_MAX_CAMERAS_3D || !s_Camera3DActive[handle])
    {
        Engine_LogError("[Script] begin_mode_3d: invalid or evicted handle %d", handle);
        return 0;
    }

    if (s_CurrentRenderMode == RENDER_MODE_3D)
    {
        double elapsed = GetTime() - s_LastMode3DEnterTime;
        if (elapsed < SCRIPTING_MODE_REENTRY_COOLDOWN_SEC)
        {
            // No-op: too soon to re-enter.
            return 0;
        }
        // Re-enter: close current block first.
        EndMode3D();
    }
    else if (s_CurrentRenderMode == RENDER_MODE_2D)
    {
        EndMode2D();
    }

    BeginMode3D(s_Cameras3D[handle]);
    s_CurrentRenderMode = RENDER_MODE_3D;
    s_LastMode3DEnterTime = GetTime();
    s_Camera3DLastUsed[handle] = s_FrameCount;
    return 0;
}

// graphics.begin_mode_2d(handle)
// Symmetric re-entry guard matching begin_mode_3d.
static int Lua_Graphics_BeginMode2D(lua_State* L)
{
    int32_t handle = (int32_t)luaL_checkinteger(L, 1);

    if (handle < 0 || handle >= SCRIPTING_MAX_CAMERAS_2D || !s_Camera2DActive[handle])
    {
        Engine_LogError("[Script] begin_mode_2d: invalid or evicted handle %d", handle);
        return 0;
    }

    if (s_CurrentRenderMode == RENDER_MODE_2D)
    {
        double elapsed = GetTime() - s_LastMode2DEnterTime;
        if (elapsed < SCRIPTING_MODE_REENTRY_COOLDOWN_SEC)
        {
            return 0;
        }
        EndMode2D();
    }
    else if (s_CurrentRenderMode == RENDER_MODE_3D)
    {
        EndMode3D();
    }

    BeginMode2D(s_Cameras2D[handle]);
    s_CurrentRenderMode = RENDER_MODE_2D;
    s_LastMode2DEnterTime = GetTime();
    s_Camera2DLastUsed[handle] = s_FrameCount;
    return 0;
}

// Core
static int Lua_Engine_Log(lua_State* L)
{
    const char* msg = luaL_checkstring(L, 1);
    Engine_LogInfo("[Lua] %s", msg);
    return 0;
}

static int Lua_Engine_GetTime(lua_State* L)
{
    lua_pushnumber(L, GetTime());
    return 1;
}

static int Lua_Engine_Exit(lua_State* L)
{
    UNUSED_VAR(L);
    if (s_OnExit)
    {
        s_OnExit();
    }
    else
    {
        Engine_LogError("engine.exit() called but no exit callback is registered");
    }
    return 0;
}

// Graphics
static int Lua_Graphics_Clear(lua_State* L)
{
    if (lua_istable(L, 1))
    {
        lua_geti(L, 1, 1);
        unsigned char r = (unsigned char)lua_tointeger(L, -1);
        lua_geti(L, 1, 2);
        unsigned char g = (unsigned char)lua_tointeger(L, -1);
        lua_geti(L, 1, 3);
        unsigned char b = (unsigned char)lua_tointeger(L, -1);
        lua_geti(L, 1, 4);
        unsigned char a = (unsigned char)lua_tointeger(L, -1);
        lua_pop(L, 4);
        ClearBackground((Color)
        {
            r, g, b, a
        }
        )
        ;
    }
    return 0;
}

static int Lua_Graphics_DrawRect(lua_State* L)
{
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float w = (float)luaL_checknumber(L, 3);
    float h = (float)luaL_checknumber(L, 4);

    if (lua_istable(L, 5))
    {
        lua_geti(L, 5, 1);
        unsigned char r = (unsigned char)lua_tointeger(L, -1);
        lua_geti(L, 5, 2);
        unsigned char g = (unsigned char)lua_tointeger(L, -1);
        lua_geti(L, 5, 3);
        unsigned char b = (unsigned char)lua_tointeger(L, -1);
        lua_geti(L, 5, 4);
        unsigned char a = (unsigned char)lua_tointeger(L, -1);
        lua_pop(L, 4);
        DrawRectangle((int)x, (int)y, (int)w, (int)h, (Color)
        {
            r, g, b, a
        }
        )
        ;
    }
    return 0;
}

static int Lua_Graphics_DrawCube(lua_State* L)
{
    float px = (float)luaL_checknumber(L, 1);
    float py = (float)luaL_checknumber(L, 2);
    float pz = (float)luaL_checknumber(L, 3);
    float sz = (float)luaL_checknumber(L, 4);

    if (lua_istable(L, 5))
    {
        lua_geti(L, 5, 1);
        unsigned char r = (unsigned char)lua_tointeger(L, -1);
        lua_geti(L, 5, 2);
        unsigned char g = (unsigned char)lua_tointeger(L, -1);
        lua_geti(L, 5, 3);
        unsigned char b = (unsigned char)lua_tointeger(L, -1);
        lua_geti(L, 5, 4);
        unsigned char a = (unsigned char)lua_tointeger(L, -1);
        lua_pop(L, 4);
        DrawCube((Vector3)
        {
            px, py, pz
        }
        ,
        sz, sz, sz, (Color)
        {
            r, g, b, a
        }
        )
        ;
    }
    return 0;
}

static int Lua_Graphics_DrawGrid(lua_State* L)
{
    int slices = (int)luaL_checknumber(L, 1);
    float spacing = (float)luaL_checknumber(L, 2);

    DrawGrid(slices, spacing);
    return 0;
}

// graphics.draw_cube_textured(x, y, z, size, handle, {r,g,b,a})
// Draws a textured cube using rlgl immediate mode (glBegin/glVertex3f/glTexCoord2f).
//
// rlgl vs DrawModel:
//   Raylib is built for PS2 with GRAPHICS_API_OPENGL_11, and DrawModel/DrawMesh uses
//   glEnableClientState + glVertexPointer + glDrawElements (the vertex-array pipeline).
//   ps2gl does NOT implement those functions. It only supports the immediate mode pipeline
//   (glBegin/glEnd/glVertex3f/glTexCoord2f/etc.), which is the same path used by DrawCube
//   and DrawGrid. This is a reimplementation of Raylib's removed DrawCubeTextured() that
//   was dropped in v4.0 — brought back here as an rlgl immediate-mode draw.
static int Lua_Graphics_DrawCubeTextured(lua_State* L)
{
    float px = (float)luaL_checknumber(L, 1);
    float py = (float)luaL_checknumber(L, 2);
    float pz = (float)luaL_checknumber(L, 3);
    float sz = (float)luaL_checknumber(L, 4);
    int32_t handle = (int32_t)luaL_checkinteger(L, 5);

    Texture2D* tex = (Texture2D*)Engine_Resource_Get(handle);
    if (!tex || tex->id == 0)
    {
        // Resource not ready or GPU upload failed — fall back to a solid draw.
        DrawCube((Vector3)
        {
            px, py, pz
        }
        ,
        sz, sz, sz, WHITE
        )
        ;
        return 0;
    }

    Color tint = WHITE;
    if (lua_istable(L, 6))
    {
        lua_geti(L, 6, 1);
        unsigned char r = (unsigned char)lua_tointeger(L, -1);
        lua_geti(L, 6, 2);
        unsigned char g = (unsigned char)lua_tointeger(L, -1);
        lua_geti(L, 6, 3);
        unsigned char b = (unsigned char)lua_tointeger(L, -1);
        lua_geti(L, 6, 4);
        unsigned char a = (unsigned char)lua_tointeger(L, -1);
        lua_pop(L, 4);
        tint = (Color)
        {
            r, g, b, a
        };
    }

    float h = sz / 2.0f;

    rlSetTexture(tex->id);
    rlBegin(RL_QUADS);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);

    // Front face (+Z)
    rlNormal3f(0.0f, 0.0f, 1.0f);
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(px - h, py - h, pz + h);
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(px + h, py - h, pz + h);
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(px + h, py + h, pz + h);
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(px - h, py + h, pz + h);

    // Back face (-Z)
    rlNormal3f(0.0f, 0.0f, -1.0f);
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(px - h, py - h, pz - h);
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(px - h, py + h, pz - h);
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(px + h, py + h, pz - h);
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(px + h, py - h, pz - h);

    // Top face (+Y)
    rlNormal3f(0.0f, 1.0f, 0.0f);
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(px - h, py + h, pz - h);
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(px - h, py + h, pz + h);
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(px + h, py + h, pz + h);
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(px + h, py + h, pz - h);

    // Bottom face (-Y)
    rlNormal3f(0.0f, -1.0f, 0.0f);
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(px - h, py - h, pz - h);
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(px + h, py - h, pz - h);
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(px + h, py - h, pz + h);
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(px - h, py - h, pz + h);

    // Right face (+X)
    rlNormal3f(1.0f, 0.0f, 0.0f);
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(px + h, py - h, pz - h);
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(px + h, py + h, pz - h);
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(px + h, py + h, pz + h);
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(px + h, py - h, pz + h);

    // Left face (-X)
    rlNormal3f(-1.0f, 0.0f, 0.0f);
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(px - h, py - h, pz - h);
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(px - h, py - h, pz + h);
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(px - h, py + h, pz + h);
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(px - h, py + h, pz - h);
    rlEnd();
    rlSetTexture(0);
    return 0;
}

// Input
static int Lua_Input_IsPadPressed(lua_State* L)
{
    const char* btn = luaL_checkstring(L, 1);
    bool pressed = false;

    // Face buttons
    if (strcmp(btn, "x") == 0)
        pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    else if (strcmp(btn, "cir") == 0)
        pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    else if (strcmp(btn, "squ") == 0)
        pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
    else if (strcmp(btn, "tri") == 0)
        pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_UP);
        // D-pad
    else if (strcmp(btn, "dpad_up") == 0)
        pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_UP);
    else if (strcmp(btn, "dpad_down") == 0)
        pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
    else if (strcmp(btn, "dpad_left") == 0)
        pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
    else if (strcmp(btn, "dpad_right") == 0)
        pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
        // Shoulder buttons
    else if (strcmp(btn, "l1") == 0)
        pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
    else if (strcmp(btn, "l2") == 0)
        pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_2);
    else if (strcmp(btn, "r1") == 0)
        pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
    else if (strcmp(btn, "r2") == 0)
        pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_2);
        // Stick clicks
    else if (strcmp(btn, "l3") == 0)
        pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_THUMB);
    else if (strcmp(btn, "r3") == 0)
        pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_THUMB);
        // Menu
    else if (strcmp(btn, "start") == 0)
        pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_MIDDLE_RIGHT);
    else if (strcmp(btn, "select") == 0)
        pressed = IsGamepadButtonDown(0, GAMEPAD_BUTTON_MIDDLE_LEFT);
    else
        Engine_LogError("[Lua] input.is_pad_pressed: unknown button '%s'", btn);

    lua_pushboolean(L, pressed);
    return 1;
}

static void RegisterCoreBindings(lua_State* L)
{
    lua_newtable(L);
    lua_pushcfunction(L, Lua_Engine_Log);
    lua_setfield(L, -2, "log");
    lua_pushcfunction(L, Lua_Engine_GetTime);
    lua_setfield(L, -2, "get_time");
    lua_pushcfunction(L, Lua_Engine_Exit);
    lua_setfield(L, -2, "exit");
    lua_setglobal(L, "engine");
}

static void RegisterGraphicsBindings(lua_State* L)
{
    lua_newtable(L);
    lua_pushcfunction(L, Lua_Graphics_Clear);
    lua_setfield(L, -2, "clear");
    lua_pushcfunction(L, Lua_Graphics_DrawRect);
    lua_setfield(L, -2, "draw_rect");
    lua_pushcfunction(L, Lua_Graphics_DrawCube);
    lua_setfield(L, -2, "draw_cube");
    lua_pushcfunction(L, Lua_Graphics_DrawGrid);
    lua_setfield(L, -2, "draw_grid");
    lua_pushcfunction(L, Lua_Graphics_DrawCubeTextured);
    lua_setfield(L, -2, "draw_cube_textured");
    lua_pushcfunction(L, Lua_Graphics_MakeCamera3D);
    lua_setfield(L, -2, "make_camera_3d");
    lua_pushcfunction(L, Lua_Graphics_MakeCamera2D);
    lua_setfield(L, -2, "make_camera_2d");
    lua_pushcfunction(L, Lua_Graphics_BeginMode3D);
    lua_setfield(L, -2, "begin_mode_3d");
    lua_pushcfunction(L, Lua_Graphics_BeginMode2D);
    lua_setfield(L, -2, "begin_mode_2d");
    lua_setglobal(L, "graphics");
}

static void RegisterInputBindings(lua_State* L)
{
    lua_newtable(L);
    lua_pushcfunction(L, Lua_Input_IsPadPressed);
    lua_setfield(L, -2, "is_pad_pressed");
    lua_setglobal(L, "input");
}

// ---------------------------------------------------------------------------
// IO bindings  — io.*
// All file data stays C-side; Lua holds only integer fileIds.
// ---------------------------------------------------------------------------

// Forward declarations of EngineApp file functions (defined in EngineApp.c).
extern int32_t EngineApp_FileOpen(const char* path);

extern int32_t EngineApp_FileOpenWrite(const char* path);

extern size_t EngineApp_FileRead(int32_t fileId, const void** outData);

extern size_t EngineApp_FileWrite(int32_t fileId, const void* data, size_t size);

extern bool EngineApp_FileClose(int32_t fileId);

extern size_t EngineApp_FileGetSize(int32_t fileId);

static int Lua_IO_Open(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    int32_t fd = EngineApp_FileOpen(path);
    lua_pushinteger(L, (lua_Integer)fd);
    return 1;
}

static int Lua_IO_OpenWrite(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    int32_t fd = EngineApp_FileOpenWrite(path);
    lua_pushinteger(L, (lua_Integer)fd);
    return 1;
}

static int Lua_IO_Close(lua_State* L)
{
    int32_t fd = (int32_t)luaL_checkinteger(L, 1);
    bool ok = EngineApp_FileClose(fd);
    lua_pushboolean(L, ok);
    return 1;
}

static int Lua_IO_GetSize(lua_State* L)
{
    int32_t fd = (int32_t)luaL_checkinteger(L, 1);
    size_t sz = EngineApp_FileGetSize(fd);
    lua_pushinteger(L, (lua_Integer)sz);
    return 1;
}

// io.read(fileId) -> string, number
// Returns the file content as a Lua string and the byte count.
// Intended for small text files only (config, dialogue).
static int Lua_IO_Read(lua_State* L)
{
    int32_t fd = (int32_t)luaL_checkinteger(L, 1);
    const void* data = NULL;
    size_t bytesRead = EngineApp_FileRead(fd, &data);
    if (bytesRead == 0 || !data)
    {
        lua_pushnil(L);
        lua_pushinteger(L, 0);
        return 2;
    }
    lua_pushlstring(L, (const char*)data, bytesRead);
    lua_pushinteger(L, (lua_Integer)bytesRead);
    return 2;
}

// io.write(fileId, content) -> number (bytes written; 0 on error)
static int Lua_IO_Write(lua_State* L)
{
    int32_t fd = (int32_t)luaL_checkinteger(L, 1);
    size_t dataLen = 0;
    const char* data = luaL_checklstring(L, 2, &dataLen);
    size_t written = EngineApp_FileWrite(fd, data, dataLen);
    lua_pushinteger(L, (lua_Integer)written);
    return 1;
}

static void RegisterIOBindings(lua_State* L)
{
    lua_newtable(L);
    lua_pushcfunction(L, Lua_IO_Open);
    lua_setfield(L, -2, "open");
    lua_pushcfunction(L, Lua_IO_OpenWrite);
    lua_setfield(L, -2, "open_write");
    lua_pushcfunction(L, Lua_IO_Close);
    lua_setfield(L, -2, "close");
    lua_pushcfunction(L, Lua_IO_GetSize);
    lua_setfield(L, -2, "get_size");
    lua_pushcfunction(L, Lua_IO_Read);
    lua_setfield(L, -2, "read");
    lua_pushcfunction(L, Lua_IO_Write);
    lua_setfield(L, -2, "write");
    lua_setglobal(L, "io");
}

// ---------------------------------------------------------------------------
// Resource bindings  — resources.*
// ---------------------------------------------------------------------------
static int Lua_Resources_Load(lua_State* L)
{
    const char* typeStr = luaL_checkstring(L, 1);
    const char* path = luaL_checkstring(L, 2);

    ResourceType type = RES_TEXTURE;
    if (strcmp(typeStr, "TEXTURE") == 0)
        type = RES_TEXTURE;
    else if (strcmp(typeStr, "MODEL") == 0)
        type = RES_MODEL;
    else if (strcmp(typeStr, "SOUND") == 0)
        type = RES_SOUND;
    else if (strcmp(typeStr, "FONT") == 0)
        type = RES_FONT;
    else
        Engine_LogError("[Lua] resources.load: unknown type '%s'", typeStr);

    int32_t handle = Engine_Resource_Load(type, path);
    lua_pushinteger(L, (lua_Integer)handle);
    return 1;
}

static int Lua_Resources_Unload(lua_State* L)
{
    int32_t handle = (int32_t)luaL_checkinteger(L, 1);
    Engine_Resource_Unload(handle);
    return 0;
}

static int Lua_Resources_IsReady(lua_State* L)
{
    int32_t handle = (int32_t)luaL_checkinteger(L, 1);
    lua_pushboolean(L, Engine_Resource_IsReady(handle));
    return 1;
}

static void RegisterResourceBindings(lua_State* L)
{
    lua_newtable(L);
    lua_pushcfunction(L, Lua_Resources_Load);
    lua_setfield(L, -2, "load");
    lua_pushcfunction(L, Lua_Resources_Unload);
    lua_setfield(L, -2, "unload");
    lua_pushcfunction(L, Lua_Resources_IsReady);
    lua_setfield(L, -2, "is_ready");
    lua_setglobal(L, "resources");
}

// ---------------------------------------------------------------------------
// Level bindings  — level.*
// Stubs: logged at runtime; implementation pending .ps2l deserialisation.
// ---------------------------------------------------------------------------
static int Lua_Level_Load(lua_State* L)
{
    Engine_LogError("Not implemented: %s", __func__);
    lua_pushboolean(L, 0);
    return 1;
}

static int Lua_Level_Unload(lua_State* L)
{
    Engine_LogError("Not implemented: %s", __func__);
    lua_pushboolean(L, 0);
    return 1;
}

static void RegisterLevelBindings(lua_State* L)
{
    lua_newtable(L);
    lua_pushcfunction(L, Lua_Level_Load);
    lua_setfield(L, -2, "load");
    lua_pushcfunction(L, Lua_Level_Unload);
    lua_setfield(L, -2, "unload");
    lua_setglobal(L, "level");
}
