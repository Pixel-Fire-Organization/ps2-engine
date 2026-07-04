#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <float.h>
#include <malloc.h>
#include "Engine.h"
#include "EngineInput.h"
#include "graphics/DrawList.h"
#include "graphics/Renderer.h"

// Internal log severity levels for CustomLog(). Previously these matched
// raylib's TraceLogLevel by name (CustomLog was installed as its trace
// callback); now that raylib is gone they are just this file's own scheme.
enum
{
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
};

static constexpr char ASCII_TABLE_STR[] = "!\"#$%&'()*\n" /* 33-42  */
                                          "+,-./01234\n" /* 43-52  */
                                          "56789:;<=>\n" /* 53-62  */
                                          "?@ABCDEFGH\n" /* 63-72  */
                                          "IJKLMNOPQR\n" /* 73-82  */
                                          "STUVWXYZ[\\\n" /* 83-92  */
                                          "]^_`abcdef\n" /* 93-102 */
                                          "ghijklmnop\n" /* 103-112 */
                                          "qrstuvwxyz\n" /* 113-122 */
                                          "{|}~\x7f"; /* 123-127 */

static bool s_DebugOverlayVisible = false;
static bool s_ToggleWasHeld = false;

static void CustomLog(int logLevel, const char* text, va_list args)
{
    // High-frequency debug logs are completely muted to preserve bus bandwidth
    if (logLevel == LOG_DEBUG)
        return;

    char buffer[1024]; // Increased buffer for safety
    const char* prefix = "INFO: ";

    if (logLevel == LOG_WARNING)
        prefix = "WARN: ";
    else if (logLevel == LOG_ERROR)
        prefix = "ERR : ";

    int offset = snprintf(buffer, sizeof(buffer), "%s", prefix);
    if (offset < (int)sizeof(buffer))
    {
        vsnprintf(buffer + offset, sizeof(buffer) - offset, text, args);
    }
    buffer[sizeof(buffer) - 1] = '\0';

    // CRITICAL: Replace ALL control characters (especially \n and \r) with spaces
    // ps2client/plink can hang the EE if control sequences are malformed or too frequent.
    for (int i = 0; buffer[i] != '\0'; i++)
    {
        unsigned char c = static_cast<unsigned char>(buffer[i]);
        if (c == '\n' || c == '\r' || c == '\t')
        {
            continue;
        }
        else if (c < 32 || c > 126)
        {
            buffer[i] = '?';
        }
    }

    printf("%s\n", buffer); // Single newline at the end is safe for the kernel flush
}

void Engine_InitDebug()
{
    Engine_LogInfo("Engine Debug initialized.");
}

void Engine_LogInfo(const char* text, ...)
{
    va_list args;
    va_start(args, text);
    CustomLog(LOG_INFO, text, args);
    va_end(args);
}

void Engine_LogError(const char* text, ...)
{
    va_list args;
    va_start(args, text);
    CustomLog(LOG_ERROR, text, args);
    va_end(args);
}

void Engine_DrawDebugOverlay()
{
    Renderer* r = Engine_GetRenderer();
    if (r)
        r->DrawDebugOverlay();
}

void Engine_DrawAsciiTable() {}

void Engine_Panic(const char* message)
{
#ifdef DEBUG
    Engine_LogError("!!! PS2 PANIC !!! %s", message);

    // Draw a red panic screen with the active renderer (PS2GL or GIFTAG). Both
    // implement the BeginFrame/ClearFrame/DrawRect2D/EndFrame panic path.
    Renderer* currentRenderer = Engine_GetRenderer();
    if (currentRenderer && currentRenderer->IsInitialized())
    {
        while (true)
        {
            currentRenderer->BeginFrame();
            currentRenderer->ClearFrame(Color3{1.0f, 0.0f, 0.0f});
            currentRenderer->DrawRect2D(PANIC_UI_PADDING, PANIC_UI_PADDING, 320, 80, Color3{1.0f, 1.0f, 1.0f});
            currentRenderer->EndFrame();
        }
    }
#else
    UNUSED_VAR(message);
    while (1)
        ;
#endif
}

// ---------------------------------------------------------------------------
// Performance Logger
// ---------------------------------------------------------------------------

static bool s_PerfLoggerEnabled = false;
static bool s_SnapshotWasHeld = false; // debounce for L1+L2+R1+R2

void Engine_PerfLogger_Init(bool enabled)
{
    s_PerfLoggerEnabled = enabled;
    s_SnapshotWasHeld = false;
    s_ToggleWasHeld = false;
    if (enabled)
        Engine_LogInfo("[PerfLogger] Initialized. Hold L1+L2+R1+R2 for console dump. L1+L2+L3+R3 toggles UI.");
}

