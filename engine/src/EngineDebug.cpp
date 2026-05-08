#include <raylib.h>
#include <cstdarg>
#include <cstdio>
#include <malloc.h>
#include "Engine.h"
#include "EngineInput.h"
#include "graphics/DrawList.h"

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


static void CustomLog(int logLevel, const char* text, va_list args)
{
    char buffer[512];
    const char* prefix = "INFO: ";

    if (logLevel == LOG_WARNING) prefix = "WARN: ";
    else if (logLevel == LOG_ERROR)   prefix = "ERR : ";
    else if (logLevel == LOG_DEBUG)   prefix = "DBG : ";

    int offset = snprintf(buffer, sizeof(buffer), "%s", prefix);
    if (offset < (int)sizeof(buffer))
    {
        vsnprintf(buffer + offset, sizeof(buffer) - offset, text, args);
    }
    buffer[sizeof(buffer) - 1] = '\0';

    // Sanitize: replace non-ASCII / non-printable characters to prevent console hangs
    for (int i = 0; buffer[i] != '\0'; i++)
    {
        unsigned char c = static_cast<unsigned char>(buffer[i]);
        if (c < 32 || c > 126) 
        {
            buffer[i] = '?';
        }
    }

    printf("%s\n", buffer);
}

void Engine_InitDebug()
{
    SetTraceLogCallback(CustomLog);
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
    DrawFPS(10, 10);
    DrawText("System: PS2 Raylib Engine", 10, 30, 20, GREEN);
    Engine_DrawAsciiTable();
}

void Engine_DrawAsciiTable() { DrawText(ASCII_TABLE_STR, 10, 60, 20, RED); }

void Engine_Panic(const char* message)
{
#ifdef DEBUG
    Engine_LogError("!!! PS2 PANIC !!! %s", message);

    if (!Engine_Is_GFX_Initialized())
    {
        // We should init a minimal environment here.
        // This can happen if the memory map is invalid, or CORE functionality
        // failed for some reason.
        InitWindow(GFX_SCREEN_WIDTH, GFX_SCREEN_HEIGHT, "PS2 Engine");
    }

    while (!IsWindowReady())
        ;

    // Because Raylib requires a window to draw, and we will never close it, nor
    // will the user close it, the loop will be indefinite.
    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RED);
        DrawText("PS2 ENGINE PANIC", PANIC_UI_PADDING, PANIC_UI_PADDING, PANIC_UI_FONT_SIZE_TITLE, WHITE);
        DrawText("UNRECOVERABLE ERROR:", PANIC_UI_PADDING, 100, PANIC_UI_FONT_SIZE_SUBTITLE, YELLOW);
        DrawText(message, PANIC_UI_PADDING, 140, PANIC_UI_FONT_SIZE_BODY, WHITE);
        DrawText("HALTING EMOTION ENGINE...", PANIC_UI_PADDING, 200, PANIC_UI_FONT_SIZE_FOOTER, LIGHTGRAY);
        EndDrawing();

        if (IsGamePadButtonPressed(0, GamePadButton::Cross))
        {
            // Reset PS2
        }
    }
#else
    // Since this variable will never be used in release,
    // we declare it as unused, so the compiler doesn't complain.
    UNUSED_VAR(message);
    while (1)
        ;

#endif
}

// ---------------------------------------------------------------------------
// Performance Logger
// ---------------------------------------------------------------------------

static bool  s_PerfLoggerEnabled  = false;
static bool  s_ComboWasHeld       = false;   // debounce — fire once per hold

void Engine_PerfLogger_Init(bool enabled)
{
    s_PerfLoggerEnabled = enabled;
    s_ComboWasHeld      = false;
    if (enabled)
        Engine_LogInfo("[PerfLogger] Initialized. Hold L1+L2+R1+R2 to dump snapshot.");
}

void Engine_PerfLogger_Tick()
{
    if (!s_PerfLoggerEnabled)
        return;

    // Combo: all four shoulder buttons simultaneously on port 0.
    const bool comboNow =
        IsGamePadButtonPressed(0, GamePadButton::L1) &&
        IsGamePadButtonPressed(0, GamePadButton::L2) &&
        IsGamePadButtonPressed(0, GamePadButton::R1) &&
        IsGamePadButtonPressed(0, GamePadButton::R2);

    if (!comboNow)
    {
        s_ComboWasHeld = false;
        return;
    }

    // Fire once per hold (rising edge).
    if (s_ComboWasHeld)
        return;
    s_ComboWasHeld = true;

    // --- Gather stats ---
    DrawStats ds{};
    Renderer* r = Engine_GetRenderer();
    if (r)
        ds = r->GetLastStats();

    const double   timeSec    = GetTime();
    const uint32_t frameNum   = Engine_Script_GetFrameCount();
    const uint32_t gsUsed     = Engine_Resource_GetAllocatedGsPages();
    const uint32_t gsBudget   = Engine_Resource_GetGsPageBudget();

    // Heap info via mallinfo (available in newlib / glibc)
    struct mallinfo mi = mallinfo();
    // uordblks  = total allocated bytes; fordblks = total free bytes in arena
    const uint32_t heapFreeKB = static_cast<uint32_t>(mi.fordblks) / 1024u;
    const uint32_t heapUsedKB = static_cast<uint32_t>(mi.uordblks) / 1024u;

    // --- Print snapshot ---
    Engine_LogInfo("[PERF] ========== PERFORMANCE SNAPSHOT ==========");
    Engine_LogInfo("[PERF] Time since start  : %.3f s", timeSec);
    Engine_LogInfo("[PERF] Frame number      : %u",     frameNum);
    Engine_LogInfo("[PERF] --- Draw Lists ---");
    Engine_LogInfo("[PERF] Primitives        : %u",     ds.primitiveCount);
    Engine_LogInfo("[PERF] Models            : %u",     ds.modelCount);
    Engine_LogInfo("[PERF] Texture batches   : %u",     ds.uniqueTextures);
    Engine_LogInfo("[PERF] --- GS VRAM ---");
    Engine_LogInfo("[PERF] Pages used        : %u / %u (%u%% full)",
                   gsUsed, gsBudget,
                   gsBudget ? (gsUsed * 100u / gsBudget) : 0u);
    Engine_LogInfo("[PERF] --- Heap Memory ---");
    Engine_LogInfo("[PERF] Used              : %u KB", heapUsedKB);
    Engine_LogInfo("[PERF] Free (arena)      : %u KB", heapFreeKB);
    Engine_LogInfo("[PERF] --- Controllers ---");
    for (uint8_t p = 0; p < MAX_GAME_PAD_PORTS; ++p)
    {
        Engine_LogInfo("[PERF] Port %u            : %s", p,
                       IsGamePadInitialized(p) ? "CONNECTED" : "NOT INITIALIZED");
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

