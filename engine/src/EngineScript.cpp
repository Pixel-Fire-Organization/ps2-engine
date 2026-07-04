#include "EngineScript.h"
#include "EngineCore.h"
#include "EngineInput.h"
#include <malloc.h>
#include <cstring>
#include <cmath>
#include "EngineApp.h"
#include "EngineMemory.h"
#include "EngineResource.h"
#include "Macros.h"

#define LUA_USE_C89
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}


#define MAX_SCRIPT_UNITS SCRIPTING_LUA_MAX_UNITS // (16 slots = 8 pairs)
static ScriptUnit s_ScriptUnits[MAX_SCRIPT_UNITS];

// Exit callback registered by EngineApp — called when Lua invokes engine.exit().
static ExitCallback s_OnExit = nullptr;


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

// ---------------------------------------------------------------------------
// Camera registry — a fixed set of hardcoded slots. Slots are never evicted;
// scripts configure/move/rotate them and pick which one is the active (rendered)
// 3D camera. Exactly one 3D camera is active per frame. s_Camera*Count is how
// many make_camera_* calls have been issued (bounded by the slot count).
// ---------------------------------------------------------------------------
static Camera3D s_Cameras3D[SCRIPTING_MAX_CAMERAS_3D];
static int s_Camera3DCount = 0;

static Camera2D s_Cameras2D[SCRIPTING_MAX_CAMERAS_2D];
static int s_Camera2DCount = 0;

static uint32_t s_FrameCount = 0;

// Forward declare internal binding registers
static void RegisterCoreBindings(lua_State* L);

static void RegisterGraphicsBindings(lua_State* L);

static void RegisterInputBindings(lua_State* L);

static void RegisterIOBindings(lua_State* L);

static void RegisterResourceBindings(lua_State* L);

static void RegisterLevelBindings(lua_State* L);

static void RegisterMathExtensions(lua_State* L);

// ---------------------------------------------------------------------------
// Minimal first-fit free-list heap — runs entirely within a pre-allocated
// arena slot so Lua's GC can actually reclaim memory (the old bump allocator
// returned NULL for free/realloc but never reclaimed space, causing OOM after
// a few seconds of OnUpdate creating tables).
// ---------------------------------------------------------------------------
typedef struct
{
    uint32_t size; // Data region size (bytes), excluding this header
    uint32_t free; // 1 = free, 0 = in-use
} BlockHeader;

// Header is always 8 bytes — keeps data 8-byte aligned on every alloc.
#define HEAP_HEADER_SIZE ((size_t)sizeof(BlockHeader))
// Minimum remainder to bother splitting a block (header + at least 8 bytes)
#define HEAP_MIN_SPLIT (HEAP_HEADER_SIZE + 8u)

static void Heap_Init(void* base, size_t capacity)
{
    BlockHeader* first = static_cast<BlockHeader*>(base);
    first->size = static_cast<uint32_t>(capacity - HEAP_HEADER_SIZE);
    first->free = 1;
}

static void* Heap_Alloc(void* base, size_t capacity, size_t nsize)
{
    nsize = (nsize + 7u) & ~7u; // 8-byte align
    uint8_t* cursor = static_cast<uint8_t*>(base);
    uint8_t* end = cursor + capacity;

    while (cursor + HEAP_HEADER_SIZE <= end)
    {
        BlockHeader* block = reinterpret_cast<BlockHeader *>(cursor);
        if (block->free && static_cast<size_t>(block->size) >= nsize)
        {
            size_t remainder = static_cast<size_t>(block->size) - nsize;
            if (remainder >= HEAP_MIN_SPLIT)
            {
                // Split: carve a new free block from the tail
                BlockHeader* next = reinterpret_cast<BlockHeader *>(cursor + HEAP_HEADER_SIZE + nsize);
                next->size = static_cast<uint32_t>(remainder - HEAP_HEADER_SIZE);
                next->free = 1;
                block->size = static_cast<uint32_t>(nsize);
            }
            block->free = 0;
            return cursor + HEAP_HEADER_SIZE;
        }
        cursor += HEAP_HEADER_SIZE + static_cast<size_t>(block->size);
    }
    return nullptr; // OOM
}