void Engine_PerfLogger_Tick()
{
    if (!s_PerfLoggerEnabled)
        return;

    // Heartbeat: one compact line every 50 frames (~1/s at 50fps). After a hang
    // the last heartbeat pins the frame/time of death and shows the heap trend.
    const uint32_t frame = Engine_Script_GetFrameCount();
    if (frame != 0 && (frame % 50u) == 0u)
    {
        size_t heapUsed = 0;
        Engine_GetHeapStats(nullptr, &heapUsed, nullptr);
        Engine_LogInfo("[HB] frame=%u t=%.1fs heap=%zuKB fps=%.1f",
                       frame, Engine_GetTotalTime(), heapUsed / 1024, Engine_GetFPS());
    }

    // Combo 1: L1+L2+R1+R2 (Console Snapshot)
    const bool snapshotNow =
        IsGamePadButtonPressed(0, GamePadButton::L1) && IsGamePadButtonPressed(0, GamePadButton::L2) && IsGamePadButtonPressed(0, GamePadButton::R1) && IsGamePadButtonPressed(0, GamePadButton::R2);

    // Combo 2: L1+L2+L3+R3 (UI Toggle)
    const bool toggleNow =
        IsGamePadButtonPressed(0, GamePadButton::L1) && IsGamePadButtonPressed(0, GamePadButton::L2) && IsGamePadButtonPressed(0, GamePadButton::L3) && IsGamePadButtonPressed(0, GamePadButton::R3);

    // Handle UI Toggle (Rising Edge)
    if (toggleNow)
    {
        if (!s_ToggleWasHeld)
        {
            s_DebugOverlayVisible = !s_DebugOverlayVisible;
            Engine_LogInfo("[PerfLogger] Debug overlay visibility: %s", s_DebugOverlayVisible ? "ON" : "OFF");
            s_ToggleWasHeld = true;
        }
    }
    else
    {
        s_ToggleWasHeld = false;
    }

    // Handle Snapshot (Rising Edge)
    if (!snapshotNow)
    {
        s_SnapshotWasHeld = false;
        return;
    }

    if (s_SnapshotWasHeld)
        return;
    s_SnapshotWasHeld = true;

    // --- Gather stats ---
    DrawStats ds{};
    Camera3D cam{};
    Renderer* r = Engine_GetRenderer();
    if (r)
    {
        ds = r->GetLastStats();
        cam = r->GetActiveCamera3D();
    }

    const double timeSec = Engine_GetTotalTime();
    const uint32_t frameNum = Engine_Script_GetFrameCount();
    const uint32_t gsUsed = Engine_Resource_GetAllocatedGsPages();
    const uint32_t gsBudget = Engine_Resource_GetGsPageBudget();

    // --- Print snapshot ---
    Engine_LogInfo("[PERF] ========== PERFORMANCE SNAPSHOT ==========");
    Engine_LogInfo("[PERF] Time since start  : %.3f s", timeSec);
    Engine_LogInfo("[PERF] Frame number      : %u", frameNum);
    Engine_LogInfo("[PERF] Engine FPS        : %.3f", Engine_GetFPS());

    Engine_LogInfo("[PERF] --- Frame Breakdown (ms) ---");
#ifdef REGION_PAL
    Engine_LogInfo("[PERF] Target Budget     : 20.00 ms (50 FPS)");
#else
    Engine_LogInfo("[PERF] Target Budget     : 16.67 ms (60 FPS)");
#endif

    float logicMs = Engine_GetLogicTime() * 1000.0f;
    float renderMs = Engine_GetRenderTime() * 1000.0f;
    float waitMs = Engine_GetWaitTime() * 1000.0f;
    float totalMs = logicMs + renderMs + waitMs;

    Engine_LogInfo("[PERF] Script Logic      : %5.2f ms", logicMs);
    Engine_LogInfo("[PERF] C++ Render        : %5.2f ms", renderMs);
    Engine_LogInfo("[PERF] GPU Wait (Vsync)  : %5.2f ms", waitMs);
    Engine_LogInfo("[PERF] GS Wait (EndFrame): %5.2f ms", ds.gsWaitMs);
    Engine_LogInfo("[PERF] Total Frame Time  : %5.2f ms", totalMs);

    Engine_LogInfo("[PERF] --- Renderer Throughput (measured) ---");
    Engine_LogInfo("[PERF] Tris submitted    : %u", ds.trisSubmitted);
    Engine_LogInfo("[PERF] Tris culled       : %u", ds.trisCulled);
    Engine_LogInfo("[PERF] Verts transformed : %u", ds.vertsTransformed);
    Engine_LogInfo("[PERF] Texture binds     : %u", ds.texBinds);
    if (ds.packetQwordsUsed > 0)
    {
        Engine_LogInfo("[PERF] GIF packet        : %u / %u qwords (%.1f KB)", ds.packetQwordsUsed,
            static_cast<unsigned>(GFX_GIFTAG_PACKET_QWORDS), (ds.packetQwordsUsed * 16.0f) / 1024.0f);
    }

    Engine_LogInfo("[PERF] --- Draw Lists ---");
    Engine_LogInfo("[PERF] Primitives        : %u / %d", ds.primitiveCount, GFX_MAX_DRAW_LIST_LENGTH);
    Engine_LogInfo("[PERF] Models            : %u / %d", ds.modelCount, GFX_MAX_DRAW_LIST_LENGTH);
    Engine_LogInfo("[PERF] Entries culled    : %u", ds.entriesCulled);

    Engine_LogInfo("[PERF] --- Camera State ---");
    Engine_LogInfo("[PERF] Pos               : (%.2f, %.2f, %.2f)", cam.position.x, cam.position.y, cam.position.z);
    Engine_LogInfo("[PERF] Target            : (%.2f, %.2f, %.2f)", cam.target.x, cam.target.y, cam.target.z);
    Engine_LogInfo("[PERF] FOV               : %.2f", cam.fovy);

    Engine_LogInfo("[PERF] --- GS VRAM (Textures) ---");
    Engine_LogInfo("[PERF] Pages used        : %u / %u (%u%% full)", gsUsed, gsBudget, gsBudget ? (gsUsed * 100u / gsBudget) : 0u);

    Engine_LogInfo("[PERF] --- Memory Management ---");
    size_t arenaTotalUsed = 0;
    size_t arenaTotalCap = 0;

    // 1. System Heap (Authoritative total for all malloc/memalign)
    size_t heapTotal = 0, heapUsed = 0, heapFree = 0;
    Engine_GetHeapStats(&heapTotal, &heapUsed, &heapFree);

    Engine_LogInfo("[PERF] System Heap (malloc) : %zu / %zu KB", heapUsed / 1024, heapTotal / 1024);

    // 2. Arenas (Sub-allocated from Heap)
    const char* arenaNames[] = {"Script", "Config", "LevelData", "Renderer"};
    for (int i = 0; i < (int)ARENA_COUNT; ++i)
    {
        size_t cap = 0, used = 0;
        Engine_GetArenaStats((ArenaType)i, &cap, &used);
        arenaTotalUsed += used;
        arenaTotalCap += cap;
    }

    Engine_LogInfo("[PERF]  +- Engine Arenas    : %zu KB (Reserved Total)", arenaTotalCap / 1024);
    for (int i = 0; i < (int)ARENA_COUNT; ++i)
    {
        size_t cap = 0, used = 0;
        Engine_GetArenaStats((ArenaType)i, &cap, &used);
        Engine_LogInfo("[PERF]  |   - %-10s : %zu / %zu KB (%zu%%)", arenaNames[i], used / 1024, cap / 1024, cap ? (used * 100 / cap) : 0);
    }

    // 3. Main Pool (Sub-allocated from Heap)
    size_t poolCap = 0, poolUsed = 0;
    Engine_GetPoolStatsMain(&poolCap, &poolUsed);
    Engine_LogInfo("[PERF]  +- Main Pool        : %zu / %zu KB (%zu%%)", poolUsed / 1024, poolCap / 1024, poolCap ? (poolUsed * 100 / poolCap) : 0);

    size_t miscUsed = heapUsed - arenaTotalCap - poolCap;
    Engine_LogInfo("[PERF]  +- Misc (Lua/RL/IO) : %zu KB", miscUsed / 1024);

    Engine_LogInfo("[PERF] -------------------------");
    Engine_LogInfo("[PERF] Total EE RAM Used    : %zu / %zu KB (%zu%%)", heapUsed / 1024, heapTotal / 1024, heapTotal ? (heapUsed * 100 / heapTotal) : 0);

    Engine_LogInfo("[PERF] --- Controllers ---");
    for (uint8_t p = 0; p < MAX_GAME_PAD_PORTS; ++p)
    {
        if (IsGamePadInitialized(p))
            Engine_LogInfo("[PERF] Port %u            : CONNECTED", p);
    }

#if defined(PLATFORM_PLAYSTATION2)
    Engine_LogInfo("[PERF] --- System ---");
    Engine_LogInfo("[PERF] Platform         : PlayStation 2 (EE)");
#else
    Engine_LogInfo("[PERF] --- System ---");
    Engine_LogInfo("[PERF] Platform         : PC (development build)");
#endif

    Engine_LogInfo("[PERF] =============================================");
}
