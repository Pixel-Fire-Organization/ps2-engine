#include <cstdio>
#include <cstring>
#include "Engine.h"

#include <delaythread.h>
#include <kernel.h>

#define MAX_IO_REQUESTS IO_ASYNC_MAX_REQUESTS

typedef enum
{
    IO_STATE_IDLE,
    IO_STATE_QUEUED,
    IO_STATE_COMPLETED,
    // Buffer is live on the main thread inside the callback.
    // The IO thread must not reuse this slot until it transitions back to IDLE.
    IO_STATE_DISPATCHING
} IORequestState;

typedef struct
{
    char filepath[IO_FILE_MAX_PATH];
    IO_Callback callback;
    void* userData;
    IORequestState state;
    void* loadedData;
    size_t loadedSize;
} IORequest;

static IORequest s_Requests[MAX_IO_REQUESTS];
static volatile bool s_IOThreadActive = false;

static int s_IOThreadID = -1;
static int s_IOMutex = -1;

// Binary semaphore protecting s_SharedReadBuffer.
// Initialised to 1 (buffer free). The IO thread acquires it before reading a
// file; the main thread releases it once the callback has finished consuming
// the data and the slot is marked IDLE.
static int s_IOBufferSema = -1;

extern void* _gp;

// Stack for the IO thread. Must be statically allocated and 16-byte aligned
// so that the PS2 kernel can map it correctly. Never use a local/heap buffer
// here — the kernel holds a pointer to this for the thread's lifetime.
static uint8_t s_IOThreadStack[IO_THREAD_STACK_SIZE] __attribute__((aligned(16)));

// Single shared read buffer — replaces the previous per-slot array.
// Using one buffer (512 KB) instead of one per slot (16 × 512 KB = 8 MB) keeps
// BSS inside the PS2 EE TLB coverage window. Per-slot buffers pushed .bss to
// virtual address 0x30000000, which has no TLB mapping, causing a store TLB
// miss cascade during the crt0 BSS-zero loop at startup.
// Ownership alternates: IO thread acquires s_IOBufferSema before reading, main
// thread releases it after the callback returns — enforcing serial buffer use.
// 16-byte alignment is required for GS DMA (LoadImageFromMemory, etc.).
static uint8_t s_SharedReadBuffer[IO_READ_BUFFER_SIZE] __attribute__((aligned(16)));

static void IOThreadEntry(void* arg)
{
    UNUSED_VAR(arg);
    while (s_IOThreadActive)
    {
        int reqIndex = -1;
        char filepath[IO_FILE_MAX_PATH];

        if (s_IOMutex >= 0)
        {
            WaitSema(s_IOMutex);
            for (int i = 0; i < MAX_IO_REQUESTS; ++i)
            {
                if (s_Requests[i].state == IO_STATE_QUEUED)
                {
                    reqIndex = i;
                    strncpy(filepath, s_Requests[i].filepath, IO_FILE_MAX_PATH - 1);
                    filepath[IO_FILE_MAX_PATH - 1] = '\0';
                    break;
                }
            }
            SignalSema(s_IOMutex);
        }

        if (reqIndex != -1)
        {
            // Acquire the shared buffer before reading.
            // Blocks until Engine_IO_Update has finished dispatching the previous
            // callback — i.e. the callback has returned and s_SharedReadBuffer is
            // no longer referenced by the main thread.
            if (s_IOBufferSema >= 0)
                WaitSema(s_IOBufferSema);

            // Engine_IO_Shutdown signals the sema to unblock a waiting IO thread.
            // Exit immediately if that is why we woke up.
            if (!s_IOThreadActive)
                break;

            FILE* f = fopen(filepath, "rb");
            void* data = nullptr;
            size_t size = 0;
            if (f)
            {
                fseek(f, 0, SEEK_END);
                const long sizeL = ftell(f);
                if (sizeL < 0)
                {
                    Engine_LogError("IO: File seek failed. File: %s", filepath);
                    fclose(f);
                    // Treat as per-request failure: data=NULL, size=0.
                    // Continue to dispatch path below so callback gets notified.
                }
                else
                {
                    size = static_cast<size_t>(sizeL);
                    fseek(f, 0, SEEK_SET);

                    if (size > IO_READ_BUFFER_SIZE)
                    {
                        // File exceeds the static buffer cap. Reject rather than truncate
                        // — a truncated asset would silently corrupt the decoded resource.
                        Engine_LogError("IO: '%s' is %zu bytes, exceeds IO_READ_BUFFER_SIZE (%d). Rejected.", filepath,
                                        size, IO_READ_BUFFER_SIZE);
                        fclose(f);
                        size = 0;
                        // data stays NULL; callback receives (NULL, 0, userData).
                    }
                    else
                    {
                        const size_t bytesRead = fread(s_SharedReadBuffer, 1, size, f);
                        fclose(f);
                        if (bytesRead != size)
                        {
                            Engine_LogError("IO: Short read for '%s': expected %zu, got %zu", filepath, size,
                                            bytesRead);
                            size = 0;
                            // data stays NULL; downstream decoders would see truncated/garbage data.
                        }
                        else
                        {
                            data = s_SharedReadBuffer;
                        }
                    }
                }
            }
            else
            {
                Engine_LogError("Failed to open %s", filepath);
            }

            if (s_IOMutex >= 0)
            {
                WaitSema(s_IOMutex);
                s_Requests[reqIndex].loadedData = data;
                s_Requests[reqIndex].loadedSize = size;
                s_Requests[reqIndex].state = IO_STATE_COMPLETED;
                SignalSema(s_IOMutex);
            }
        }
        else
        {
            DelayThread(IO_THREAD_SLEEP_USEC);
        }
    }
}