static void Heap_Free(void* base, size_t capacity, void* ptr)
{
    if (!ptr)
        return;

    BlockHeader* block = reinterpret_cast<BlockHeader *>(static_cast<uint8_t *>(ptr) - HEAP_HEADER_SIZE);
    block->free = 1;

    // Forward coalescing: merge contiguous free blocks to reduce fragmentation
    uint8_t* next = (uint8_t*)block + HEAP_HEADER_SIZE + static_cast<size_t>(block->size);
    uint8_t* end = static_cast<uint8_t*>(base) + capacity;
    while (next + HEAP_HEADER_SIZE <= end)
    {
        BlockHeader* nextBlock = reinterpret_cast<BlockHeader *>(next);
        if (!nextBlock->free)
            break;
        block->size += static_cast<uint32_t>((HEAP_HEADER_SIZE + static_cast<size_t>(nextBlock->size)));
        next = (uint8_t*)block + HEAP_HEADER_SIZE + static_cast<size_t>(block->size);
    }

    // Backward coalescing: the blocks are an implicit list with no back-pointer,
    // so walk from base to find `block`'s predecessor and absorb `block` into it
    // if it is free. Without this, freeing in forward order leaves the heap
    // fragmented and Heap_Alloc can fail with free space still available.
    uint8_t* cursor = static_cast<uint8_t*>(base);
    BlockHeader* prev = nullptr;
    while (cursor + HEAP_HEADER_SIZE <= end)
    {
        BlockHeader* cur = reinterpret_cast<BlockHeader *>(cursor);
        if (cur == block)
            break;
        prev = cur;
        cursor += HEAP_HEADER_SIZE + static_cast<size_t>(cur->size);
    }
    if (prev && prev->free)
        prev->size += static_cast<uint32_t>(HEAP_HEADER_SIZE + static_cast<size_t>(block->size));
}

static void* Heap_Realloc(void* base, size_t capacity, void* ptr, size_t osize, size_t nsize)
{
    BlockHeader* block = reinterpret_cast<BlockHeader *>(static_cast<uint8_t *>(ptr) - HEAP_HEADER_SIZE);
    size_t alignedN = (nsize + 7u) & ~7u;

    if (static_cast<size_t>(block->size) >= alignedN)
        return ptr; // Fits in place — no copy needed

    void* newPtr = Heap_Alloc(base, capacity, nsize);
    if (!newPtr)
        return nullptr;

    memmove(newPtr, ptr, (osize < nsize) ? osize : nsize);
    Heap_Free(base, capacity, ptr);
    return newPtr;
}

// Custom allocator that restricts Lua to her assigned EVEN slot in ARENA_SCRIPT
static void* Engine_Lua_Alloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    ScriptUnit* unit = static_cast<ScriptUnit*>(ud);

    void* slotBase = Engine_GetSlot(ARENA_SCRIPT, unit->slotIndex);
    size_t slotCapacity = Engine_GetSlotCapacity(ARENA_SCRIPT, unit->slotIndex);

    // Guard: if the slot base is NULL the arena was never initialised.
    // Returning NULL here prevents Lua from writing its state to address 0x0,
    // which would corrupt the PS2 exception-vector table.
    if (!slotBase || !slotCapacity)
        return nullptr;

    // Lazy-init: place one free block spanning the entire slot on first use.
    if (!unit->heapReady)
    {
        Heap_Init(slotBase, slotCapacity);
        unit->heapReady = true;
    }

    if (nsize == 0)
    {
        Heap_Free(slotBase, slotCapacity, ptr);
        return nullptr;
    }

    if (ptr == nullptr)
        return Heap_Alloc(slotBase, slotCapacity, nsize);

    return Heap_Realloc(slotBase, slotCapacity, ptr, osize, nsize);
}

static int Internal_Lua_Panic(lua_State* L)
{
    const char* msg = lua_tostring(L, -1);
    Engine_LogError("LUA PANIC: %s", msg);
    return 0;
}

void Engine_Script_SetExitCallback(ExitCallback onExit) { s_OnExit = onExit; }

bool Engine_Script_Init()
{
    for (int i = 0; i < MAX_SCRIPT_UNITS; i++)
    {
        s_ScriptUnits[i].slotIndex = i * 2;
        s_ScriptUnits[i].heapReady = false;
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
            static const luaL_Reg s_Libs[] = {{"_G", luaopen_base}, // print, type, pairs, tostring, etc.
                                              {"math", luaopen_math}, // math.sin, math.sqrt, etc.
                                              {"string", luaopen_string}, // string.format, string.find, etc.
                                              {"table", luaopen_table}, // table.insert, table.remove, etc.
                                              {nullptr, nullptr}};
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

            // Inject performance extensions into the standard math table.
            RegisterMathExtensions(s_ScriptUnits[i].L);

            // Switch to generational GC (Lua 5.4).
            // Generational mode is far better for a game loop:
            //   - Minor cycle: only scans short-lived (young) objects — very cheap.
            //   - Major cycle: full mark-sweep, triggered when old gen grows significantly.
            // Scripts that allocate nothing per frame (e.g. STRESSDRAW) pay zero GC cost.
            // Scripts that allocate a few small tables per frame (e.g. MAIN) pay only cheap
            // minor cycles. minormul=20: minor GC when young-gen memory grows 20%;
            // majormul=100: major GC when old-gen doubles.
            lua_gc(s_ScriptUnits[i].L, LUA_GCGEN, 20, 100);
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

void Engine_Script_Close()
{
    for (int i = 0; i < MAX_SCRIPT_UNITS; i++)
    {
        if (s_ScriptUnits[i].L)
        {
            lua_close(s_ScriptUnits[i].L);
            s_ScriptUnits[i].L = nullptr;
        }
    }

    // Reset render-mode and camera state
    s_CurrentRenderMode = RENDER_MODE_NONE;
    s_FrameCount = 0;
    s_Camera3DCount = 0;
    s_Camera2DCount = 0;
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
    const char* code = static_cast<const char*>(Engine_GetSlot(ARENA_SCRIPT, codeSlot));
    size_t codeSize = unit->codeSize;

    // Strip UTF-8 BOM (0xEF 0xBB 0xBF) — Windows editors prepend this to text
    // files. Lua does not understand the BOM and returns LUA_ERRSYNTAX without it.
    if (codeSize >= 3 && static_cast<unsigned char>(code[0]) == 0xEF && static_cast<unsigned char>(code[1]) == 0xBB &&
        static_cast<unsigned char>(code[2]) == 0xBF)
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
            lua_State* L = s_ScriptUnits[i].L;
            lua_getglobal(L, "OnUpdate");
            if (lua_isfunction(L, -1))
            {
                lua_pushnumber(L, dt);
                if (lua_pcall(L, 1, 0, 0) != LUA_OK)
                {
                    const char* err = lua_tostring(L, -1);
                    Engine_LogError("Lua Update error in unit %d: %s", i, err);
                    lua_pop(L, 1);
                }
            }
            else
            {
                lua_pop(L, 1);
            }
            // Generational GC handles reclamation automatically (minor cycles are
            // triggered by allocations, not by an explicit per-frame cost here).
        }
    }
}

