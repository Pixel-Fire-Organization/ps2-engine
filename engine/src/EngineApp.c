#include "Engine.h"
#include "EngineApp.h"

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Internal exit flag — set by EngineApp_OnExitRequested (called from Lua via
// the engine.exit() binding, which receives this as a function pointer).
// ---------------------------------------------------------------------------
static bool s_ExitRequested = false;

void EngineApp_OnExitRequested(void) {
    s_ExitRequested = true;
}

// ---------------------------------------------------------------------------
// File Descriptor Table
// Tracks open file handles for the io.* Lua bindings.
// READ  descriptors hold file data in an ARENA_CONFIG slot.
// WRITE descriptors hold only the target path; data goes directly to disc.
// ---------------------------------------------------------------------------
typedef enum {
    FILE_MODE_READ,
    FILE_MODE_WRITE
} FileMode;

typedef struct {
    int32_t slotIndex;
    size_t size;
    FileMode mode;
    char path[IO_FILE_MAX_PATH];
    bool inUse;
} FileDescriptor;

static FileDescriptor s_Files[APP_MAX_FILE_SLOTS];

static int32_t Internal_FindFreeDescriptor(void) {
    for (int32_t i = 0; i < APP_MAX_FILE_SLOTS; i++) {
        if (!s_Files[i].inUse) return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// EngineApp_FileOpen — synchronous read into ARENA_CONFIG slot.
// Returns a fileId >= 0 on success, or -1 on failure.
// ---------------------------------------------------------------------------
int32_t EngineApp_FileOpen(const char *path) {
    if (!path) return -1;

    int32_t fd = Internal_FindFreeDescriptor();
    if (fd < 0) {
        Engine_LogError("EngineApp: no free file descriptors (max %d)", APP_MAX_FILE_SLOTS);
        return -1;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        Engine_LogError("EngineApp: failed to open '%s'", path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    size_t fileSize = (size_t) ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fileSize == 0 || fileSize > APP_MAX_FILE_DATA_SIZE) {
        Engine_LogError("EngineApp: file '%s' size %zu exceeds APP_MAX_FILE_DATA_SIZE", path, fileSize);
        fclose(f);
        return -1;
    }

    // Use the descriptor index as the config arena slot index.
    if (!Engine_LoadToSlot(ARENA_CONFIG, (uint32_t) fd, NULL, fileSize)) {
        Engine_LogError("EngineApp: failed to reserve config slot %d for '%s'", fd, path);
        fclose(f);
        return -1;
    }

    void *slotPtr = Engine_GetSlot(ARENA_CONFIG, (uint32_t) fd);
    size_t bytesRead = fread(slotPtr, 1, fileSize, f);
    fclose(f);

    if (bytesRead != fileSize) {
        Engine_LogError("EngineApp: partial read for '%s' (%zu / %zu bytes)", path, bytesRead, fileSize);
        Engine_ClearSlot(ARENA_CONFIG, (uint32_t) fd);
        return -1;
    }

    s_Files[fd].slotIndex = fd;
    s_Files[fd].size = bytesRead;
    s_Files[fd].mode = FILE_MODE_READ;
    s_Files[fd].inUse = true;
    strncpy(s_Files[fd].path, path, IO_FILE_MAX_PATH - 1);
    s_Files[fd].path[IO_FILE_MAX_PATH - 1] = '\0';

    return fd;
}

// ---------------------------------------------------------------------------
// EngineApp_FileOpenWrite — registers a path for writing. No slot allocated.
// Returns a fileId >= 0 on success, or -1 on failure.
// ---------------------------------------------------------------------------
int32_t EngineApp_FileOpenWrite(const char *path) {
    if (!path) return -1;

    int32_t fd = Internal_FindFreeDescriptor();
    if (fd < 0) {
        Engine_LogError("EngineApp: no free file descriptors (max %d)", APP_MAX_FILE_SLOTS);
        return -1;
    }

    s_Files[fd].slotIndex = -1;
    s_Files[fd].size = 0;
    s_Files[fd].mode = FILE_MODE_WRITE;
    s_Files[fd].inUse = true;
    strncpy(s_Files[fd].path, path, IO_FILE_MAX_PATH - 1);
    s_Files[fd].path[IO_FILE_MAX_PATH - 1] = '\0';

    return fd;
}

// ---------------------------------------------------------------------------
// EngineApp_FileRead — returns a pointer to the slot data and bytes read.
// Only valid on READ descriptors. Returns 0 on any error.
// ---------------------------------------------------------------------------
size_t EngineApp_FileRead(int32_t fileId, const void **outData) {
    if (fileId < 0 || fileId >= APP_MAX_FILE_SLOTS || !s_Files[fileId].inUse) {
        Engine_LogError("EngineApp: FileRead called with invalid fileId %d", fileId);
        if (outData) *outData = NULL;
        return 0;
    }
    if (s_Files[fileId].mode != FILE_MODE_READ) {
        Engine_LogError("EngineApp: FileRead called on WRITE descriptor (fileId %d)", fileId);
        if (outData) *outData = NULL;
        return 0;
    }

    if (outData) {
        *outData = Engine_GetSlot(ARENA_CONFIG, (uint32_t) s_Files[fileId].slotIndex);
    }
    return s_Files[fileId].size;
}

// ---------------------------------------------------------------------------
// EngineApp_FileWrite — writes data to disc at the path stored in the descriptor.
// Guard: returns 0 and logs an error if called on a READ descriptor.
// ---------------------------------------------------------------------------
size_t EngineApp_FileWrite(int32_t fileId, const void *data, size_t size) {
    if (fileId < 0 || fileId >= APP_MAX_FILE_SLOTS || !s_Files[fileId].inUse) {
        Engine_LogError("EngineApp: FileWrite called with invalid fileId %d", fileId);
        return 0;
    }
    if (s_Files[fileId].mode != FILE_MODE_WRITE) {
        Engine_LogError("EngineApp: FileWrite called on READ descriptor (fileId %d) — use io.open_write", fileId);
        return 0;
    }
    if (!data || size == 0) return 0;

    FILE *f = fopen(s_Files[fileId].path, "wb");
    if (!f) {
        Engine_LogError("EngineApp: failed to open '%s' for writing", s_Files[fileId].path);
        return 0;
    }

    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    return written;
}

// ---------------------------------------------------------------------------
// EngineApp_FileClose — releases the descriptor (and its arena slot if READ).
// Returns false if the fileId is not found in the descriptor table.
// ---------------------------------------------------------------------------
bool EngineApp_FileClose(int32_t fileId) {
    if (fileId < 0 || fileId >= APP_MAX_FILE_SLOTS || !s_Files[fileId].inUse) {
        Engine_LogError("EngineApp: FileClose called with invalid fileId %d", fileId);
        return false;
    }

    if (s_Files[fileId].mode == FILE_MODE_READ) {
        Engine_ClearSlot(ARENA_CONFIG, (uint32_t) s_Files[fileId].slotIndex);
    }

    memset(&s_Files[fileId], 0, sizeof(FileDescriptor));
    return true;
}

// ---------------------------------------------------------------------------
// EngineApp_FileGetSize
// ---------------------------------------------------------------------------
size_t EngineApp_FileGetSize(int32_t fileId) {
    if (fileId < 0 || fileId >= APP_MAX_FILE_SLOTS || !s_Files[fileId].inUse) {
        return 0;
    }
    return s_Files[fileId].size;
}

// ---------------------------------------------------------------------------
// Public shell API
// ---------------------------------------------------------------------------

bool EngineStart(const char *mainScript) {
    const char *scriptPath = mainScript ? mainScript : SCRIPTING_MAIN_SCRIPT_PATH;

    EngineConfig config = {
        .windowTitle = "PS2 Engine"
    };

    if (!Engine_Init(config)) {
        Engine_Panic("Engine_Init failed — hardware or memory error");
        return false;
    }

    memset(s_Files, 0, sizeof(s_Files));
    s_ExitRequested = false;

    // Pass the exit callback into the scripting system so Lua engine.exit() works.
    Engine_Script_SetExitCallback(EngineApp_OnExitRequested);

    // Bootstrap: load and run the entry-point script.
    int32_t fd = EngineApp_FileOpen(scriptPath);
    if (fd < 0) {
        char buff[LOG_STRING_MAX_SIZE];
        sprintf(buff, "EngineStart: failed to open main script '%s'", scriptPath);
        Engine_Panic(buff);
        return false;
    }

    const void *scriptData = NULL;
    size_t scriptSize = EngineApp_FileRead(fd, &scriptData);
    if (scriptSize == 0 || !scriptData) {
        Engine_Panic("Main Lua script is empty or unreadable");
        EngineApp_FileClose(fd);
        return false;
    }

    int unitIndex = Engine_Script_Load(scriptData, scriptSize);
    EngineApp_FileClose(fd); // slot no longer needed after load

    if (unitIndex < 0) {
        Engine_Panic("Failed to load Lua script into script unit — slot exhausted or too large");
        return false;
    }

    if (!Engine_Script_Run(unitIndex)) {
        return false;
    }

    Engine_LogInfo("EngineStart: '%s' running on unit %d", scriptPath, unitIndex);
    return true;
}

void EngineUpdate(void) {
    float dt = GetFrameTime();

    BeginDrawing();
    {
        Engine_Script_UpdateAll(dt);
        Engine_Script_EndCurrentMode(); // If any modes have started end them.
        Engine_DrawDebugOverlay();
    }
    EndDrawing();

    Engine_IO_Update();
    Engine_Resource_Update();
    Engine_Script_FrameTick();
}

bool EngineExited(void) {
    return WindowShouldClose() || s_ExitRequested;
}

void EngineStop(void) {
    Engine_Close();
}