bool Engine_IO_Init()
{
    memset(s_Requests, 0, sizeof(s_Requests));
    s_IOThreadActive = true;

    ee_sema_t sema;
    sema.init_count = 1;
    sema.max_count = 1;
    sema.option = 0;
    s_IOMutex = CreateSema(&sema);

    if (s_IOMutex < 0)
    {
        Engine_LogError("Failed to create IO semaphore! Error: %d", s_IOMutex);
        return false;
    }

    // Buffer semaphore: 1 = s_SharedReadBuffer is free for the IO thread.
    // Acquiring it before a read and releasing it after the callback guarantees
    // the IO thread never overwrites the buffer while the main thread is inside
    // the callback using the pointer.
    ee_sema_t bufSema;
    bufSema.init_count = 1;
    bufSema.max_count = 1;
    bufSema.option = 0;
    s_IOBufferSema = CreateSema(&bufSema);
    if (s_IOBufferSema < 0)
    {
        Engine_LogError("Failed to create IO buffer semaphore! Error: %d", s_IOBufferSema);
        DeleteSema(s_IOMutex);
        s_IOMutex = -1;
        return false;
    }

    // Zero-initialise the entire struct first so that the 'attr', 'option',
    // 'status', and 'current_priority' fields never contain stack garbage.
    // PS2 kernel behaviour on CreateThread is undefined for non-zero 'attr'
    // bits that do not correspond to recognised flags; a garbage value here
    // changes every time the call-stack above changes (e.g. when EngineScript
    // is modified) and can corrupt the EE kernel's thread table, which then
    // manifests as a crash inside an unrelated ISR (typically libpad's DMA
    // handler in the pad polling interrupt).
    ee_thread_t threadParam = {};
    threadParam.func = reinterpret_cast<void*>(IOThreadEntry);
    threadParam.stack = s_IOThreadStack;
    threadParam.stack_size = IO_THREAD_STACK_SIZE;
    threadParam.gp_reg = &_gp;
    threadParam.initial_priority = 0x40;

    s_IOThreadID = CreateThread(&threadParam);
    if (s_IOThreadID < 0)
    {
        Engine_LogError("Failed to create IO thread! Error: %d", s_IOThreadID);
        return false;
    }

    StartThread(s_IOThreadID, nullptr);

    Engine_LogInfo("Async IO system initialized.");
    return true;
}

bool Engine_IO_ReadAsync(const char* filepath, IO_Callback callback, void* userData)
{
    if (s_IOMutex < 0)
        return false;

    bool queued = false;
    WaitSema(s_IOMutex);

    for (int i = 0; i < MAX_IO_REQUESTS; ++i)
    {
        if (s_Requests[i].state == IO_STATE_IDLE)
        {
            strncpy(s_Requests[i].filepath, filepath, IO_FILE_MAX_PATH - 1);
            s_Requests[i].filepath[IO_FILE_MAX_PATH - 1] = '\0';
            s_Requests[i].callback = callback;
            s_Requests[i].userData = userData;
            s_Requests[i].state = IO_STATE_QUEUED;
            queued = true;
            break;
        }
    }

    SignalSema(s_IOMutex);

    if (!queued)
    {
        Engine_LogError("Failed to queue IO request for %s. Queue full.", filepath);
    }
    return queued;
}

void Engine_IO_Update()
{
    if (s_IOMutex < 0)
        return;

    WaitSema(s_IOMutex);

    for (int i = 0; i < MAX_IO_REQUESTS; ++i)
    {
        if (s_Requests[i].state == IO_STATE_COMPLETED)
        {
            // Copy callback data to locals before releasing the mutex.
            // The callback (e.g. Internal_OnAsyncLoadComplete) may itself call
            // Engine_IO_ReadAsync for dependency loads, which acquires s_IOMutex —
            // invoking it while the lock is held would deadlock.
            IO_Callback cb = s_Requests[i].callback;
            void* data = s_Requests[i].loadedData;
            size_t size = s_Requests[i].loadedSize;
            void* userData = s_Requests[i].userData;

            // Transition to DISPATCHING before releasing the mutex. The IO thread
            // only picks up QUEUED slots, so this keeps s_SharedReadBuffer protected
            // for the full duration of the callback — preventing a requeue of slot i
            // from racing with the callback's use of the buffer pointer.
            s_Requests[i].state = IO_STATE_DISPATCHING;

            SignalSema(s_IOMutex);

            if (cb)
            {
                cb(data, size, userData);
            }
            // data points into s_SharedReadBuffer (or NULL on error) — no free() needed.

            // Re-acquire to mark the slot idle and continue the scan.
            WaitSema(s_IOMutex);
            s_Requests[i].loadedData = nullptr;
            s_Requests[i].state = IO_STATE_IDLE;

            // Release the shared read buffer AFTER setting the slot to IDLE, so
            // that any new request the callback may have queued during DISPATCHING
            // is already visible to the IO thread when it next scans.
            if (s_IOBufferSema >= 0)
                SignalSema(s_IOBufferSema);
        }
    }

    SignalSema(s_IOMutex);
}

void Engine_IO_Shutdown()
{
    s_IOThreadActive = false;
    // Wake the IO thread if it is blocked on WaitSema(s_IOBufferSema).
    // Without this signal the thread would stall indefinitely after shutdown.
    if (s_IOBufferSema >= 0)
        SignalSema(s_IOBufferSema);
    // Note: Full thread cleanup (DeleteThread / DeleteSema) requires waiting
    // for the thread to exit — deferred until proper join support is added.
}