void Engine_Script_EndCurrentMode()
{
    s_CurrentRenderMode = RENDER_MODE_NONE;
}

void Engine_Script_FrameTick()
{
    // Camera slots are fixed and never evicted, so this just advances the frame
    // counter (used by the perf logger and other subsystems).
    s_FrameCount++;
}

uint32_t Engine_Script_GetFrameCount() { return s_FrameCount; }

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
    // Fixed slots: assign the next unused slot (bounded by SCRIPTING_MAX_CAMERAS_3D).
    // Slots are never evicted; scripts keep and reuse the returned handle.
    if (s_Camera3DCount >= SCRIPTING_MAX_CAMERAS_3D)
    {
        Engine_LogError("[Script] make_camera_3d: all %d fixed camera slots are in use", SCRIPTING_MAX_CAMERAS_3D);
        lua_pushinteger(L, -1);
        return 1;
    }
    const int slot = s_Camera3DCount++;

    Camera3D cam;
    cam.position = Vector3{static_cast<float>(luaL_checknumber(L, 1)), static_cast<float>(luaL_checknumber(L, 2)),
                           static_cast<float>(luaL_checknumber(L, 3))};
    cam.target = Vector3{static_cast<float>(luaL_checknumber(L, 4)), static_cast<float>(luaL_checknumber(L, 5)),
                         static_cast<float>(luaL_checknumber(L, 6))};
    cam.up = Vector3{static_cast<float>(luaL_checknumber(L, 7)), static_cast<float>(luaL_checknumber(L, 8)),
                     static_cast<float>(luaL_checknumber(L, 9))};
    cam.fovy = static_cast<float>(luaL_checknumber(L, 10));
    cam.projection = static_cast<int>(luaL_checkinteger(L, 11));

    s_Cameras3D[slot] = cam;

    // Seed the renderer's matching slot so the camera is ready to activate.
    Renderer* r = Engine_GetRenderer();
    if (r)
        r->SetCamera3D(static_cast<CameraID>(slot), cam);

    lua_pushinteger(L, slot);
    return 1;
}

// graphics.make_camera_2d(offset_x, offset_y,
//                         target_x, target_y,
//                         rotation, zoom)      -> handle (int) or -1 on error
static int Lua_Graphics_MakeCamera2D(lua_State* L)
{
    if (s_Camera2DCount >= SCRIPTING_MAX_CAMERAS_2D)
    {
        Engine_LogError("[Script] make_camera_2d: all %d fixed camera slots are in use", SCRIPTING_MAX_CAMERAS_2D);
        lua_pushinteger(L, -1);
        return 1;
    }
    const int slot = s_Camera2DCount++;

    Camera2D cam;
    cam.offset = Vector2{static_cast<float>(luaL_checknumber(L, 1)), static_cast<float>(luaL_checknumber(L, 2))};
    cam.target = Vector2{static_cast<float>(luaL_checknumber(L, 3)), static_cast<float>(luaL_checknumber(L, 4))};
    cam.rotation = static_cast<float>(luaL_checknumber(L, 5));
    cam.zoom = static_cast<float>(luaL_checknumber(L, 6));

    s_Cameras2D[slot] = cam;

    lua_pushinteger(L, slot);
    return 1;
}

// ---------------------------------------------------------------------------
// Mode-switching bindings
// ---------------------------------------------------------------------------

// graphics.update_camera_3d(handle, pos_x, pos_y, pos_z, target_x, target_y, target_z)
// Moves/re-aims a fixed 3D camera slot in-place. The up vector and projection are
// preserved. Rotation is expressed by moving the look-at target. Safe to call
// every frame when driving the camera with analogue input.
static int Lua_Graphics_UpdateCamera3D(lua_State* L)
{
    int32_t handle = luaL_checkinteger(L, 1);

    if (handle < 0 || handle >= SCRIPTING_MAX_CAMERAS_3D)
    {
        Engine_LogError("[Script] update_camera_3d: invalid handle %d", handle);
        return 0;
    }

    s_Cameras3D[handle].position =
        Vector3{static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)),
                static_cast<float>(luaL_checknumber(L, 4))};
    s_Cameras3D[handle].target =
        Vector3{static_cast<float>(luaL_checknumber(L, 5)), static_cast<float>(luaL_checknumber(L, 6)),
                static_cast<float>(luaL_checknumber(L, 7))};

    // Always push the pose to the renderer's slot — it's the render camera when
    // this slot is the active one, and a harmless slot update otherwise.
    Renderer* r = Engine_GetRenderer();
    if (r)
        r->SetCamera3D(static_cast<CameraID>(handle), s_Cameras3D[handle]);
    return 0;
}

// graphics.begin_mode_3d(handle)
// Selects which fixed camera slot is the active (rendered) 3D camera. Exactly
// one 3D camera is rendered per frame.
static int Lua_Graphics_BeginMode3D(lua_State* L)
{
    int32_t handle = (int32_t)luaL_checkinteger(L, 1);

    if (handle < 0 || handle >= SCRIPTING_MAX_CAMERAS_3D)
    {
        Engine_LogError("[Script] begin_mode_3d: invalid handle %d", handle);
        return 0;
    }

    Renderer* r = Engine_GetRenderer();
    if (r)
    {
        r->SetCamera3D(static_cast<CameraID>(handle), s_Cameras3D[handle]);
        r->SetActiveCamera3D(static_cast<CameraID>(handle));
    }

    s_CurrentRenderMode = RENDER_MODE_3D;
    return 0;
}

static int Lua_Graphics_BeginMode2D(lua_State* L)
{
    int32_t handle = luaL_checkinteger(L, 1);

    if (handle < 0 || handle >= SCRIPTING_MAX_CAMERAS_2D)
    {
        Engine_LogError("[Script] begin_mode_2d: invalid handle %d", handle);
        return 0;
    }

    Renderer* r = Engine_GetRenderer();
    if (r)
        r->SetActiveCamera2D(s_Cameras2D[handle]);

    s_CurrentRenderMode = RENDER_MODE_2D;
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
    lua_pushnumber(L, static_cast<lua_Number>(Engine_GetTotalTime()));
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

// engine.get_resource_token() → string
// Returns the active resource location token (e.g. "cdrom0:", "host:") so
// Lua scripts can construct paths without hardcoding a device prefix.
static int Lua_Engine_GetResourceToken(lua_State* L)
{
    const char* token = Engine_GetResourceLocationToken();
    lua_pushstring(L, token ? token : "");
    return 1;
}

// engine.make_path(relativePath) → string
// Constructs a full device path from a relative path using the active token.
// relativePath uses backslash separators; e.g. "RASSETS\\BOX.PS2A".
// Result format per device:
//   cdrom0:  → "cdrom0:\\<PATH>;1"
//   mass0:   → "mass0:\\<PATH>"
//   hdd0:    → "hdd0:\\<PATH>"
//   host:    → "host:<PATH>"
static int Lua_Engine_MakePath(lua_State* L)
{
    const char* relativePath = luaL_checkstring(L, 1);
    const char* token = Engine_GetResourceLocationToken();
    char pathBuf[IO_FILE_MAX_PATH];
    Engine_BuildPath(token ? token : "cdrom0:", relativePath, pathBuf, IO_FILE_MAX_PATH);
    lua_pushstring(L, pathBuf);
    return 1;
}

// engine.load_script(path) → bool
// Loads a Lua source file into a free ScriptUnit, runs it (registering its
// OnUpdate), and returns true on success.  The file descriptor is released
// immediately after the bytecode is copied into the arena slot.
//
// Forward-declare the EngineApp file helpers here; their full extern block
// lives in the IO bindings section further down the file.
extern int32_t EngineApp_FileOpen(const char* path);
extern size_t  EngineApp_FileRead(int32_t fileId, const void** outData);
extern bool    EngineApp_FileClose(int32_t fileId);

static int Lua_Engine_LoadScript(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);

    int32_t fd = EngineApp_FileOpen(path);
    if (fd < 0)
    {
        Engine_LogError("[Lua] engine.load_script: cannot open '%s'", path);
        lua_pushboolean(L, 0);
        return 1;
    }

    const void* data = nullptr;
    size_t size = EngineApp_FileRead(fd, &data);
    if (size == 0 || !data)
    {
        Engine_LogError("[Lua] engine.load_script: empty or unreadable '%s'", path);
        EngineApp_FileClose(fd);
        lua_pushboolean(L, 0);
        return 1;
    }

    int unitIndex = Engine_Script_Load(data, size);
    EngineApp_FileClose(fd);

    if (unitIndex < 0)
    {
        Engine_LogError("[Lua] engine.load_script: no free script unit for '%s'", path);
        lua_pushboolean(L, 0);
        return 1;
    }

    bool ok = Engine_Script_Run(unitIndex);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// Read a primitive color starting at stack index `arg`. Two accepted forms:
//   * three (optionally four) numeric args  r, g, b [, a]   — the fast path used
//     by the hot draw loops: plain lua_tonumber reads, no table probes/pops.
//   * a single {r, g, b [, a]} table                        — kept for scripts
//     that still pass a color table.
// Missing / non-numeric args default to opaque white. Components are 0..255.
static Color3 Lua_ReadColorArgs(lua_State* L, int arg)
{
    Color3 color = {1.f, 1.f, 1.f};
    if (lua_isnumber(L, arg))
    {
        color.r = static_cast<float>(lua_tonumber(L, arg)) / 255.f;
        color.g = static_cast<float>(lua_tonumber(L, arg + 1)) / 255.f;
        color.b = static_cast<float>(lua_tonumber(L, arg + 2)) / 255.f;
    }
    else if (lua_istable(L, arg))
    {
        lua_geti(L, arg, 1); color.r = static_cast<float>(lua_tonumber(L, -1)) / 255.f;
        lua_geti(L, arg, 2); color.g = static_cast<float>(lua_tonumber(L, -1)) / 255.f;
        lua_geti(L, arg, 3); color.b = static_cast<float>(lua_tonumber(L, -1)) / 255.f;
        lua_pop(L, 3);
    }
    return color;
}

// Graphics
static int Lua_Graphics_Clear(lua_State* L)
{
    if (lua_istable(L, 1))
    {
        lua_geti(L, 1, 1);
        unsigned char r = static_cast<unsigned char>(lua_tointeger(L, -1));
        lua_geti(L, 1, 2);
        unsigned char g = static_cast<unsigned char>(lua_tointeger(L, -1));
        lua_geti(L, 1, 3);
        unsigned char b = static_cast<unsigned char>(lua_tointeger(L, -1));
        lua_geti(L, 1, 4);
        unsigned char a = static_cast<unsigned char>(lua_tointeger(L, -1));
        lua_pop(L, 4);
        UNUSED_VAR(a);

        Renderer* rRenderer = Engine_GetRenderer();
        if (rRenderer)
        {
            Color3 color{static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f, static_cast<float>(b) / 255.0f};
            rRenderer->ClearFrame(color);
        }
    }
    return 0;
}

static int Lua_Graphics_DrawRect(lua_State* L)
{
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float w = static_cast<float>(luaL_checknumber(L, 3));
    float h = static_cast<float>(luaL_checknumber(L, 4));

    if (lua_istable(L, 5))
    {
        lua_geti(L, 5, 1);
        unsigned char r = static_cast<unsigned char>(lua_tointeger(L, -1));
        lua_geti(L, 5, 2);
        unsigned char g = static_cast<unsigned char>(lua_tointeger(L, -1));
        lua_geti(L, 5, 3);
        unsigned char b = static_cast<unsigned char>(lua_tointeger(L, -1));
        lua_geti(L, 5, 4);
        unsigned char a = static_cast<unsigned char>(lua_tointeger(L, -1));
        lua_pop(L, 4);
        UNUSED_VAR(a);

        Renderer* rRenderer = Engine_GetRenderer();
        if (rRenderer)
        {
            Color3 color{static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f, static_cast<float>(b) / 255.0f};
            rRenderer->DrawRect2D(static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(w), static_cast<int32_t>(h), color);
        }
    }
    return 0;
}

// graphics.draw_cube(x, y, z, size [, r, g, b | {r,g,b,a}])
static int Lua_Graphics_DrawCube(lua_State* L)
{
    float px = static_cast<float>(luaL_checknumber(L, 1));
    float py = static_cast<float>(luaL_checknumber(L, 2));
    float pz = static_cast<float>(luaL_checknumber(L, 3));
    float sz = static_cast<float>(luaL_checknumber(L, 4));
    Color3 color = Lua_ReadColorArgs(L, 5);

    Renderer* r = Engine_GetRenderer();
    if (r)
        r->AddPrimitiveToDrawList(Primitive3D::Cube,
                                  Vector3{px, py, pz},
                                  Vector3{0.f, 0.f, 0.f},
                                  Vector3{sz, sz, sz},
                                  color);
    return 0;
}

// graphics.draw_sphere(x, y, z, size [, r, g, b | {r,g,b,a}])
static int Lua_Graphics_DrawSphere(lua_State* L)
{
    float px = static_cast<float>(luaL_checknumber(L, 1));
    float py = static_cast<float>(luaL_checknumber(L, 2));
    float pz = static_cast<float>(luaL_checknumber(L, 3));
    float sz = static_cast<float>(luaL_checknumber(L, 4));
    Color3 color = Lua_ReadColorArgs(L, 5);

    Renderer* r = Engine_GetRenderer();
    if (r)
        r->AddPrimitiveToDrawList(Primitive3D::Sphere,
                                  Vector3{px, py, pz},
                                  Vector3{0.f, 0.f, 0.f},
                                  Vector3{sz, sz, sz},
                                  color);
    return 0;
}

// graphics.draw_cylinder(x, y, z, size [, r, g, b | {r,g,b,a}])
static int Lua_Graphics_DrawCylinder(lua_State* L)
{
    float px = static_cast<float>(luaL_checknumber(L, 1));
    float py = static_cast<float>(luaL_checknumber(L, 2));
    float pz = static_cast<float>(luaL_checknumber(L, 3));
    float sz = static_cast<float>(luaL_checknumber(L, 4));
    Color3 color = Lua_ReadColorArgs(L, 5);

    Renderer* r = Engine_GetRenderer();
    if (r)
        r->AddPrimitiveToDrawList(Primitive3D::Cylinder,
                                  Vector3{px, py, pz},
                                  Vector3{0.f, 0.f, 0.f},
                                  Vector3{sz, sz, sz},
                                  color);
    return 0;
}

static int Lua_Graphics_DrawGrid(lua_State* L)
{
    int slices = static_cast<int>(luaL_checknumber(L, 1));
    float spacing = static_cast<float>(luaL_checknumber(L, 2));

    Renderer* rRenderer = Engine_GetRenderer();
    if (rRenderer)
        rRenderer->DrawGrid(slices, spacing);
    return 0;
}

// graphics.draw_cube_textured(x, y, z, size, handle [, r, g, b | {r,g,b,a}])
// Submits a textured cube to the renderer draw list.
static int Lua_Graphics_DrawCubeTextured(lua_State* L)
{
    float px       = static_cast<float>(luaL_checknumber(L, 1));
    float py       = static_cast<float>(luaL_checknumber(L, 2));
    float pz       = static_cast<float>(luaL_checknumber(L, 3));
    float sz       = static_cast<float>(luaL_checknumber(L, 4));
    int32_t handle = static_cast<int32_t>(luaL_checkinteger(L, 5));
    Color3 tint = Lua_ReadColorArgs(L, 6);

    Renderer* r = Engine_GetRenderer();
    if (r)
        r->AddPrimitiveToDrawList(Primitive3D::Cube,
                                  Vector3{px, py, pz},
                                  Vector3{0.f, 0.f, 0.f},
                                  Vector3{sz, sz, sz},
                                  tint, handle);
    return 0;
}

// ---------------------------------------------------------------------------
// Lua math type helpers — convert raylib math types to Lua tables
// ---------------------------------------------------------------------------

// Pushes {x, y} onto the Lua stack.
static void Lua_PushVector2(lua_State* L, Vector2 v)
{
    lua_createtable(L, 0, 2);
    lua_pushnumber(L, v.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, v.y);
    lua_setfield(L, -2, "y");
}

// Pushes {x, y, z} onto the Lua stack.
[[maybe_unused]] static void Lua_PushVector3(lua_State* L, Vector3 v)
{
    lua_createtable(L, 0, 3);
    lua_pushnumber(L, v.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, v.y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, v.z);
    lua_setfield(L, -2, "z");
}

// Pushes {x, y, z, w} onto the Lua stack.
[[maybe_unused]] static void Lua_PushVector4(lua_State* L, Vector4 v)
{
    lua_createtable(L, 0, 4);
    lua_pushnumber(L, v.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, v.y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, v.z);
    lua_setfield(L, -2, "z");
    lua_pushnumber(L, v.w);
    lua_setfield(L, -2, "w");
}

// Pushes {m0..m15} onto the Lua stack (column-major, matching raylib's Matrix layout).
[[maybe_unused]] static void Lua_PushMatrix(lua_State* L, Matrix m)
{
    lua_createtable(L, 0, 16);
    lua_pushnumber(L, m.m0);
    lua_setfield(L, -2, "m0");
    lua_pushnumber(L, m.m1);
    lua_setfield(L, -2, "m1");
    lua_pushnumber(L, m.m2);
    lua_setfield(L, -2, "m2");
    lua_pushnumber(L, m.m3);
    lua_setfield(L, -2, "m3");
    lua_pushnumber(L, m.m4);
    lua_setfield(L, -2, "m4");
    lua_pushnumber(L, m.m5);
    lua_setfield(L, -2, "m5");
    lua_pushnumber(L, m.m6);
    lua_setfield(L, -2, "m6");
    lua_pushnumber(L, m.m7);
    lua_setfield(L, -2, "m7");
    lua_pushnumber(L, m.m8);
    lua_setfield(L, -2, "m8");
    lua_pushnumber(L, m.m9);
    lua_setfield(L, -2, "m9");
    lua_pushnumber(L, m.m10);
    lua_setfield(L, -2, "m10");
    lua_pushnumber(L, m.m11);
    lua_setfield(L, -2, "m11");
    lua_pushnumber(L, m.m12);
    lua_setfield(L, -2, "m12");
    lua_pushnumber(L, m.m13);
    lua_setfield(L, -2, "m13");
    lua_pushnumber(L, m.m14);
    lua_setfield(L, -2, "m14");
    lua_pushnumber(L, m.m15);
    lua_setfield(L, -2, "m15");
}

// ---------------------------------------------------------------------------
// math.sincos — PS2 performance extension
// ---------------------------------------------------------------------------

// math.sincos(angle) → sin_val, cos_val
// Computes both sin and cos of the same angle in a single C binding call.
// Saves one Lua→C FFI crossing vs. calling math.sin + math.cos separately, and
// with LUA_32BITS=1 the underlying sinf/cosf hits the PS2 hardware FPU path via
// ps2sdk's libm (single-precision, hardware-accelerated on the EE COP1 unit).
static int Lua_Math_SinCos(lua_State* L)
{
    float angle = static_cast<float>(luaL_checknumber(L, 1));
    lua_pushnumber(L, static_cast<lua_Number>(sinf(angle)));
    lua_pushnumber(L, static_cast<lua_Number>(cosf(angle)));
    return 2;
}

// Injects math.sincos into the existing math table.
// Must be called AFTER luaL_requiref(L, "math", luaopen_math, 1).
static void RegisterMathExtensions(lua_State* L)
{
    lua_getglobal(L, "math");
    if (lua_istable(L, -1))
    {
        lua_pushcfunction(L, Lua_Math_SinCos);
        lua_setfield(L, -2, "sincos");
    }
    lua_pop(L, 1);
}

// Input
/// input.is_pad_pressed(port, btn)
static int Lua_Input_IsPadPressed(lua_State* L)
{
    const lua_Number port = luaL_checknumber(L, 1);
    const char* btn = luaL_checkstring(L, 2);
    bool pressed = false;
    GamePadButton button = GamePadButton::Unknown;

    // Face buttons
    if (strcmp(btn, "x") == 0)
        button = GamePadButton::Cross;
    else if (strcmp(btn, "cir") == 0)
        button = GamePadButton::Circle;
    else if (strcmp(btn, "squ") == 0)
        button = GamePadButton::Square;
    else if (strcmp(btn, "tri") == 0)
        button = GamePadButton::Triangle;
    // D-pad
    else if (strcmp(btn, "dpad_up") == 0)
        button = GamePadButton::DPadUp;
    else if (strcmp(btn, "dpad_down") == 0)
        button = GamePadButton::DPadDown;
    else if (strcmp(btn, "dpad_left") == 0)
        button = GamePadButton::DPadLeft;
    else if (strcmp(btn, "dpad_right") == 0)
        button = GamePadButton::DPadRight;
    // Shoulder buttons
    else if (strcmp(btn, "l1") == 0)
        button = GamePadButton::L1;
    else if (strcmp(btn, "l2") == 0)
        button = GamePadButton::L2;
    else if (strcmp(btn, "r1") == 0)
        button = GamePadButton::R1;
    else if (strcmp(btn, "r2") == 0)
        button = GamePadButton::R2;
    // Stick clicks
    else if (strcmp(btn, "l3") == 0)
        button = GamePadButton::L3;
    else if (strcmp(btn, "r3") == 0)
        button = GamePadButton::R3;
    // Menu
    else if (strcmp(btn, "start") == 0)
        button = GamePadButton::Start;
    else if (strcmp(btn, "select") == 0)
        button = GamePadButton::Select;
    else
        Engine_LogError("[Lua] input.is_pad_pressed: unknown button '%s'", btn);

    pressed = IsGamePadButtonPressed(static_cast<uint8_t>(port), button);

    lua_pushboolean(L, pressed);
    return 1;
}

// input.get_joy_status(port, joystick) -> {x, y} or nil on error
// joystick: "left" or "right"
static int Lua_Input_GetJoyStatus(lua_State* L)
{
    const lua_Number port = luaL_checknumber(L, 1);
    const char* joystick = luaL_checkstring(L, 2);
    GamePadJoystick joy = GamePadJoystick::UnknownJoystick;

    if (port < 0 || port >= MAX_GAME_PAD_PORTS)
    {
        Engine_LogError("[Lua] input.get_joy_status: invalid port specified '%d'", port);
        lua_pushnil(L);
        return 1;
    }

    if (strcmp(joystick, "left") == 0)
        joy = GamePadJoystick::LeftJoystick;
    else if (strcmp(joystick, "right") == 0)
        joy = GamePadJoystick::RightJoystick;
    else
    {
        Engine_LogError("[Lua] input.get_joy_status: unknown joystick '%s' (expected 'left' or 'right')", joystick);
        lua_pushnil(L);
        return 1;
    }

    const auto vec = GetGamePadAxis(static_cast<uint8_t>(port), joy);
    Lua_PushVector2(L, vec);
    return 1;
}

// input.get_joy_axis(port, joystick) -> x, y
// Like get_joy_status but returns two numbers instead of a table.
// Avoids a per-frame Lua heap allocation (lua_createtable) in the hot update loop.
// joystick: "left" or "right".  Returns 0, 0 on any error.
static int Lua_Input_GetJoyAxis(lua_State* L)
{
    const lua_Number port = luaL_checknumber(L, 1);
    const char* joystick = luaL_checkstring(L, 2);
    GamePadJoystick joy = GamePadJoystick::UnknownJoystick;

    if (port < 0 || port >= MAX_GAME_PAD_PORTS)
    {
        Engine_LogError("[Lua] input.get_joy_axis: invalid port specified '%d'", (int)port);
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        return 2;
    }

    if (strcmp(joystick, "left") == 0)
        joy = GamePadJoystick::LeftJoystick;
    else if (strcmp(joystick, "right") == 0)
        joy = GamePadJoystick::RightJoystick;
    else
    {
        Engine_LogError("[Lua] input.get_joy_axis: unknown joystick '%s' (expected 'left' or 'right')", joystick);
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        return 2;
    }

    const auto vec = GetGamePadAxis(static_cast<uint8_t>(port), joy);
    lua_pushnumber(L, static_cast<lua_Number>(vec.x));
    lua_pushnumber(L, static_cast<lua_Number>(vec.y));
    return 2;
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
    lua_pushcfunction(L, Lua_Engine_GetResourceToken);
    lua_setfield(L, -2, "get_resource_token");
    lua_pushcfunction(L, Lua_Engine_MakePath);
    lua_setfield(L, -2, "make_path");
    lua_pushcfunction(L, Lua_Engine_LoadScript);
    lua_setfield(L, -2, "load_script");
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
    lua_pushcfunction(L, Lua_Graphics_DrawSphere);
    lua_setfield(L, -2, "draw_sphere");
    lua_pushcfunction(L, Lua_Graphics_DrawCylinder);
    lua_setfield(L, -2, "draw_cylinder");
    lua_pushcfunction(L, Lua_Graphics_DrawGrid);
    lua_setfield(L, -2, "draw_grid");
    lua_pushcfunction(L, Lua_Graphics_DrawCubeTextured);
    lua_setfield(L, -2, "draw_cube_textured");
    lua_pushcfunction(L, Lua_Graphics_MakeCamera3D);
    lua_setfield(L, -2, "make_camera_3d");
    lua_pushcfunction(L, Lua_Graphics_UpdateCamera3D);
    lua_setfield(L, -2, "update_camera_3d");
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
    lua_pushcfunction(L, Lua_Input_GetJoyStatus);
    lua_setfield(L, -2, "get_joy_status");
    lua_pushcfunction(L, Lua_Input_GetJoyAxis);
    lua_setfield(L, -2, "get_joy_axis");
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
    lua_pushinteger(L, fd);
    return 1;
}

static int Lua_IO_OpenWrite(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    int32_t fd = EngineApp_FileOpenWrite(path);
    lua_pushinteger(L, fd);
    return 1;
}

static int Lua_IO_Close(lua_State* L)
{
    int32_t fd = luaL_checkinteger(L, 1);
    bool ok = EngineApp_FileClose(fd);
    lua_pushboolean(L, ok);
    return 1;
}

static int Lua_IO_GetSize(lua_State* L)
{
    int32_t fd = luaL_checkinteger(L, 1);
    size_t sz = EngineApp_FileGetSize(fd);
    lua_pushinteger(L, static_cast<lua_Integer>(sz));
    return 1;
}

// io.read(fileId) -> string, number
// Returns the file content as a Lua string and the byte count.
// Intended for small text files only (config, dialogue).
static int Lua_IO_Read(lua_State* L)
{
    int32_t fd = luaL_checkinteger(L, 1);
    const void* data = nullptr;
    size_t bytesRead = EngineApp_FileRead(fd, &data);
    if (bytesRead == 0 || !data)
    {
        lua_pushnil(L);
        lua_pushinteger(L, 0);
        return 2;
    }
    lua_pushlstring(L, static_cast<const char*>(data), bytesRead);
    lua_pushinteger(L, static_cast<lua_Integer>(bytesRead));
    return 2;
}

// io.write(fileId, content) -> number (bytes written; 0 on error)
static int Lua_IO_Write(lua_State* L)
{
    int32_t fd = luaL_checkinteger(L, 1);
    size_t dataLen = 0;
    const char* data = luaL_checklstring(L, 2, &dataLen);
    size_t written = EngineApp_FileWrite(fd, data, dataLen);
    lua_pushinteger(L, static_cast<lua_Integer>(written));
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
    lua_pushinteger(L, handle);
    return 1;
}

static int Lua_Resources_Unload(lua_State* L)
{
    int32_t handle = luaL_checkinteger(L, 1);
    Engine_Resource_Unload(handle);
    return 0;
}

static int Lua_Resources_IsReady(lua_State* L)
{
    int32_t handle = luaL_checkinteger(L, 1);
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
